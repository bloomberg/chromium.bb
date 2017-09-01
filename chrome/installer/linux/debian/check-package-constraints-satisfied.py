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

if len(sys.argv) != 4:
  print 'Usage: %s /path/to/binary /path/to/sysroot arch' % sys.argv[0]
  sys.exit(1)

binary = os.path.abspath(sys.argv[1])
sysroot = os.path.abspath(sys.argv[2])
arch = sys.argv[3]

cmd = ['dpkg-shlibdeps']
if arch == 'amd64':
  cmd.extend(['-l%s/usr/lib/x86_64-linux-gnu' % sysroot,
              '-l%s/lib/x86_64-linux-gnu' % sysroot])
elif arch == 'i386':
  cmd.extend(['-l%s/usr/lib/i386-linux-gnu' % sysroot,
              '-l%s/lib/i386-linux-gnu' % sysroot])
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

def version_statement_satisfied(left_version, op, right_version):
  # Allowed relationship operators are specified in:
  # https://www.debian.org/doc/debian-policy/ch-relationships.html
  if op == '>=':
    return left_version >= right_version
  if op == '<=':
    return left_version <= right_version
  if op == '>>':
    return left_version > right_version
  if op == '<<':
    return left_version < right_version
  assert op == '='
  return left_version == right_version

def get_package_and_version_requirement(dep):
  package_name_regex = '[a-z][a-z0-9\+\-\.]+'
  match = re.match('^(%s)$' % package_name_regex, dep)
  if match:
    return (match.group(1), lambda version: True)
  match = re.match('^(%s) \(([\>\=\<]+) ([\~0-9A-Za-z\+\-\.\:]+)\)$' %
                   package_name_regex, dep)
  if match:
    return (match.group(1), lambda version: version_statement_satisfied(
        version, match.group(2), deb_version.DebVersion(match.group(3))))
  # At the time of writing this script, Chrome does not have any
  # complex version requirements like 'version >> 3 | version << 2'.
  print ('Conjunctions and disjunctions in package version requirements are ' +
         'not implemented at this time.')
  sys.exit(1)

deps = stdout.replace('shlibs:Depends=', '').replace('\n', '').split(', ')
package_requirements = {}
for dep in deps:
  (package, requirement) = get_package_and_version_requirement(dep)
  package_requirements[package] = requirement

script_dir = os.path.dirname(os.path.abspath(__file__))
deps_file = os.path.join(script_dir, 'dist-package-versions.json')
distro_package_versions = json.load(open(deps_file))

ret_code = 0
for distro in distro_package_versions:
  for package in package_requirements:
    if package not in distro_package_versions[distro]:
      print >> sys.stderr, (
          'Unexpected new dependency %s on distro %s caused by binary %s' % (
              package, distro, os.path.basename(binary)))
      ret_code = 1
      continue
    distro_version = deb_version.DebVersion(
        distro_package_versions[distro][package])
    if not package_requirements[package](distro_version):
      print >> sys.stderr, 'Dependency on package %s not satisfiable on %s' % (
          package, distro)
      ret_code = 1
exit(ret_code)
