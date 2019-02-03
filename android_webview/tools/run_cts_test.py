# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

import mock
import run_cts

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
import devil_chromium  # pylint: disable=import-error, unused-import
from devil.android.ndk import abis  # pylint: disable=import-error

class _RunCtsTest(unittest.TestCase):
  """Unittests for the run_cts module.
  """

  _EXCLUDED_TEST = 'bad#test'
  # This conforms to schema of the test_run entry in
  # cts_config/webview_cts_gcs_path.json
  _CTS_RUN = {'apk': 'module.apk', 'excludes': [{'match': _EXCLUDED_TEST}]}

  @staticmethod
  def _getArgsMock(**kwargs):
    args = {'test_filter_file': None, 'test_filter': None,
            'isolated_script_test_filter': None,
            'skip_expected_failures': False}
    args.update(kwargs)
    return mock.Mock(**args)

  def testDetermineArch_arm64(self):
    logging_mock = mock.Mock()
    logging.info = logging_mock
    device = mock.Mock(product_cpu_abi=abis.ARM_64)
    self.assertEqual(run_cts.DetermineArch(device), 'arm64')
    # We should log a message to explain how we auto-determined the arch. We
    # don't assert the message itself, since that's rather strict.
    logging_mock.assert_called()

  def testDetermineArch_unsupported(self):
    device = mock.Mock(product_cpu_abi='madeup-abi')
    with self.assertRaises(Exception) as _:
      run_cts.DetermineArch(device)

  def testNoFilter_SkipExpectedFailures(self):
    mock_args = self._getArgsMock(skip_expected_failures=True)
    skips = run_cts.GetExpectedFailures()
    skips.append(self._EXCLUDED_TEST)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=-' + ':'.join(skips)],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testNoFilter_ExcludedMatches(self):
    mock_args = self._getArgsMock(skip_expected_failures=False)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=-' + self._EXCLUDED_TEST],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_OverridesExcludedMatches(self):
    mock_args = self._getArgsMock(test_filter='good#test',
                                  skip_expected_failures=False)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good#test'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_OverridesAll(self):
    mock_args = self._getArgsMock(test_filter='good#test',
                                  skip_expected_failures=True)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good#test'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilter_ForMultipleTests(self):
    mock_args = self._getArgsMock(test_filter='good#t1:good#t2',
                                  skip_expected_failures=True)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=good#t1:good#t2'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testIsolatedFilter_OverridesExcludedMatches(self):
    mock_args = self._getArgsMock(isolated_script_test_filter='good#test',
                                  skip_expected_failures=False)
    self.assertEqual([run_cts.ISOLATED_FILTER_OPT + '=good#test'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testIsolatedFilter_OverridesAll(self):
    mock_args = self._getArgsMock(isolated_script_test_filter='good#test',
                                  skip_expected_failures=True)
    self.assertEqual([run_cts.ISOLATED_FILTER_OPT + '=good#test'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testIsolatedFilter_ForMultipleTests(self):
    # Isolated test filters use :: to separate matches
    mock_args = self._getArgsMock(
        isolated_script_test_filter='good#t1::good#t2',
        skip_expected_failures=True)
    self.assertEqual([run_cts.ISOLATED_FILTER_OPT + '=good#t1::good#t2'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilterFile_OverridesExcludedMatches(self):
    mock_args = self._getArgsMock(test_filter_file='test.filter',
                                  skip_expected_failures=False)
    self.assertEqual([run_cts.FILE_FILTER_OPT + '=test.filter'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testFilterFile_OverridesAll(self):
    mock_args = self._getArgsMock(test_filter_file='test.filter',
                                  skip_expected_failures=True)
    self.assertEqual([run_cts.FILE_FILTER_OPT + '=test.filter'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testNegative_Filter(self):
    mock_args = self._getArgsMock(test_filter='-good#t1:good#t2',
                                  skip_expected_failures=True)
    self.assertEqual([run_cts.TEST_FILTER_OPT + '=-good#t1:good#t2'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))

  def testNegative_IsolatedFilter(self):
    mock_args = self._getArgsMock(
        isolated_script_test_filter='-good#t1::good#t2',
        skip_expected_failures=True)
    self.assertEqual([run_cts.ISOLATED_FILTER_OPT + '=-good#t1::good#t2'],
                     run_cts.GetTestRunFilterArg(mock_args, self._CTS_RUN))


if __name__ == '__main__':
  unittest.main()
