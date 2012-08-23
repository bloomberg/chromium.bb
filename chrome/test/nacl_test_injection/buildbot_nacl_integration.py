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

  # This environment variable check mimics what
  # buildbot_chrome_nacl_stage.py does.
  is_win64 = (sys.platform in ('win32', 'cygwin') and
              ('64' in os.environ.get('PROCESSOR_ARCHITECTURE', '') or
               '64' in os.environ.get('PROCESSOR_ARCHITEW6432', '')))

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
    # http://code.google.com/p/nativeclient/issues/detail?id=2511
    tests_to_disable.append('run_ppapi_ppb_image_data_browser_test')

    # TODO(ncbray) are these tests stable?
    tests_to_disable.extend([
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
      # This one is only failing for nacl_glibc on x64 Windows
      # but it is not clear how to disable only that limited case.
      # See http://crbug.com/132395
      tests_to_disable.append('run_inbrowser_test_runner')

    if sys.platform in ('win32', 'cygwin') and not is_win64:
      # The Breakpad tests have started failing on 32-bit Windows (but
      # not 64-bit Windows) because Chromium is producing an excess
      # crash dump.
      # See http://code.google.com/p/chromium/issues/detail?id=143413
      # TODO(mseaborn): Change the tests to ignore the excess crash
      # dump until we find out what is causing it.
      tests_to_disable.extend([
          'run_breakpad_trusted_crash_in_startup_test',
          'run_breakpad_crash_in_syscall_test',
          'run_breakpad_untrusted_crash_test',
          # TODO(mseaborn): Remove these when the test is removed from
          # the NaCl tree.
          'run_inbrowser_trusted_crash_in_startup_test',
          'run_inbrowser_crash_in_syscall_test',
          'run_inbrowser_untrusted_crash_test',
          ])

  if sys.platform in ('win32', 'cygwin'):
    tests_to_disable.append('run_ppapi_ppp_input_event_browser_test')

  # Webkit seems to be slightly buggy and breaking this test.  Disabled so
  # Webkit's DEPS can be rolled.
  # See: http://crbug.com/143973
  # TODO(ncbray) re-enable.
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
         # TODO(ncbray) re-enable.
         # https://code.google.com/p/chromium/issues/detail?id=133568
         '--disable_glibc',
         '--disable_tests=%s' % ','.join(tests_to_disable)]
  cmd += args
  sys.stdout.write('Running %s\n' % ' '.join(cmd))
  sys.stdout.flush()
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
