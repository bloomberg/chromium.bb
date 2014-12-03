# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
import sys

from pylib import constants
from pylib.base import test_instance

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'build', 'util', 'lib', 'common'))
import unittest_util


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


class GtestTestInstance(test_instance.TestInstance):

  def __init__(self, options, isolate_delegate):
    super(GtestTestInstance, self).__init__()
    self._apk_path = os.path.join(
        constants.GetOutDirectory(), '%s_apk' % options.suite_name,
        '%s-debug.apk' % options.suite_name)
    self._data_deps = []
    self._gtest_filter = options.test_filter
    if options.isolate_file_path:
      self._isolate_abs_path = os.path.abspath(options.isolate_file_path)
      self._isolate_delegate = isolate_delegate
      self._isolated_abs_path = os.path.join(
          constants.GetOutDirectory(), '%s.isolated' % options.suite_name)
    else:
      logging.warning('No isolate file provided. No data deps will be pushed.');
      self._isolate_delegate = None
    self._suite = options.suite_name

  #override
  def TestType(self):
    return 'gtest'

  #override
  def SetUp(self):
    """Map data dependencies via isolate."""
    if self._isolate_delegate:
      self._isolate_delegate.Remap(
          self._isolate_abs_path, self._isolated_abs_path)
      self._isolate_delegate.PurgeExcluded(_DEPS_EXCLUSION_LIST)
      self._isolate_delegate.MoveOutputDeps()
      self._data_deps.extend([(constants.ISOLATE_DEPS_DIR, None)])

  def GetDataDependencies(self):
    """Returns the test suite's data dependencies.

    Returns:
      A list of (host_path, device_path) tuples to push. If device_path is
      None, the client is responsible for determining where to push the file.
    """
    return self._data_deps

  def FilterTests(self, test_list, disabled_prefixes=None):
    """Filters |test_list| based on prefixes and, if present, a filter string.

    Args:
      test_list: The list of tests to filter.
      disabled_prefixes: A list of test prefixes to filter. Defaults to
        DISABLED_, FLAKY_, FAILS_, PRE_, and MANUAL_
    Returns:
      A filtered list of tests to run.
    """
    gtest_filter_strings = [
        self._GenerateDisabledFilterString(disabled_prefixes)]
    if self._gtest_filter:
      gtest_filter_strings.append(self._gtest_filter)

    filtered_test_list = test_list
    for gtest_filter_string in gtest_filter_strings:
      logging.info('gtest filter string: %s' % gtest_filter_string)
      filtered_test_list = unittest_util.FilterTestNames(
          filtered_test_list, gtest_filter_string)
    logging.info('final list of tests: %s' % (str(filtered_test_list)))
    return filtered_test_list

  def _GenerateDisabledFilterString(self, disabled_prefixes):
    disabled_filter_items = []

    if disabled_prefixes is None:
      disabled_prefixes = ['DISABLED_', 'FLAKY_', 'FAILS_', 'PRE_', 'MANUAL_']
    disabled_filter_items += ['%s*' % dp for dp in disabled_prefixes]

    disabled_tests_file_path = os.path.join(
        constants.DIR_SOURCE_ROOT, 'build', 'android', 'pylib', 'gtest',
        'filter', '%s_disabled' % self._suite)
    if disabled_tests_file_path and os.path.exists(disabled_tests_file_path):
      with open(disabled_tests_file_path) as disabled_tests_file:
        disabled_filter_items += [
            '%s' % l for l in (line.strip() for line in disabled_tests_file)
            if l and not l.startswith('#')]

    return '*-%s' % ':'.join(disabled_filter_items)

  #override
  def TearDown(self):
    """Clear the mappings created by SetUp."""
    if self._isolate_delegate:
      self._isolate_delegate.Clear()

  @property
  def apk(self):
    return self._apk_path

  @property
  def suite(self):
    return self._suite

