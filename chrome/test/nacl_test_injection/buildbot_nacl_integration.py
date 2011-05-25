#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


def Main():
  pwd = os.environ.get('PWD', '')
  is_integration_bot = 'nacl-chrome' in pwd

  if not is_integration_bot:
    # On the main Chrome waterfall, we may need to control where the tests are
    # run.

    # TODO(ncbray): enable on all platforms.
    if sys.platform in ['win32', 'cygwin']: return
    if sys.platform == 'darwin': return
    # if sys.platform in ['linux', 'linux2']: return

    # Uncomment the following line if there is skew in the PPAPI interface
    # and the tests are failing.  Comment out once the issues are resolved.
    # return

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
