#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main entry point for the NaCl SDK buildbot.

The entry point used to be build_sdk.py itself, but we want
to be able to simplify build_sdk (for example separating out
the test code into test_sdk) and change its default behaviour
while being able to separately control excactly what the bots
run.
"""

import buildbot_common
import os
import sys
from buildbot_common import Run
from build_paths import SDK_SRC_DIR, SCRIPT_DIR


def StepRunUnittests():
  buildbot_common.BuildStep('Run unittests')

  # Our tests shouldn't be using the proxy; they should all be connecting to
  # localhost. Some slaves can't route HTTP traffic through the proxy to
  # localhost (we get 504 gateway errors), so we clear it here.
  env = dict(os.environ)
  if 'http_proxy' in env:
    del env['http_proxy']

  Run([sys.executable, 'test_all.py'], env=env, cwd=SDK_SRC_DIR)


def StepBuildSDK(args):
  Run([sys.executable, 'build_sdk.py'] + args, cwd=SCRIPT_DIR)


def StepTestSDK():
  Run([sys.executable, 'test_sdk.py'], cwd=SCRIPT_DIR)


def main(args):
  StepRunUnittests()
  StepBuildSDK(args)
  StepTestSDK()
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('buildbot_run: interrupted')
