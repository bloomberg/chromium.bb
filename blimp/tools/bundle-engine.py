#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Bundles the Blimp Engine and its runtime dependencies into a tarball.

   The created bundle can be passed as input to docker build.  E.g.
   docker build - < ../../out-linux/Debug/blimp_engine_deps.tar
'''


import argparse
import errno
import os
import subprocess
import tarfile

def GetDependencies(build_dir, target):
  """Return the list of target's dependencies.
  :raises CalledProcessError: if the dependency list could not be determined.
  """
  # Get the list of runtime dependencies from the already built target.
  deps = subprocess.check_output(['gn', 'desc', build_dir, target,
                                  'runtime_deps']).split('\n')
  # Remove any empty strings.
  deps = filter(bool, deps)
  # Exclude any deps aren't under the build_dir. They are not needed.
  return [dep for dep in deps if not dep.startswith('..')]

def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--build-dir',
                      help=('build output directory (e.g. out/Debug)'),
                      required=True,
                      metavar='DIR')
  parser.add_argument('--dockerfile',
                      help=('Dockerfile to add to the bundle'),
                      required=True,
                      metavar='FILE')
  parser.add_argument('--target',
                      help=('build target of engine'),
                      required=True)
  parser.add_argument('--output',
                      help=('name and path of bundle to create'),
                      required=True,
                      metavar='FILE')
  args = parser.parse_args()

  # Get the list of runtime dependencies from the already built target.
  try:
    deps = GetDependencies(args.build_dir, args.target)
  except subprocess.CalledProcessError as e:
    print "Error: " + ' '.join(e.cmd)
    print e.output
    exit(1)

  # Add the deps to the tarball along with the Dockerfile.
  with tarfile.open(args.output, 'w') as tarball:
    tarball.add(args.dockerfile, arcname='Dockerfile')
    os.chdir(args.build_dir)
    for dep in deps:
      try:
        tarball.add(dep)
      except OSError as e:
        if e.errno == errno.ENOENT:
          print dep + " not found (did you build " + args.target + "?)"
          exit(1)
  print 'Created ' + args.output

if __name__ == "__main__":
  main()
