# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
GCC = gcc

OBJDIR = cobj

OBJECTS=\
	$(OBJDIR)/libevdev.o \
	$(OBJDIR)/libevdev_mt.o \
	$(OBJDIR)/libevdev_event.o

SONAME=$(OBJDIR)/libevdev.so.0

DEPDIR = .deps

DESTDIR = .

CFLAGS=\
	-g \
	-O2 \
	-fno-exceptions \
	-fno-strict-aliasing \
	-fPIC \
	-Wall \
	-Wclobbered \
	-Wempty-body \
	-Werror \
	-Wignored-qualifiers \
	-Wmissing-field-initializers \
	-Wtype-limits \
	-Wuninitialized \
	-D_FILE_OFFSET_BITS=64 \
	-DGESTURES_INTERNAL=1 \
	-Iinclude

all: $(SONAME)

$(SONAME): $(OBJECTS)
	$(GCC) -shared -o $@ $(OBJECTS) -Wl,-h$(SONAME:$(OBJDIR)/%=%) \
		$(LINK_FLAGS)

$(OBJDIR)/%.o : src/%.c
	mkdir -p $(OBJDIR) $(DEPDIR) || true
	$(GCC) -std=gnu99 $(CFLAGS) -MD -c -o $@ $<
	@mv $(@:$.o=$.d) $(DEPDIR)

LIBDIR = /usr/lib

install: $(SONAME)
	install -D -m 0755 $(SONAME) \
		$(DESTDIR)$(LIBDIR)/$(SONAME:$(OBJDIR)/%=%)
	ln -s $(SONAME:$(OBJDIR)/%=%) \
		$(DESTDIR)$(LIBDIR)/$(SONAME:$(OBJDIR)/%.0=%)
	install -D -m 0644 \
		include/libevdev.h $(DESTDIR)/usr/include/libevdev/libevdev.h
		include/libevdev_event.h $(DESTDIR)/usr/include/libevdev/libevdev_event.h
		include/libevdev_mt.h $(DESTDIR)/usr/include/libevdev/libevdev_mt.h

clean:
	rm -rf $(OBJDIR) $(DEPDIR) html app.info app.info.orig

.PHONY : clean all

-include $(OBJECTS:$(OBJDIR)/%.o=$(DEPDIR)/%.d)
