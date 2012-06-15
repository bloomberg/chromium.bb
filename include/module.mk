# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

install-header: CC_LIBRARY(src/libevdev.so.0)
	install -D -m 0644 include/libevdev/libevdev.h \
		 $(DESTDIR)/usr/include/libevdev/libevdev.h
	install -D -m 0664 include/libevdev/libevdev_event.h \
		 $(DESTDIR)/usr/include/libevdev/libevdev_event.h
	install -D -m 0664 include/libevdev/libevdev_mt.h \
		 $(DESTDIR)/usr/include/libevdev/libevdev_mt.h
	install -D -m 0664 include/libevdev/libevdev_log.h \
		 $(DESTDIR)/usr/include/libevdev/libevdev_log.h