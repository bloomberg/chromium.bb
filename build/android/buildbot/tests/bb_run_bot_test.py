#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

BUILDBOT_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILDBOT_DIR)
import bb_run_bot


def main():
  code = 0
  procs = [
      (bot, subprocess.Popen(
          [os.path.join(BUILDBOT_DIR, 'bb_run_bot.py'), '--bot-id', bot,
          '--testing'], stdout=subprocess.PIPE, stderr=subprocess.PIPE))
      for bot in bb_run_bot.GetBotStepMap()]
  for bot, proc in procs:
    _, err = proc.communicate()
    code |= proc.returncode
    if proc.returncode != 0:
      print 'Error running bb_run_bot with id="%s"' % bot, err

  return code


if __name__ == '__main__':
  sys.exit(main())
