#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Bundles the Blimp Engine and its runtime dependencies into a tarball.

   The created bundle can be passed as input to docker build.  E.g.
   docker build - < ../../out-linux/Debug/blimp_engine_deps.tar.gz
'''


import argparse
import errno
import os
import subprocess
import sys

def ReadDependencies(manifest):
  """Read the manifest and return the list of dependencies.
  :raises IOError: if the manifest could not be read.
  """
  deps = []
  with open(manifest) as f:
    for line in f.readlines():
      # Strip comments.
      dep = line.partition('#')[0].strip()
      # Ignore empty strings.
      if dep:
        deps.append(dep)
  return deps

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
  parser.add_argument('--startup-script',
                      help=('Engine startup script to add to the bundle'),
                      required=True,
                      metavar='FILE')
  parser.add_argument('--manifest',
                      help=('engine manifest'),
                      required=True)
  parser.add_argument('--output',
                      help=('name and path of bundle to create'),
                      required=True,
                      metavar='FILE')
  args = parser.parse_args()

  deps = ReadDependencies(args.manifest)

  dockerfile_dirname, dockerfile_basename = os.path.split(args.dockerfile)
  startup_script_dirname, startup_script_basename = os.path.split(
      args.startup_script)

  try:
    env = os.environ.copy()
    # Use fastest possible mode when gzipping.
    env["GZIP"] = "-1"
    subprocess.check_output(
        ["tar",
         "-zcf", args.output,
         "-C", dockerfile_dirname, dockerfile_basename,
         "-C", startup_script_dirname, startup_script_basename,
         "-C", args.build_dir] + deps,
        # Redirect stderr to stdout, so that its output is captured.
        stderr=subprocess.STDOUT,
        env=env)
  except subprocess.CalledProcessError as e:
    print >> sys.stderr, "Failed to create tarball:"
    print >> sys.stderr, e.output
    sys.exit(1)

if __name__ == "__main__":
  main()
