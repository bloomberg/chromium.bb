# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script returns the default goma directory for Posix systems, which is
# relative to the user's home directory. On Windows, goma.gypi hardcodes a
# value.

import os

print '"' + os.environ['HOME'] + '/goma"'
