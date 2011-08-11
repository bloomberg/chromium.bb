#!/bin/sh

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a small wrapper script around make_heap_non_executable.py allowing
# it to be invoked easily from Xcode. make_heap_non_executable.py expects its
# argument on the command line, and Xcode puts all of its parameters in the
# environment.

set -e

exec "$(dirname "${0}")/make_heap_non_executable.py" \
     "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}"
