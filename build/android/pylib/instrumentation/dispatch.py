# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dispatches the instrumentation tests."""

import logging
import os

from pylib import android_commands
from pylib.base import test_result

import apk_info
import test_sharder


def Dispatch(options, apks):
  """Dispatches instrumentation tests onto connected device(s).

  If possible, this method will attempt to shard the tests to
  all connected devices. Otherwise, dispatch and run tests on one device.

  Args:
    options: Command line options.
    apks: list of APKs to use.

  Returns:
    A TestResults object holding the results of the Java tests.

  Raises:
    Exception: when there are no attached devices.
  """
  test_apk = apks[0]
  # The default annotation for tests which do not have any sizes annotation.
  default_size_annotation = 'SmallTest'

  def _GetTestsMissingAnnotation(test_apk):
    test_size_annotations = frozenset(['Smoke', 'SmallTest', 'MediumTest',
                                       'LargeTest', 'EnormousTest', 'FlakyTest',
                                       'DisabledTest', 'Manual', 'PerfTest'])
    tests_missing_annotations = []
    for test_method in test_apk.GetTestMethods():
      annotations = frozenset(test_apk.GetTestAnnotations(test_method))
      if (annotations.isdisjoint(test_size_annotations) and
          not apk_info.ApkInfo.IsPythonDrivenTest(test_method)):
        tests_missing_annotations.append(test_method)
    return sorted(tests_missing_annotations)

  if options.annotation:
    available_tests = test_apk.GetAnnotatedTests(options.annotation)
    if options.annotation.count(default_size_annotation) > 0:
      tests_missing_annotations = _GetTestsMissingAnnotation(test_apk)
      if tests_missing_annotations:
        logging.warning('The following tests do not contain any annotation. '
                        'Assuming "%s":\n%s',
                        default_size_annotation,
                        '\n'.join(tests_missing_annotations))
        available_tests += tests_missing_annotations
  else:
    available_tests = [m for m in test_apk.GetTestMethods()
                       if not apk_info.ApkInfo.IsPythonDrivenTest(m)]
  coverage = os.environ.get('EMMA_INSTRUMENT') == 'true'

  tests = []
  if options.test_filter:
    # |available_tests| are in adb instrument format: package.path.class#test.
    filter_without_hash = options.test_filter.replace('#', '.')
    tests = [t for t in available_tests
             if filter_without_hash in t.replace('#', '.')]
  else:
    tests = available_tests

  if not tests:
    logging.warning('No Java tests to run with current args.')
    return test_result.TestResults()

  tests *= options.number_of_runs

  attached_devices = android_commands.GetAttachedDevices()
  test_results = test_result.TestResults()

  if not attached_devices:
    raise Exception('You have no devices attached or visible!')
  if options.device:
    attached_devices = [options.device]

  logging.info('Will run: %s', str(tests))

  if len(attached_devices) > 1 and (coverage or options.wait_for_debugger):
    logging.warning('Coverage / debugger can not be sharded, '
                    'using first available device')
    attached_devices = attached_devices[:1]
  sharder = test_sharder.TestSharder(attached_devices, options, tests, apks)
  test_results = sharder.RunShardedTests()
  return test_results
