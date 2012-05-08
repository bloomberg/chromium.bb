# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#

# =SNIP=
# This Makefile is used as a template to generate the real Makefile that is
# included in the SDK distribution. The toolchain-specific project lists can be
# found in build_tools/build_sdk.py.
$(error MISSING PROJECTS)
# =SNIP=

# Define the default target
all:

#
# Target Macro
#
# Macro defines a phony target for each example, and adds it to a list of
# targets.
#
define TARGET
TARGET_LIST+=$(1)_TARGET
.PHONY: $(1)_TARGET
$(1)_TARGET:
	+$(MAKE) -C $(1)
endef


# Define the various targets via the Macro
$(foreach proj,$(PROJECTS),$(eval $(call TARGET,$(proj))))


all: $(TARGET_LIST)
	echo "Done building targets."

RUN: all
	echo "Staring up python webserver."
	python httpd.py

