#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import sys

from mopy.android import AndroidShell
from mopy.config import Config

USAGE = ('android_mojo_shell.py [<shell-and-app-args>] [<mojo-app>]')

def main():
  logging.basicConfig()

  parser = argparse.ArgumentParser(usage=USAGE)

  debug_group = parser.add_mutually_exclusive_group()
  debug_group.add_argument('--debug', help='Debug build (default)',
                           default=True, action='store_true')
  debug_group.add_argument('--release', help='Release build', default=False,
                           dest='debug', action='store_false')
  parser.add_argument('--target-cpu', help='CPU architecture to run for.',
                      choices=['x64', 'x86', 'arm'], default='arm')
  parser.add_argument('--device', help='Serial number of the target device.')
  parser.add_argument('--verbose', default=False, action='store_true')
  parser.add_argument('--apk', help='Name of the APK to run.',
                      default='MojoRunner.apk')
  runner_args, args = parser.parse_known_args()

  logger = logging.getLogger()
  logging.basicConfig(stream=sys.stdout, format='%(levelname)s:%(message)s')
  logger.setLevel(logging.DEBUG if runner_args.verbose else logging.WARNING)
  logger.debug('Initialized logging: level=%s' % logger.level)

  config = Config(target_os=Config.OS_ANDROID,
                  target_cpu=runner_args.target_cpu,
                  is_debug=runner_args.debug,
                  is_verbose=runner_args.verbose,
                  apk_name=runner_args.apk)
  shell = AndroidShell(config)
  shell.InitShell(runner_args.device)
  p = shell.ShowLogs()
  shell.StartActivity('MojoShellActivity', args, sys.stdout, p.terminate)
  return 0


if __name__ == '__main__':
  sys.exit(main())
