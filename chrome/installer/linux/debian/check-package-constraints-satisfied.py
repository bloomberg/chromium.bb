#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Given a binary, uses dpkg-shlibdeps to check that its package dependencies
are satisfiable on all supported debian-based distros.
"""

import json
import os
import re
import subprocess
import sys

import deb_version
import package_version_interval

if len(sys.argv) != 5:
  print 'Usage: %s binary_path sysroot_path arch stamp_path' % sys.argv[0]
  sys.exit(1)
binary = os.path.abspath(sys.argv[1])
sysroot = os.path.abspath(sys.argv[2])
arch = sys.argv[3]
stamp = sys.argv[4]

cmd = ['dpkg-shlibdeps']
if arch == 'x64':
  cmd.extend(['-l%s/usr/lib/x86_64-linux-gnu' % sysroot,
              '-l%s/lib/x86_64-linux-gnu' % sysroot])
elif arch == 'x86':
  cmd.extend(['-l%s/usr/lib/i386-linux-gnu' % sysroot,
              '-l%s/lib/i386-linux-gnu' % sysroot])
elif arch == 'arm':
  cmd.extend(['-l%s/usr/lib/arm-linux-gnueabihf' % sysroot,
              '-l%s/lib/arm-linux-gnueabihf' % sysroot])
else:
  print 'Unsupported architecture ' + arch
  sys.exit(1)
cmd.extend(['-l%s/usr/lib' % sysroot, '-O', '-e', binary])

proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                        cwd=sysroot)
exit_code = proc.wait()
(stdout, stderr) = proc.communicate()
if exit_code != 0:
  print 'dpkg-shlibdeps failed with exit code ' + str(exit_code)
  print 'stderr was ' + stderr
  sys.exit(1)

deps_str = stdout.replace('shlibs:Depends=', '').replace('\n', '')
deps = deps_str.split(', ')
package_intervals = {}
if deps_str != '':
  for dep in deps:
    (package, interval) = package_version_interval.parse_dep(dep)
    package_intervals[package] = interval

script_dir = os.path.dirname(os.path.abspath(__file__))
deps_file = os.path.join(script_dir, 'dist-package-versions.json')
distro_package_versions = json.load(open(deps_file))

ret_code = 0
for distro in distro_package_versions:
  for package in package_intervals:
    if package not in distro_package_versions[distro]:
      print >> sys.stderr, (
          'Unexpected new dependency %s on distro %s caused by binary %s' % (
              package, distro, os.path.basename(binary)))
      ret_code = 1
      continue
    distro_version = deb_version.DebVersion(
        distro_package_versions[distro][package])
    if not package_intervals[package].contains(distro_version):
      print >> sys.stderr, 'Dependency on package %s not satisfiable on %s' % (
          package, distro)
      ret_code = 1
if ret_code == 0:
  with open(stamp, 'a'):
    os.utime(stamp, None)
exit(ret_code)
