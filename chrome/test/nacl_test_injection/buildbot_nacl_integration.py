#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


# TODO(phajdan.jr): Make buildbot refer to the update path and remove this file.


def Main(args):
  script_dir = os.path.dirname(os.path.abspath(__file__))
  test_dir = os.path.dirname(script_dir)
  chrome_dir = os.path.dirname(test_dir)

  nacl_integration_script = os.path.join(
      chrome_dir, 'nacl', 'buildbot_nacl_integration.py')
  cmd = [sys.executable, nacl_integration_script]
  sys.stdout.write('Running %s\n' % ' '.join(cmd))
  sys.stdout.flush()
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
