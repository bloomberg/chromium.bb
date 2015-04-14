#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import sys

from mopy.config import Config
from mopy import android

def main():
  logging.basicConfig()

  parser = argparse.ArgumentParser("Helper for running mojo_shell")

  debug_group = parser.add_mutually_exclusive_group()
  debug_group.add_argument('--debug', help='Debug build (default)',
                           default=True, action='store_true')
  debug_group.add_argument('--release', help='Release build', default=False,
                           dest='debug', action='store_false')
  parser.add_argument('--target-cpu', help='CPU architecture to run for.',
                      choices=['x64', 'x86', 'arm'])
  parser.add_argument('--origin', help='Origin for mojo: URLs.')
  launcher_args, args = parser.parse_known_args()

  config = Config(target_os=Config.OS_ANDROID,
                  target_cpu=launcher_args.target_cpu,
                  is_debug=launcher_args.debug)

  extra_shell_args = android.PrepareShellRun(config, launcher_args.origin)
  args.extend(extra_shell_args)

  android.CleanLogs()
  p = android.ShowLogs()
  android.StartShell(args, sys.stdout, p.terminate)
  return 0


if __name__ == "__main__":
  sys.exit(main())
