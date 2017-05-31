# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script (intended to be invoked by autoninja or autoninja.bat) detects
whether a build is using goma. If so it runs with a large -j value, and
otherwise it chooses a small one. This auto-adjustment makes using goma simpler
and safer, and avoids errors that can cause slow goma builds or swap-storms
on non-goma builds.
"""

import multiprocessing
import os
import re
import sys

j_specified = False
output_dir = '.'
for index, arg in enumerate(sys.argv[1:]):
  if arg == '-j':
    j_specified = True
  if arg == '-C':
    # + 1 to get the next argument and +1 because we trimmed off sys.argv[0]
    output_dir = sys.argv[index + 2]

use_goma = False
try:
  with open(os.path.join(output_dir, 'args.gn')) as file_handle:
    for line in file_handle:
      # This regex pattern copied from create_installer_archive.py
      m = re.match('^\s*use_goma\s*=\s*true(\s*$|\s*#.*$)', line)
      if m:
        use_goma = True
except IOError:
  pass

if sys.platform.startswith('win'):
  # Specify ninja.exe on Windows so that ninja.bat can call autoninja and not
  # be called back.
  args = ['ninja.exe'] + sys.argv[1:]
else:
  args = ['ninja'] + sys.argv[1:]

num_cores = multiprocessing.cpu_count()
if not j_specified:
  if use_goma:
    args.append('-j')
    core_multiplier = int(os.environ.get("NINJA_CORE_MULTIPLIER", "20"))
    args.append('%d' % (num_cores * core_multiplier))
  else:
    core_addition = os.environ.get("NINJA_CORE_ADDITION")
    if core_addition:
      core_addition = int(core_addition)
      args.append('-j')
      args.append('%d' % (num_cores + core_addition))

# Specify a maximum CPU load so that running builds in two different command
# prompts won't overload the system too much. This is not reliable enough to
# be used to auto-adjust between goma/non-goma loads, but it is a nice
# fallback load balancer.
args.append('-l')
args.append('%d' % num_cores)

print ' '.join(args)
