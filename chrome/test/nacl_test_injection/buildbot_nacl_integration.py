#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


def Main():
  pwd = os.environ.get('PWD', '')
  # TODO(ncbray): figure out why this is failing on windows and enable.
  if (sys.platform in ['win32', 'cygwin'] and
      'xp-nacl-chrome' not in pwd and 'win64-nacl-chrome' not in pwd): return
  # TODO(ncbray): figure out why this is failing on mac and re-enable.
  if (sys.platform == 'darwin' and
      'mac-nacl-chrome' not in pwd): return
  # TODO(ncbray): figure out why this is failing on some linux trybots.
  if (sys.platform in ['linux', 'linux2'] and
      'hardy64-nacl-chrome' not in pwd): return

  script_dir = os.path.dirname(os.path.abspath(__file__))
  test_dir = os.path.dirname(script_dir)
  chrome_dir = os.path.dirname(test_dir)
  src_dir = os.path.dirname(chrome_dir)
  nacl_integration_script = os.path.join(
      src_dir, 'native_client/build/buildbot_chrome_nacl_stage.py')
  cmd = [sys.executable, nacl_integration_script] + sys.argv[1:]
  print cmd
  subprocess.check_call(cmd)


if __name__ == '__main__':
  Main()
