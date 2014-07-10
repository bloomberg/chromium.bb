# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates test runner factory and tests for GTests."""
# pylint: disable=W0212

import fnmatch
import glob
import logging
import os
import shutil
import sys

from pylib import cmd_helper
from pylib import constants

from pylib.base import base_test_result
from pylib.base import test_dispatcher
from pylib.gtest import test_package_apk
from pylib.gtest import test_package_exe
from pylib.gtest import test_runner

sys.path.insert(0,
                os.path.join(constants.DIR_SOURCE_ROOT, 'build', 'util', 'lib',
                             'common'))
import unittest_util # pylint: disable=F0401


_ISOLATE_FILE_PATHS = {
    'base_unittests': 'base/base_unittests.isolate',
    'blink_heap_unittests':
      'third_party/WebKit/Source/platform/heap/BlinkHeapUnitTests.isolate',
    'breakpad_unittests': 'breakpad/breakpad_unittests.isolate',
    'cc_perftests': 'cc/cc_perftests.isolate',
    'components_unittests': 'components/components_unittests.isolate',
    'content_browsertests': 'content/content_browsertests.isolate',
    'content_unittests': 'content/content_unittests.isolate',
    'media_perftests': 'media/media_perftests.isolate',
    'media_unittests': 'media/media_unittests.isolate',
    'net_unittests': 'net/net_unittests.isolate',
    'sql_unittests': 'sql/sql_unittests.isolate',
    'ui_unittests': 'ui/ui_unittests.isolate',
    'unit_tests': 'chrome/unit_tests.isolate',
    'webkit_unit_tests':
      'third_party/WebKit/Source/web/WebKitUnitTests.isolate',
}

# Paths relative to third_party/webrtc/ (kept separate for readability).
_WEBRTC_ISOLATE_FILE_PATHS = {
    'audio_decoder_unittests':
      'modules/audio_coding/neteq/audio_decoder_unittests.isolate',
    'common_audio_unittests': 'common_audio/common_audio_unittests.isolate',
    'common_video_unittests': 'common_video/common_video_unittests.isolate',
    'modules_tests': 'modules/modules_tests.isolate',
    'modules_unittests': 'modules/modules_unittests.isolate',
    'system_wrappers_unittests':
      'system_wrappers/source/system_wrappers_unittests.isolate',
    'test_support_unittests': 'test/test_support_unittests.isolate',
    'tools_unittests': 'tools/tools_unittests.isolate',
    'video_engine_tests': 'video_engine_tests.isolate',
    'video_engine_core_unittests':
      'video_engine/video_engine_core_unittests.isolate',
    'voice_engine_unittests': 'voice_engine/voice_engine_unittests.isolate',
    'webrtc_perf_tests': 'webrtc_perf_tests.isolate',
}

# Append the WebRTC tests with the full path from Chromium's src/ root.
for webrtc_test, isolate_path in _WEBRTC_ISOLATE_FILE_PATHS.items():
  _ISOLATE_FILE_PATHS[webrtc_test] = 'third_party/webrtc/%s' % isolate_path

# Used for filtering large data deps at a finer grain than what's allowed in
# isolate files since pushing deps to devices is expensive.
# Wildcards are allowed.
_DEPS_EXCLUSION_LIST = [
    'chrome/test/data/extensions/api_test',
    'chrome/test/data/extensions/secure_shell',
    'chrome/test/data/firefox*',
    'chrome/test/data/gpu',
    'chrome/test/data/image_decoding',
    'chrome/test/data/import',
    'chrome/test/data/page_cycler',
    'chrome/test/data/perf',
    'chrome/test/data/pyauto_private',
    'chrome/test/data/safari_import',
    'chrome/test/data/scroll',
    'chrome/test/data/third_party',
    'third_party/hunspell_dictionaries/*.dic',
    # crbug.com/258690
    'webkit/data/bmp_decoder',
    'webkit/data/ico_decoder',
]

_ISOLATE_SCRIPT = os.path.join(
    constants.DIR_SOURCE_ROOT, 'tools', 'swarming_client', 'isolate.py')


def _GenerateDepsDirUsingIsolate(suite_name, isolate_file_path=None):
  """Generate the dependency dir for the test suite using isolate.

  Args:
    suite_name: Name of the test suite (e.g. base_unittests).
    isolate_file_path: .isolate file path to use. If there is a default .isolate
                       file path for the suite_name, this will override it.
  """
  if os.path.isdir(constants.ISOLATE_DEPS_DIR):
    shutil.rmtree(constants.ISOLATE_DEPS_DIR)

  if isolate_file_path:
    if os.path.isabs(isolate_file_path):
      isolate_abs_path = isolate_file_path
    else:
      isolate_abs_path = os.path.join(constants.DIR_SOURCE_ROOT,
                                      isolate_file_path)
  else:
    isolate_rel_path = _ISOLATE_FILE_PATHS.get(suite_name)
    if not isolate_rel_path:
      logging.info('Did not find an isolate file for the test suite.')
      return
    isolate_abs_path = os.path.join(constants.DIR_SOURCE_ROOT, isolate_rel_path)

  isolated_abs_path = os.path.join(
      constants.GetOutDirectory(), '%s.isolated' % suite_name)
  assert os.path.exists(isolate_abs_path)
  # This needs to be kept in sync with the cmd line options for isolate.py
  # in src/build/isolate.gypi.
  isolate_cmd = [
      'python', _ISOLATE_SCRIPT,
      'remap',
      '--isolate', isolate_abs_path,
      '--isolated', isolated_abs_path,
      '--outdir', constants.ISOLATE_DEPS_DIR,

      '--path-variable', 'DEPTH', constants.DIR_SOURCE_ROOT,
      '--path-variable', 'PRODUCT_DIR', constants.GetOutDirectory(),

      '--config-variable', 'OS', 'android',
      '--config-variable', 'CONFIGURATION_NAME', constants.GetBuildType(),
      '--config-variable', 'chromeos', '0',
      '--config-variable', 'component', 'static_library',
      '--config-variable', 'icu_use_data_file_flag', '1',
      # TODO(maruel): This may not be always true.
      '--config-variable', 'target_arch', 'arm',
      '--config-variable', 'use_openssl', '0',
  ]
  assert not cmd_helper.RunCmd(isolate_cmd)

  # We're relying on the fact that timestamps are preserved
  # by the remap command (hardlinked). Otherwise, all the data
  # will be pushed to the device once we move to using time diff
  # instead of md5sum. Perform a sanity check here.
  for root, _, filenames in os.walk(constants.ISOLATE_DEPS_DIR):
    if filenames:
      linked_file = os.path.join(root, filenames[0])
      orig_file = os.path.join(
          constants.DIR_SOURCE_ROOT,
          os.path.relpath(linked_file, constants.ISOLATE_DEPS_DIR))
      if os.stat(linked_file).st_ino == os.stat(orig_file).st_ino:
        break
      else:
        raise Exception('isolate remap command did not use hardlinks.')

  # Delete excluded files as defined by _DEPS_EXCLUSION_LIST.
  old_cwd = os.getcwd()
  try:
    os.chdir(constants.ISOLATE_DEPS_DIR)
    excluded_paths = [x for y in _DEPS_EXCLUSION_LIST for x in glob.glob(y)]
    if excluded_paths:
      logging.info('Excluding the following from dependency list: %s',
                   excluded_paths)
    for p in excluded_paths:
      if os.path.isdir(p):
        shutil.rmtree(p)
      else:
        os.remove(p)
  finally:
    os.chdir(old_cwd)

  # On Android, all pak files need to be in the top-level 'paks' directory.
  paks_dir = os.path.join(constants.ISOLATE_DEPS_DIR, 'paks')
  os.mkdir(paks_dir)

  deps_out_dir = os.path.join(
      constants.ISOLATE_DEPS_DIR,
      os.path.relpath(os.path.join(constants.GetOutDirectory(), os.pardir),
                      constants.DIR_SOURCE_ROOT))
  for root, _, filenames in os.walk(deps_out_dir):
    for filename in fnmatch.filter(filenames, '*.pak'):
      shutil.move(os.path.join(root, filename), paks_dir)

  # Move everything in PRODUCT_DIR to top level.
  deps_product_dir = os.path.join(deps_out_dir, constants.GetBuildType())
  if os.path.isdir(deps_product_dir):
    for p in os.listdir(deps_product_dir):
      shutil.move(os.path.join(deps_product_dir, p), constants.ISOLATE_DEPS_DIR)
    os.rmdir(deps_product_dir)
    os.rmdir(deps_out_dir)


def _GetDisabledTestsFilterFromFile(suite_name):
  """Returns a gtest filter based on the *_disabled file.

  Args:
    suite_name: Name of the test suite (e.g. base_unittests).

  Returns:
    A gtest filter which excludes disabled tests.
    Example: '*-StackTrace.*:StringPrintfTest.StringPrintfMisc'
  """
  filter_file_path = os.path.join(
      os.path.abspath(os.path.dirname(__file__)),
      'filter', '%s_disabled' % suite_name)

  if not filter_file_path or not os.path.exists(filter_file_path):
    logging.info('No filter file found at %s', filter_file_path)
    return '*'

  filters = [x for x in [x.strip() for x in file(filter_file_path).readlines()]
             if x and x[0] != '#']
  disabled_filter = '*-%s' % ':'.join(filters)
  logging.info('Applying filter "%s" obtained from %s',
               disabled_filter, filter_file_path)
  return disabled_filter


def _GetTests(test_options, test_package, devices):
  """Get a list of tests.

  Args:
    test_options: A GTestOptions object.
    test_package: A TestPackageApk object.
    devices: A list of attached devices.

  Returns:
    A list of all the tests in the test suite.
  """
  def TestListerRunnerFactory(device, _shard_index):
    class TestListerRunner(test_runner.TestRunner):
      def RunTest(self, _test):
        result = base_test_result.BaseTestResult(
            'gtest_list_tests', base_test_result.ResultType.PASS)
        self.test_package.Install(self.device)
        result.test_list = self.test_package.GetAllTests(self.device)
        results = base_test_result.TestRunResults()
        results.AddResult(result)
        return results, None
    return TestListerRunner(test_options, device, test_package)

  results, _no_retry = test_dispatcher.RunTests(
      ['gtest_list_tests'], TestListerRunnerFactory, devices)
  tests = []
  for r in results.GetAll():
    tests.extend(r.test_list)
  return tests


def _FilterTestsUsingPrefixes(all_tests, pre=False, manual=False):
  """Removes tests with disabled prefixes.

  Args:
    all_tests: List of tests to filter.
    pre: If True, include tests with PRE_ prefix.
    manual: If True, include tests with MANUAL_ prefix.

  Returns:
    List of tests remaining.
  """
  filtered_tests = []
  filter_prefixes = ['DISABLED_', 'FLAKY_', 'FAILS_']

  if not pre:
    filter_prefixes.append('PRE_')

  if not manual:
    filter_prefixes.append('MANUAL_')

  for t in all_tests:
    test_case, test = t.split('.', 1)
    if not any([test_case.startswith(prefix) or test.startswith(prefix) for
                prefix in filter_prefixes]):
      filtered_tests.append(t)
  return filtered_tests


def _FilterDisabledTests(tests, suite_name, has_gtest_filter):
  """Removes disabled tests from |tests|.

  Applies the following filters in order:
    1. Remove tests with disabled prefixes.
    2. Remove tests specified in the *_disabled files in the 'filter' dir

  Args:
    tests: List of tests.
    suite_name: Name of the test suite (e.g. base_unittests).
    has_gtest_filter: Whether a gtest_filter is provided.

  Returns:
    List of tests remaining.
  """
  tests = _FilterTestsUsingPrefixes(
      tests, has_gtest_filter, has_gtest_filter)
  tests = unittest_util.FilterTestNames(
      tests, _GetDisabledTestsFilterFromFile(suite_name))

  return tests


def Setup(test_options, devices):
  """Create the test runner factory and tests.

  Args:
    test_options: A GTestOptions object.
    devices: A list of attached devices.

  Returns:
    A tuple of (TestRunnerFactory, tests).
  """
  test_package = test_package_apk.TestPackageApk(test_options.suite_name)
  if not os.path.exists(test_package.suite_path):
    test_package = test_package_exe.TestPackageExecutable(
        test_options.suite_name)
    if not os.path.exists(test_package.suite_path):
      raise Exception(
          'Did not find %s target. Ensure it has been built.'
          % test_options.suite_name)
  logging.warning('Found target %s', test_package.suite_path)

  _GenerateDepsDirUsingIsolate(test_options.suite_name,
                               test_options.isolate_file_path)

  tests = _GetTests(test_options, test_package, devices)

  # Constructs a new TestRunner with the current options.
  def TestRunnerFactory(device, _shard_index):
    return test_runner.TestRunner(
        test_options,
        device,
        test_package)

  if test_options.run_disabled:
    test_options = test_options._replace(
        test_arguments=('%s --gtest_also_run_disabled_tests' %
                        test_options.test_arguments))
  else:
    tests = _FilterDisabledTests(tests, test_options.suite_name,
                                 bool(test_options.gtest_filter))
  if test_options.gtest_filter:
    tests = unittest_util.FilterTestNames(tests, test_options.gtest_filter)

  # Coalesce unit tests into a single test per device
  if test_options.suite_name != 'content_browsertests':
    num_devices = len(devices)
    tests = [':'.join(tests[i::num_devices]) for i in xrange(num_devices)]
    tests = [t for t in tests if t]

  return (TestRunnerFactory, tests)
