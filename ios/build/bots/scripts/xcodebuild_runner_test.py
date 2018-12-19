# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for xcodebuild_runner.py."""

import mock
import os

import test_runner
import test_runner_test
import xcodebuild_runner


_ROOT_FOLDER_PATH = 'root/folder'
_XCODE_BUILD_VERSION = '10B61'
_DESTINATION = ('platform=iOS Simulator,OS=12.0,name=iPhone 7 Plus;'
                'platform=iOS Simulator,OS=12.0,name=iPhone X')
_OUT_DIR = 'out/dir'
_XTEST_RUN = '/tmp/temp_file.xctestrun'
_EGTESTS_APP_PATH = '%s/any_egtests.app' % _ROOT_FOLDER_PATH


class XCodebuildRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner."""

  def testEgtests_not_found_egtests_app(self):
    with self.assertRaises(test_runner.AppNotFoundError):
      xcodebuild_runner.EgtestsApp(_EGTESTS_APP_PATH)

  @mock.patch('os.path.exists', autospec=True)
  def testEgtests_not_found_plugins(self, mock_path_exists):
    mock_path_exists.return_value = True
    egtests = xcodebuild_runner.EgtestsApp(_EGTESTS_APP_PATH)
    mock_path_exists.return_value = False
    with self.assertRaises(test_runner.PlugInsNotFoundError):
      egtests._xctest_path()

  @mock.patch('os.listdir', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def testEgtests_found_xctest(self, mock_path_exists, mock_listdir):
    mock_path_exists.return_value = True
    mock_listdir.return_value = ['any_egtests.xctest']
    self.assertEqual('/PlugIns/any_egtests.xctest',
                     xcodebuild_runner.EgtestsApp(
                         _EGTESTS_APP_PATH)._xctest_path())

  @mock.patch('os.listdir', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def testEgtests_not_found_xctest(self, mock_path_exists, mock_listdir):
    mock_path_exists.return_value = True
    mock_listdir.return_value = ['some_egtests.xctest']
    egtest = xcodebuild_runner.EgtestsApp(_EGTESTS_APP_PATH)
    with self.assertRaises(test_runner.XCTestPlugInNotFoundError):
      egtest._xctest_path()

  @mock.patch('os.listdir', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def testEgtests_xctestRunNode_without_filter(self, mock_path_exists,
                                               mock_listdir):
    mock_path_exists.return_value = True
    mock_listdir.return_value = ['any_egtests.xctest']
    egtest_node = xcodebuild_runner.EgtestsApp(
        _EGTESTS_APP_PATH).xctestrun_node()['any_egtests_module']
    self.assertNotIn('OnlyTestIdentifiers', egtest_node)
    self.assertNotIn('SkipTestIdentifiers', egtest_node)

  @mock.patch('os.listdir', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def testEgtests_xctestRunNode_with_filter_only_identifiers(
      self, mock_path_exists, mock_listdir):
    mock_path_exists.return_value = True
    mock_listdir.return_value = ['any_egtests.xctest']
    filtered_tests = ['TestCase1/testMethod1', 'TestCase1/testMethod2',
                      'TestCase2/testMethod1', 'TestCase1/testMethod2']
    egtest_node = xcodebuild_runner.EgtestsApp(
        _EGTESTS_APP_PATH, filtered_tests=filtered_tests).xctestrun_node()[
            'any_egtests_module']
    self.assertEqual(filtered_tests, egtest_node['OnlyTestIdentifiers'])
    self.assertNotIn('SkipTestIdentifiers', egtest_node)

  @mock.patch('os.listdir', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def testEgtests_xctestRunNode_with_filter_skip_identifiers(
      self, mock_path_exists, mock_listdir):
    mock_path_exists.return_value = True
    mock_listdir.return_value = ['any_egtests.xctest']
    skipped_tests = ['TestCase1/testMethod1', 'TestCase1/testMethod2',
                     'TestCase2/testMethod1', 'TestCase1/testMethod2']
    egtest_node = xcodebuild_runner.EgtestsApp(
        _EGTESTS_APP_PATH, filtered_tests=skipped_tests,
        invert=True).xctestrun_node()['any_egtests_module']
    self.assertEqual(skipped_tests, egtest_node['SkipTestIdentifiers'])
    self.assertNotIn('OnlyTestIdentifiers', egtest_node)

  @mock.patch('xcodebuild_runner.LaunchCommand.fill_xctest_run', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def testLaunchCommand_command(self, mock_path_exists, mock_fill_xctestrun):
    mock_path_exists.return_value = True
    destination = 'platform=iOS Simulator,OS=12.0,name=iPhone X'
    mock_fill_xctestrun.return_value = _XTEST_RUN
    mock_egtest = mock.MagicMock(spec=xcodebuild_runner.EgtestsApp)
    type(mock_egtest).egtests_path = mock.PropertyMock(
        return_value=_EGTESTS_APP_PATH)
    cmd = xcodebuild_runner.LaunchCommand(
        mock_egtest, destination, shards=3, retries=1, out_dir=_OUT_DIR)
    self.assertEqual(['xcodebuild', 'test-without-building',
                      '-xctestrun', '/tmp/temp_file.xctestrun',
                      '-destination',
                      'platform=iOS Simulator,OS=12.0,name=iPhone X',
                      '-resultBundlePath', 'out/dir',
                      '-parallel-testing-enabled', 'YES',
                      '-parallel-testing-worker-count', '3'],
                     cmd.command(egtests_app=mock_egtest,
                                 out_dir=_OUT_DIR,
                                 destination=destination,
                                 shards=3))

  @mock.patch('tempfile.mkstemp', autospec=True)
  @mock.patch('plistlib.writePlist', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def testFill_xctest_run(self, mock_path_exists, _, mock_tmpfile):
    mock_path_exists.return_value = True
    mock_tmpfile.return_value = (1, _XTEST_RUN)
    destination = 'platform=iOS Simulator,OS=12.0,name=iPhone X'
    mock_egtest = mock.MagicMock(spec=xcodebuild_runner.EgtestsApp)
    launch_command = xcodebuild_runner.LaunchCommand(
        mock_egtest, destination, shards=1, retries=1, out_dir=_OUT_DIR)
    self.assertEqual(_XTEST_RUN, launch_command.fill_xctest_run(mock_egtest))
    self.assertEqual([mock.call.xctestrun_node()], mock_egtest.method_calls)

  def testFill_xctest_run_exception(self):
    with self.assertRaises(test_runner.AppNotFoundError):
      xcodebuild_runner.LaunchCommand([], 'destination', shards=1, retries=1,
                                      out_dir=_OUT_DIR).fill_xctest_run([])

  @mock.patch('xcodebuild_runner.LaunchCommand.fill_xctest_run', autospec=True)
  @mock.patch('os.path.exists', autospec=True)
  def testLaunchCommand_make_cmd_list_for_failed_tests(self, mock_path_exists,
                                                       fill_xctest_run_mock):
    mock_path_exists.return_value = True
    fill_xctest_run_mock.side_effect = [
        '/var/folders/tmpfile1'
    ]
    egtest_app = 'module_1_egtests.app'
    destination = 'platform=iOS Simulator,OS=12.0,name=iPhone X'
    egtest_app_path = '%s/%s' % (_ROOT_FOLDER_PATH, egtest_app)
    failed_tests = {
        egtest_app: [
            'TestCase1_1/TestMethod1',
            'TestCase1_1/TestMethod2',
            'TestCase1_2/TestMethod1',
        ]
    }
    expected_egtests = xcodebuild_runner.EgtestsApp(
        egtest_app_path, filtered_tests=failed_tests[egtest_app])
    mock_egtest = mock.MagicMock(spec=xcodebuild_runner.EgtestsApp)
    type(mock_egtest).egtests_path = mock.PropertyMock(
        return_value=egtest_app_path)
    cmd = xcodebuild_runner.LaunchCommand(
        egtests_app=mock_egtest,
        destination=destination,
        out_dir='out/dir/attempt_2/iPhone X 12.0',
        shards=1,
        retries=1
    )
    cmd._make_cmd_list_for_failed_tests(
        failed_tests, os.path.join(_OUT_DIR, 'attempt_2'))
    self.assertEqual(1, len(fill_xctest_run_mock.mock_calls))
    self.assertItemsEqual(expected_egtests.__dict__,
                          fill_xctest_run_mock.mock_calls[0][1][1].__dict__)
