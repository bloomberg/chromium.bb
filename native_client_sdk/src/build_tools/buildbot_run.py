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
import subprocess
import sys

from buildbot_common import Run
from build_paths import SRC_DIR, SDK_SRC_DIR, SCRIPT_DIR
import getos


def StepRunUnittests():
  buildbot_common.BuildStep('Run unittests')

  # Our tests shouldn't be using the proxy; they should all be connecting to
  # localhost. Some slaves can't route HTTP traffic through the proxy to
  # localhost (we get 504 gateway errors), so we clear it here.
  env = dict(os.environ)
  if 'http_proxy' in env:
    del env['http_proxy']

  Run([sys.executable, 'test_all.py'], env=env, cwd=SDK_SRC_DIR)


def StepBuildSDK():
  is_win = getos.GetPlatform() == 'win'

  # Windows has a path length limit of 255 characters, after joining cwd with a
  # relative path. Use subst before building to keep the path lengths short.
  if is_win:
    subst_drive = 'S:'
    root_dir = os.path.dirname(SRC_DIR)
    new_root_dir = subst_drive + '\\'
    subprocess.check_call(['subst', subst_drive, root_dir])
    new_script_dir = os.path.join(new_root_dir,
                                  os.path.relpath(SCRIPT_DIR, root_dir))
  else:
    new_script_dir = SCRIPT_DIR

  try:
    Run([sys.executable, 'build_sdk.py'], cwd=new_script_dir)
  finally:
    if is_win:
      subprocess.check_call(['subst', '/D', subst_drive])


def StepTestSDK():
  cmd = []
  if getos.GetPlatform() == 'linux':
    # Run all of test_sdk.py under xvfb-run; it's startup time leaves something
    # to be desired, so only start it up once.
    # We also need to make sure that there are at least 24 bits per pixel.
    # https://code.google.com/p/chromium/issues/detail?id=316687
    cmd.extend([
        'xvfb-run',
        '--auto-servernum',
        '--server-args', '-screen 0 1024x768x24'
    ])

  cmd.extend([sys.executable, 'test_sdk.py'])
  Run(cmd, cwd=SCRIPT_DIR)


def main():
  StepRunUnittests()
  StepBuildSDK()
  # Skip the testing phase if we are running on a build-only bots.
  if not buildbot_common.IsBuildOnlyBot():
    StepTestSDK()
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main())
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('buildbot_run: interrupted')
