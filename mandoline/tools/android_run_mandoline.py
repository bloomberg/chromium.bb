#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import sys

sys.path.insert(0, os.path.join(os.path.abspath(os.path.dirname(__file__)),
                                '../../mojo/tools'))

from mopy.android import AndroidShell
from mopy.config import Config

USAGE = ("android_run_mandoline.py [<shell-and-app-args>] [<mojo-app>]")

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
  parser.add_argument('--target-device', help='Device to run on.')
  parser.add_argument('--gdb', help='Run gdb',
                      default=False, action='store_true')
  launcher_args, args = parser.parse_known_args()

  config = Config(target_os=Config.OS_ANDROID,
                  target_cpu=launcher_args.target_cpu,
                  is_debug=launcher_args.debug,
                  apk_name="Mandoline.apk")
  shell = AndroidShell(config, launcher_args.target_device)

  extra_shell_args = shell.PrepareShellRun(gdb=launcher_args.gdb)
  args.extend(extra_shell_args)

  shell.CleanLogs()
  p = shell.ShowLogs()
  shell.StartShell(args, sys.stdout, p.terminate, launcher_args.gdb)
  return 0


if __name__ == "__main__":
  sys.exit(main())
