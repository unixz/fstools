/*
 * Copyright (C) 2014 John Crispin <blogic@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libubox/ulog.h>

#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libfstools/libfstools.h"
#include "libfstools/volume.h"

static int jffs2_mark(struct volume *v);

static int
ask_user(int argc, char **argv)
{
	if ((argc < 2) || strcmp(argv[1], "-y")) {
		ULOG_WARN("This will erase all settings and remove any installed packages. Are you sure? [N/y]\n");
		if (getchar() != 'y')
			return -1;
	}
	return 0;

}

static int jffs2_reset(struct volume *v)
{
	char *mp;

	mp = find_mount_point(v->blk, 1);
	if (mp) {
		ULOG_INFO("%s is mounted as %s, only erasing files\n", v->blk, mp);
		fs_state_set("/overlay", FS_STATE_PENDING);
		overlay_delete(mp, false);
		mount(mp, "/", NULL, MS_REMOUNT, 0);
	} else {
		ULOG_INFO("%s is not mounted\n", v->blk);
		return jffs2_mark(v);
	}

	return 0;
}

static int jffs2_mark(struct volume *v)
{
	__u32 deadc0de = __cpu_to_be32(0xdeadc0de);
	size_t sz;
	int fd;

	fd = open(v->blk, O_WRONLY);
	ULOG_INFO("%s will be erased on next mount\n", v->blk);
	if (!fd) {
		ULOG_ERR("opening %s failed\n", v->blk);
		return -1;
	}

	sz = write(fd, &deadc0de, sizeof(deadc0de));
	close(fd);

	if (sz != 4) {
		ULOG_ERR("writing %s failed: %s\n", v->blk, strerror(errno));
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct volume *v;

	if (ask_user(argc, argv))
		return -1;

	/*
	 * TODO: Currently this only checks if kernel supports OverlayFS. We
	 * should check if there is a mount point using it with rootfs_data
	 * as upperdir.
	 */
	if (find_filesystem("overlay")) {
		ULOG_ERR("overlayfs not supported by kernel\n");
		return -1;
	}

	v = volume_find("rootfs_data");
	if (!v) {
		ULOG_ERR("MTD partition 'rootfs_data' not found\n");
		return -1;
	}

	if (!strcmp(*argv, "jffs2mark"))
		return jffs2_mark(v);
	return jffs2_reset(v);
}
