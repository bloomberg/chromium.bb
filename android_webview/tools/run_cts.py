#!/usr/bin/env python
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the CTS test APKs stored in GS."""

import argparse
import json
import os
import shutil
import sys
import tempfile


sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
import devil_chromium  # pylint: disable=import-error
from devil.utils import cmd_helper  # pylint: disable=import-error

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build'))
import find_depot_tools  # pylint: disable=import-error

_CTS_BUCKET = 'gs://chromium-cts'

_GSUTIL_PATH = os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py')
_TEST_RUNNER_PATH = os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir,
    'build', 'android', 'test_runner.py')

_EXPECTED_FAILURES_FILE = os.path.join(
    os.path.dirname(__file__), 'cts_config', 'expected_failure_on_bot.json')
_WEBVIEW_CTS_GCS_PATH_FILE = os.path.join(
    os.path.dirname(__file__), 'cts_config', 'webview_cts_gcs_path.json')


def GetCtsPath(arch, platform):
  """Gets relative path the CTS APK is stored at."""
  with open(_WEBVIEW_CTS_GCS_PATH_FILE) as f:
    cts_gcs_path_info = json.load(f)
  try:
    return cts_gcs_path_info[arch][platform]['apk']
  except KeyError:
    raise Exception('No CTS test available for arch:%s, android:%s' %
                    (arch, platform))


def GetExpectedFailures():
  """Gets list of tests expected to fail."""
  with open(_EXPECTED_FAILURES_FILE) as f:
    expected_failures_info = json.load(f)
  expected_failures = []
  for class_name, methods in expected_failures_info.iteritems():
    expected_failures.extend(['%s#%s' % (class_name, m['name'])
                              for m in methods])
  return expected_failures


def DownloadAndRunCTS(args, test_runner_args):
  base_cts_dir = None
  delete_cts_dir = False
  try:
    relative_cts_path = GetCtsPath(args.arch, args.platform)

    if args.apk_dir:
      base_cts_dir = args.apk_dir
    else:
      base_cts_dir = tempfile.mkdtemp()
      delete_cts_dir = True

    local_cts_path = os.path.join(base_cts_dir, relative_cts_path)
    google_storage_cts_path = '%s/%s' % (_CTS_BUCKET, relative_cts_path)

    # Download CTS APK if needed.
    if not os.path.exists(local_cts_path):
      if cmd_helper.RunCmd(
          [_GSUTIL_PATH, 'cp', google_storage_cts_path, local_cts_path]):
        raise Exception('Error downloading CTS from Google Storage.')

    test_runner_args += ['--test-apk', local_cts_path]
    # TODO(mikecase): This doesn't work at all with the
    # --gtest-filter test runner option currently. The
    # filter options will just override eachother.
    if args.skip_expected_failures:
      test_runner_args += ['-f=-%s' % ':'.join(GetExpectedFailures())]
    return cmd_helper.RunCmd(
        [_TEST_RUNNER_PATH, 'instrumentation'] + test_runner_args)
  finally:
    if delete_cts_dir and base_cts_dir:
      shutil.rmtree(base_cts_dir)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--arch',
      choices=['arm_64'],
      default='arm_64',
      help='Arch for CTS tests.')
  parser.add_argument(
      '--platform',
      choices=['L', 'M', 'N'],
      required=True,
      help='Android platform version for CTS tests.')
  parser.add_argument(
      '--skip-expected-failures',
      action='store_true',
      help='Option to skip all tests that are expected to fail.')
  parser.add_argument(
      '--apk-dir',
      help='Directory to load/save CTS APKs. Will try to load CTS APK '
           'from this directory before downloading from Google Storage '
           'and will then cache APK here.')

  args, test_runner_args = parser.parse_known_args()
  devil_chromium.Initialize()

  return DownloadAndRunCTS(args, test_runner_args)


if __name__ == '__main__':
  sys.exit(main())
