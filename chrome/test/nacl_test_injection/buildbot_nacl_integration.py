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

    # TODO(mseaborn): Enable these tests on Mac OS X.  We believe that
    # NaCl's startup within Chromium may be flaky on Mac and
    # occasionally fail due to a kernel bug that we need to work
    # around.
    # See http://code.google.com/p/nativeclient/issues/detail?id=1835
    if sys.platform == 'darwin': return

    # Uncomment the following line if there is skew in the PPAPI interface
    # and the tests are failing.  Comment out once the issues are resolved.
    return

  script_dir = os.path.dirname(os.path.abspath(__file__))
  test_dir = os.path.dirname(script_dir)
  chrome_dir = os.path.dirname(test_dir)
  src_dir = os.path.dirname(chrome_dir)
  nacl_integration_script = os.path.join(
      src_dir, 'native_client/build/buildbot_chrome_nacl_stage.py')
  cmd = [sys.executable, nacl_integration_script] + sys.argv[1:]

  if sys.platform in ('win32', 'cygwin'):
    # TODO(mseaborn): Re-enable this test when it passes.
    # See http://code.google.com/p/nativeclient/issues/detail?id=2038
    # It is disabled here because it fails in the debug build of Chromium
    # but not in the release build that the NaCl buildbots test against.
    cmd.append('--disable_tests=ppapi_ppb_scrollbar_browser_test')

  print cmd
  subprocess.check_call(cmd)


if __name__ == '__main__':
  Main()
