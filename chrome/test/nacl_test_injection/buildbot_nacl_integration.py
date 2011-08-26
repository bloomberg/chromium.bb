#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


def Main(args):
  pwd = os.environ.get('PWD', '')
  is_integration_bot = 'nacl-chrome' in pwd

  # On the main Chrome waterfall, we may need to control where the tests are
  # run.
  # If there is serious skew in the PPAPI interface that causes all of
  # the NaCl integration tests to fail, you can uncomment the
  # following block.  (Make sure you comment it out when the issues
  # are resolved.)  *However*, it is much preferred to add tests to
  # the 'tests_to_disable' list below.
  #if not is_integration_bot:
  #  return

  tests_to_disable = []

  # In general, you should disable tests inside this conditional.  This turns
  # them off on the main Chrome waterfall, but not on NaCl's integration bots.
  # This makes it easier to see when things have been fixed NaCl side.
  if not is_integration_bot:
    # TODO(ncbray): Reenable when this issue is resolved.
    # http://code.google.com/p/nativeclient/issues/detail?id=2091
    tests_to_disable.append('run_ppapi_bad_browser_test')
    # This thread safety stress test is flaky on at least Windows.
    # See http://code.google.com/p/nativeclient/issues/detail?id=2124
    # TODO(mseaborn): Reenable when this issue is resolved.
    tests_to_disable.append('run_ppapi_ppb_var_browser_test')
    # Te behavior of the URLRequest changed slightly and this test needs to be
    # updated. http://code.google.com/p/chromium/issues/detail?id=94352
    tests_to_disable.append('run_ppapi_ppb_url_request_info_browser_test')

  if sys.platform == 'darwin':
    # The following test is failing on Mac OS X 10.5.  This may be
    # because of a kernel bug that we might need to work around.
    # See http://code.google.com/p/nativeclient/issues/detail?id=1835
    # TODO(mseaborn): Remove this when the issue is resolved.
    tests_to_disable.append('run_async_messaging_test')
    # The following test fails on debug builds of Chromium.
    # See http://code.google.com/p/nativeclient/issues/detail?id=2077
    # TODO(mseaborn): Remove this when the issue is resolved.
    tests_to_disable.append('run_ppapi_example_font_test')

  script_dir = os.path.dirname(os.path.abspath(__file__))
  test_dir = os.path.dirname(script_dir)
  chrome_dir = os.path.dirname(test_dir)
  src_dir = os.path.dirname(chrome_dir)
  nacl_integration_script = os.path.join(
      src_dir, 'native_client/build/buildbot_chrome_nacl_stage.py')
  cmd = [sys.executable,
         nacl_integration_script,
         '--disable_tests=%s' % ','.join(tests_to_disable)] + args
  sys.stdout.write('Running %s\n' % ' '.join(cmd))
  sys.stdout.flush()
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
