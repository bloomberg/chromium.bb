#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Entry point for both build and try bots

This script is invoked from XXX, usually without arguments.

To determin which commands to run, the script inspects the environment:
  BUILDBOT_BUILDERNAME
"""


import os
import sys
import subprocess


# Set the directory that this script lives in.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


SDK_BUILDER_MAP = {
    'DEFAULT':
        [sys.executable, 'build_sdk.py'],
}


def Run(args, cwd=None, shell=False):
  """Start a process with the provided arguments.
  
  Starts a process in the provided directory given the provided arguments. If
  shell is not False, the process is launched via the shell to provide shell
  interpretation of the arguments.  Shell behavior can differ between platforms
  so this should be avoided when not using platform dependent shell scripts."""
  print 'Running: ' + ' '.join(args)
  sys.stdout.flush()
  sys.stderr.flush()
  subprocess.check_call(args, cwd=cwd, shell=shell)
  sys.stdout.flush()
  sys.stderr.flush()


def main(args):
  args = args[1:]
  buildername = os.environ.get('BUILDBOT_BUILDERNAME', '')
  cmd = SDK_BUILDER_MAP.get(buildername) or SDK_BUILDER_MAP.get('DEFAULT')
  Run(cmd + args, cwd=SCRIPT_DIR)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))