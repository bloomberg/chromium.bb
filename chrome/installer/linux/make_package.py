# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage:
#   make_package.py <shell_script> [<args_to_script>*]

import subprocess
import sys

if len(sys.argv) < 2:
  print "Incorrect args."
  sys.exit(1)
sys.exit(subprocess.call(["bash"] + sys.argv[2:]))
