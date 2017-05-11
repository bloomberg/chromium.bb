#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage:
#    gclient-new-workdir.py [options] <repository> <new_workdir>
#

import argparse
import os
import shutil
import subprocess
import sys

import git_common


def parse_options():
  if sys.platform == 'win32':
    print('ERROR: This script cannot run on Windows because it uses symlinks.')
    sys.exit(1)

  parser = argparse.ArgumentParser(description='''\
      Clone an existing gclient directory, taking care of all sub-repositories.
      Works similarly to 'git new-workdir'.''')
  parser.add_argument('repository', type=os.path.abspath,
                      help='should contain a .gclient file')
  parser.add_argument('new_workdir', help='must not exist')
  args = parser.parse_args()

  if not os.path.exists(args.repository):
    parser.error('Repository "%s" does not exist.' % args.repository)

  gclient = os.path.join(args.repository, '.gclient')
  if not os.path.exists(gclient):
    parser.error('No .gclient file at "%s".' % gclient)

  if os.path.exists(args.new_workdir):
    parser.error('New workdir "%s" already exists.' % args.new_workdir)

  return args


def main():
  args = parse_options()

  gclient = os.path.join(args.repository, '.gclient')

  os.makedirs(args.new_workdir)
  os.symlink(gclient, os.path.join(args.new_workdir, '.gclient'))

  for root, dirs, _ in os.walk(args.repository):
    if '.git' in dirs:
      workdir = root.replace(args.repository, args.new_workdir, 1)
      print('Creating: %s' % workdir)
      git_common.make_workdir(os.path.join(root, '.git'),
                              os.path.join(workdir, '.git'))
      subprocess.check_call(['git', 'checkout', '-f'], cwd=workdir)


if __name__ == '__main__':
  sys.exit(main())
