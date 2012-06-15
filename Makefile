# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

LIBDIR ?= /usr/lib

all: CC_LIBRARY(src/libevdev.so.0)
clean: CLEAN(src/libevdev.so.0)
install: install-lib install-header