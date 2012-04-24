#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Entry point for both build and try bots

This script is invoked from XXX, usually without arguments.

To determin which commands to run, the script inspects the environment:
  BUILDBOT_BUILDERNAME
"""


import buildbot_common
import os
import sys

# Set the directory that this script lives in.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


SDK_BUILDER_MAP = {
    'linux-sdk-mono32':
        [sys.executable, 'nacl-mono-buildbot.py'],
    'linux-sdk-mono64':
        [sys.executable, 'nacl-mono-buildbot.py'],
    'DEFAULT':
        [sys.executable, 'build_sdk.py'],
}


def main(args):
  args = args[1:]
  buildername = os.environ.get('BUILDBOT_BUILDERNAME', '')
  cmd = SDK_BUILDER_MAP.get(buildername) or SDK_BUILDER_MAP.get('DEFAULT')
  buildbot_common.Run(cmd + args, cwd=SCRIPT_DIR)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
