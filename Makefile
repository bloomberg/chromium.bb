# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS = libdrm
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -std=c99 -D_GNU_SOURCE=1
CFLAGS += -Wall -Wsign-compare -Wpointer-arith -Wcast-qual -Wcast-align

ifdef GBM_EXYNOS
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_exynos)
endif
ifdef GBM_I915
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_intel)
endif
ifdef GBM_ROCKCHIP
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_rockchip)
endif

CPPFLAGS += $(PC_CFLAGS)
LDLIBS += $(PC_LIBS)

LIBDIR ?= /usr/lib/

CC_LIBRARY(libgbm.so): $(C_OBJECTS)

all: CC_LIBRARY(libgbm.so)

clean: CLEAN(libgbm.so)

install: all
	mkdir -p $(DESTDIR)/$(LIBDIR)
	install -D -m 755 $(OUT)/libgbm.so $(DESTDIR)/$(LIBDIR)
	install -D -m 0644 $(SRC)/libgbm.pc $(DESTDIR)$(LIBDIR)/pkgconfig/libgbm.pc
	install -D -m 0644 $(SRC)/gbm.h $(DESTDIR)/usr/include/gbm.h
