# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Linux package generation is serialized because it creates large numbers of
# temporary files that can overload the /tmp partition on the builders.
# See https://codereview.chromium.org/243019
#
# This script does a flock to serialize, and then runs the given shell
# script with the given parameters.
#
# Usage:
#   flock_make_package.py <lockfile> <shell_script> [<args_to_script>*]

import subprocess
import sys

if len(sys.argv) < 3:
  print "Incorrect args."
  sys.exit(1)
sys.exit(subprocess.call(["flock", "--", sys.argv[1], "bash"] + sys.argv[2:]))
