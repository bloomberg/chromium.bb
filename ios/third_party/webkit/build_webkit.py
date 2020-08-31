#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

def main():
  description = 'Invokes build-webkit with the given options.'
  parser = argparse.ArgumentParser(description=description)
  parser.add_argument('--output_dir',
                    help='Output directory for build products.')
  parser.add_argument('--ios_simulator', action='store_true', default=False,
                      help='Use "iphoneos" SDK instead of "macos".')
  parser.add_argument('--asan', action='store_true', default=False,
                      help='Make Asan build.')
  parser.add_argument('--clean', action='store_true', default=False,
                      help='Clean output directory before building.')
  parser.add_argument('--debug', action='store_true', default=False,
                      help='Make debug build.')
  parser.add_argument('-j',
                    help='Number of parallel jobs to run.')
  (opts, extra_args) = parser.parse_known_args()

  output_dir = opts.output_dir
  if not output_dir:
    # Use a default that matches what ninja uses.
    output_dir = os.path.realpath(os.path.join(
      os.path.dirname(__file__),
      '../../..',
      'out', 'Debug-iphonesimulator', 'obj', 'ios', 'third_party', 'webkit'));

  command = ['src/Tools/Scripts/build-webkit']

  if opts.ios_simulator:
    command.append('--ios-simulator')

  if opts.debug:
    command.append('--debug')

  if opts.j:
    command.extend(['-jobs', opts.j])
  command.extend(extra_args)

  env = {'WEBKIT_OUTPUTDIR': output_dir}
  cwd = os.path.dirname(os.path.realpath(__file__))

  if opts.clean:
     clean_command = ['src/Tools/Scripts/clean-webkit']
     proc = subprocess.Popen(clean_command, cwd=cwd, env=env)
     proc.communicate()

  if opts.asan:
     config_command = ['src/Tools/Scripts/set-webkit-configuration', '--asan']
     if opts.debug:
       config_command.append('--debug')
     proc = subprocess.Popen(config_command, cwd=cwd, env=env)
     proc.communicate()
     if proc.returncode:
       return proc.returncode

  proc = subprocess.Popen(command, cwd=cwd, env=env)
  proc.communicate()
  return proc.returncode

if __name__ == '__main__':
  sys.exit(main())
