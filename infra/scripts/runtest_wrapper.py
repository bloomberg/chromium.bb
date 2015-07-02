#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for runtest.py, makes it possible to control src-side
which file gets used and test the changes on trybots before landing."""


import argparse
import os
import subprocess
import sys


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--path-build', required=True, help='Path to the build repo')
  parser.add_argument('args', nargs='*', help='Arguments to pass to runtest.py')
  args = parser.parse_args(argv)
  return subprocess.call([
      sys.executable,
      os.path.join(args.path_build, 'scripts', 'tools', 'runit.py'),
      '--show-path',
      sys.executable,
      os.path.join(args.path_build, 'scripts', 'slave', 'runtest.py')
  ] + args.args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
