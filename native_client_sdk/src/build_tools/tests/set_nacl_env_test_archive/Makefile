# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

EXAMPLE_DIR = .
EXAMPLE = hello_world_c.nexe

all : $(EXAMPLE)

clean :
	rm -f $(EXAMPLE) hello_world_c.o hello_world_c.nexe

SRCS_ = $(EXAMPLE_DIR)/*.c

hello_world_c.o : $(SRCS_)
	$(CXX) $(CPPFLAGS) -I$(EXAMPLE_DIR) $(CXXFLAGS) -c \
            $(EXAMPLE_DIR)/hello_world_c.c

$(EXAMPLE) : hello_world_c.o
	$(LINK) $(CFLAGS) $(LDFLAGS) $^ -o $@
