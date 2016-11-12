#!/usr/bin/env python
#
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# This script is used to run OfflinePageSavePageLaterEvaluationTests.
# The test will try to call SavePageLater on the list provided as the input,
# and generate results of the background offlining. Then it will pull the
# results to the output directory.
#
# Example Steps:
# 1. Build chrome_public_test_apk
# 2. Prepare a list of urls.
# 3. Run the script (use -d when you have more than one device connected.)
#   run_offline_page_evaluation_test.py --output-directory
#   ~/offline_eval_short_output/ --url-timeout 150 --user-requested=true
#   --use-test-scheduler=true $CHROME_SRC/out/Default ~/offline_eval_urls.txt
# 4. Check the results in the output directory.

import argparse
import os
import shutil
import subprocess
import sys

DEFAULT_URL_TIMEOUT = 60 * 8
DEFAULT_USER_REQUEST = True
DEFAULT_USE_TEST_SCHEDULER = False
DEFAULT_VERBOSE = False
CONFIG_FILENAME = 'test_config'
CONFIG_TEMPLATE = """\
TimeoutPerUrlInSeconds = {timeout_per_url_in_seconds}
IsUserRequested = {is_user_requested}
UseTestScheduler = {use_test_scheduler}
"""


def main(args):
  # Setting up the argument parser.
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--output-directory',
      dest='output_dir',
      help='Directory for output. Default is ~/offline_eval_output/')
  parser.add_argument(
      '--url-timeout',
      type=int,
      dest='url_timeout',
      help='Time out per url, in seconds. Default is 480 seconds.')
  parser.add_argument(
      '--user-requested',
      dest='user_request',
      action='store_true',
      help='Testing as user-requested urls. Default option.')
  parser.add_argument(
      '--not-user-requested',
      dest='user_request',
      action='store_false',
      help='Testing as not user-requested urls.')
  parser.add_argument(
      '--use-test-scheduler',
      dest='use_test_scheduler',
      action='store_true',
      help='Use test scheduler to avoid real scheduling')
  parser.add_argument(
      '--not-use-test-scheduler',
      dest='use_test_scheduler',
      action='store_false',
      help='Use GCMNetworkManager for scheduling. Default option.')
  parser.add_argument(
      '-v',
      '--verbose',
      dest='verbose',
      action='store_true',
      help='Make test runner verbose.')
  parser.add_argument(
      '-d',
      '--device',
      type=str,
      dest='device_id',
      help='Specify which device to be used. See \'adb devices\'.')
  parser.add_argument('build_output_dir', help='Path to build directory.')
  parser.add_argument(
      'test_urls_file', help='Path to input file with urls to be tested.')
  parser.set_defaults(
      output_dir=os.path.expanduser('~/offline_eval_output'),
      url_timeout=DEFAULT_URL_TIMEOUT,
      user_request=DEFAULT_USER_REQUEST,
      user_test_scheduler=DEFAULT_USE_TEST_SCHEDULER,
      verbose=DEFAULT_VERBOSE)

  def get_adb_command(args):
    if options.device_id != None:
      return ['adb', '-s', options.device_id] + args
    return ['adb'] + args

  # Get the arguments and several paths.
  options, extra_args = parser.parse_known_args(args)

  if extra_args:
    print 'Unknown args: ' + ', '.join(
        extra_args) + '. Please check and run again.'
    return

  build_dir_path = os.path.abspath(
      os.path.join(os.getcwd(), options.build_output_dir))
  test_runner_path = os.path.join(build_dir_path,
                                  'bin/run_chrome_public_test_apk')
  config_output_path = os.path.join(options.output_dir, CONFIG_FILENAME)
  external_dir = subprocess.check_output(
      get_adb_command(['shell', 'echo', '$EXTERNAL_STORAGE'])).strip()

  # Create the output directory for results, and have a copy of test config
  # there.
  if not os.path.exists(options.output_dir):
    print 'Creating output directory for results... ' + options.output_dir
    os.makedirs(options.output_dir)
  with open(config_output_path, 'w') as config:
    config.write(
        CONFIG_TEMPLATE.format(
            timeout_per_url_in_seconds=options.url_timeout,
            is_user_requested=options.user_request,
            use_test_scheduler=options.use_test_scheduler))

  print 'Uploading config file and input file onto the device.'
  subprocess.call(
      get_adb_command(
          ['push', config_output_path, external_dir + '/paquete/test_config']))
  subprocess.call(
      get_adb_command([
          'push', options.test_urls_file,
          '/sdcard/paquete/offline_eval_urls.txt'
      ]))
  print 'Start running test...'

  # Run test
  test_runner_cmd = [
      test_runner_path, '-f',
      'OfflinePageSavePageLaterEvaluationTest.testFailureRateWithTimeout'
  ]
  if options.verbose:
    test_runner_cmd += ['-v']
  subprocess.call(test_runner_cmd)

  print 'Fetching results from device...'
  archive_dir = os.path.join(options.output_dir, 'archives/')
  if os.path.exists(archive_dir):
    shutil.rmtree(archive_dir)
  subprocess.call(
      get_adb_command(
          ['pull', external_dir + '/paquete/archives', archive_dir]))
  subprocess.call(
      get_adb_command([
          'pull', external_dir + '/paquete/offline_eval_results.txt',
          options.output_dir
      ]))
  subprocess.call(
      get_adb_command([
          'pull', external_dir + '/paquete/offline_eval_logs.txt',
          options.output_dir
      ]))
  print 'Test finished!'


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
