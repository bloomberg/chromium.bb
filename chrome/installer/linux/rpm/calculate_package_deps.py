#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Given a binary, uses find-requires to check that its package dependencies
are satisfiable on all supported rpm-based distros.
"""

import json
import os
import subprocess
import sys

if len(sys.argv) < 3:
  print 'Usage: %s binary_path dep_file [shlib1 shlib2 ...]' % sys.argv[0]
  sys.exit(1)

binary = os.path.abspath(sys.argv[1])
dep_filename = sys.argv[2]
bundled_shlibs = [os.path.basename(file) for file in sys.argv[3:]]

proc = subprocess.Popen(['/usr/lib/rpm/find-requires'], stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
(stdout, stderr) = proc.communicate(binary + '\n')
exit_code = proc.wait()
if exit_code != 0:
  print 'find-requires failed with exit code ' + str(exit_code)
  print 'stderr was ' + stderr
  sys.exit(1)

requires = set([] if stdout == '' else stdout.rstrip('\n').split('\n'))

script_dir = os.path.dirname(os.path.abspath(__file__))
provides_file = open(os.path.join(script_dir, 'dist-package-provides.json'))
distro_package_provides = json.load(provides_file)

remove_requires = set()
ret_code = 0
for distro in distro_package_provides:
  for requirement in requires:
    if any([requirement.startswith(shlib) for shlib in bundled_shlibs]):
      remove_requires.add(requirement)
      continue
    if requirement not in distro_package_provides[distro]:
      print >> sys.stderr, (
          'Unexpected new dependency %s on distro %s caused by binary %s' % (
              requirement, distro, os.path.basename(binary)))
      ret_code = 1
      continue
if ret_code == 0:
  requires = requires.difference(remove_requires)
  with open(dep_filename, 'w') as dep_file:
    for requirement in sorted(list(requires)):
      dep_file.write(requirement + '\n')
sys.exit(ret_code)
