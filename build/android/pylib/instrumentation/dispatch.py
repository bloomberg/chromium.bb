# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dispatches the instrumentation tests."""

import logging
import os

from pylib import android_commands
from pylib.base import shard
from pylib.base import test_result
from pylib.uiautomator import test_package as uiautomator_package

import test_package
import test_runner


def Dispatch(options):
  """Dispatches instrumentation tests onto connected device(s).

  If possible, this method will attempt to shard the tests to
  all connected devices. Otherwise, dispatch and run tests on one device.

  Args:
    options: Command line options.

  Returns:
    A TestResults object holding the results of the Java tests.

  Raises:
    Exception: when there are no attached devices.
  """
  is_uiautomator_test = False
  if hasattr(options, 'uiautomator_jar'):
    test_pkg = uiautomator_package.TestPackage(
        options.uiautomator_jar, options.uiautomator_info_jar)
    is_uiautomator_test = True
  else:
    test_pkg = test_package.TestPackage(options.test_apk_path,
                                        options.test_apk_jar_path)
  # The default annotation for tests which do not have any sizes annotation.
  default_size_annotation = 'SmallTest'

  def _GetTestsMissingAnnotation(test_pkg):
    test_size_annotations = frozenset(['Smoke', 'SmallTest', 'MediumTest',
                                       'LargeTest', 'EnormousTest', 'FlakyTest',
                                       'DisabledTest', 'Manual', 'PerfTest'])
    tests_missing_annotations = []
    for test_method in test_pkg.GetTestMethods():
      annotations = frozenset(test_pkg.GetTestAnnotations(test_method))
      if (annotations.isdisjoint(test_size_annotations) and
          not test_pkg.IsPythonDrivenTest(test_method)):
        tests_missing_annotations.append(test_method)
    return sorted(tests_missing_annotations)

  if options.annotation:
    available_tests = test_pkg.GetAnnotatedTests(options.annotation)
    if options.annotation.count(default_size_annotation) > 0:
      tests_missing_annotations = _GetTestsMissingAnnotation(test_pkg)
      if tests_missing_annotations:
        logging.warning('The following tests do not contain any annotation. '
                        'Assuming "%s":\n%s',
                        default_size_annotation,
                        '\n'.join(tests_missing_annotations))
        available_tests += tests_missing_annotations
  else:
    available_tests = [m for m in test_pkg.GetTestMethods()
                       if not test_pkg.IsPythonDrivenTest(m)]
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
    logging.warning('No instrumentation tests to run with current args.')
    return test_result.TestResults()

  tests *= options.number_of_runs

  attached_devices = android_commands.GetAttachedDevices()

  if not attached_devices:
    raise Exception('There are no devices online.')
  if options.device:
    attached_devices = [options.device]

  logging.info('Will run: %s', str(tests))

  if len(attached_devices) > 1 and (coverage or options.wait_for_debugger):
    logging.warning('Coverage / debugger can not be sharded, '
                    'using first available device')
    attached_devices = attached_devices[:1]

  def TestRunnerFactory(device, shard_index):
    return test_runner.TestRunner(
        options, device, shard_index, False, test_pkg, [], is_uiautomator_test)

  return shard.ShardAndRunTests(TestRunnerFactory, attached_devices, tests,
                                options.build_type)
