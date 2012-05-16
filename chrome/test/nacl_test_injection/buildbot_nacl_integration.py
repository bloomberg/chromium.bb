#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    # TODO(nfullagar): Reenable when this issue is resolved.
    # http://code.google.com/p/chromium/issues/detail?id=116169
    tests_to_disable.append('run_ppapi_example_audio_test');
    # TODO(ncbray): Reenable when this issue is resolved.
    # http://code.google.com/p/nativeclient/issues/detail?id=2091
    tests_to_disable.append('run_ppapi_bad_browser_test')
    # This thread safety stress test is flaky on at least Windows.
    # See http://code.google.com/p/nativeclient/issues/detail?id=2124
    # TODO(mseaborn): Reenable when this issue is resolved.
    tests_to_disable.append('run_ppapi_ppb_var_browser_test')
    # The behavior of the URLRequest changed slightly and this test needs to be
    # updated. http://code.google.com/p/chromium/issues/detail?id=94352
    tests_to_disable.append('run_ppapi_ppb_url_request_info_browser_test')
    # This test failed and caused the build's gatekeep to close the tree.
    # http://code.google.com/p/chromium/issues/detail?id=96434
    tests_to_disable.append('run_ppapi_example_post_message_test')
    # These tests are flakey on the chrome waterfall and need to be looked at.
    # TODO(bsy): http://code.google.com/p/nativeclient/issues/detail?id=2509
    tests_to_disable.append('run_pm_redir_stderr_fg_0_chrome_browser_test')
    tests_to_disable.append('run_pm_redir_stderr_bg_0_chrome_browser_test')
    tests_to_disable.append('run_pm_redir_stderr_bg_1000_chrome_browser_test')
    tests_to_disable.append('run_pm_redir_stderr_bg_1000000_chrome_browser_test')
    # http://code.google.com/p/nativeclient/issues/detail?id=2511
    tests_to_disable.append('run_ppapi_ppb_image_data_browser_test')
    # Font API is only exported to trusted plugins now, and this test should be
    # removed.
    tests_to_disable.append('run_ppapi_example_font_test')

    # TODO(ncbray) why did these tests flake?
    # http://code.google.com/p/nativeclient/issues/detail?id=2230
    tests_to_disable.extend([
        'run_pm_manifest_file_chrome_browser_test',
        'run_srpc_basic_chrome_browser_test',
        'run_srpc_hw_data_chrome_browser_test',
        'run_srpc_hw_chrome_browser_test',
        'run_srpc_manifest_file_chrome_browser_test',
        'run_srpc_nameservice_chrome_browser_test',
        'run_srpc_nrd_xfer_chrome_browser_test',
        'run_no_fault_pm_nameservice_chrome_browser_test',
        'run_fault_pm_nameservice_chrome_browser_test',
        'run_fault_pq_os_pm_nameservice_chrome_browser_test',
        'run_fault_pq_dep_pm_nameservice_chrome_browser_test',
        ])

    if sys.platform == 'darwin':
      # TODO(mseaborn) fix
      # http://code.google.com/p/nativeclient/issues/detail?id=1835
      tests_to_disable.append('run_ppapi_crash_browser_test')

  if sys.platform in ('win32', 'cygwin'):
    tests_to_disable.append('run_ppapi_ppp_input_event_browser_test')

  script_dir = os.path.dirname(os.path.abspath(__file__))
  test_dir = os.path.dirname(script_dir)
  chrome_dir = os.path.dirname(test_dir)
  src_dir = os.path.dirname(chrome_dir)
  nacl_integration_script = os.path.join(
      src_dir, 'native_client/build/buildbot_chrome_nacl_stage.py')
  cmd = [sys.executable,
         '/b/build/scripts/slave/runtest.py',
         '--run-python-script',
         '--target=',
         '--build-dir=',
         '--',
         nacl_integration_script,
         '--disable_tests=%s' % ','.join(tests_to_disable)]
  if not is_integration_bot:
    if sys.platform in ('win32', 'cygwin'):
      # http://code.google.com/p/nativeclient/issues/detail?id=2648
      cmd.append('--disable_glibc')
  cmd += args
  sys.stdout.write('Running %s\n' % ' '.join(cmd))
  sys.stdout.flush()
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
