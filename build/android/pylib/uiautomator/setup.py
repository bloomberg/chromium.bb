# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates test runner factory and tests for uiautomator tests."""

import logging
import os

from pylib import android_commands
from pylib import constants
from pylib.base import base_test_result
from pylib.utils import report_results

import test_package
import test_runner


def Setup(uiautomator_jar, uiautomator_info_jar, annotations,
          exclude_annotations, test_filter, package_name, build_type, test_data,
          save_perf_json, screenshot_failures, tool, disable_assertions,
          push_deps, cleanup_test_files):
  """Runs uiautomator tests on connected device(s).

  Args:
    uiautomator_jar: Location of the jar file with the uiautomator test suite.
    uiautomator_info_jar: Info jar accompanying the jar.
    annotations: Annotations for the tests.
    exclude_annotations: Any annotations to exclude from running.
    test_filter: Filter string for tests.
    package_name: Application package name under test.
    build_type: 'Release' or 'Debug'.
    test_data: Location of the test data.
    save_perf_json: Whether or not to save the JSON file from UI perf tests.
    screenshot_failures: Take a screenshot for a test failure
    tool: Name of the Valgrind tool.
    disable_assertions: Whether to disable java assertions on the device.
    push_deps: If True, push all dependencies to the device.
    cleanup_test_files: Whether or not to cleanup test files on device.

  Returns:
    A tuple of (TestRunnerFactory, tests).
  """
  test_pkg = test_package.TestPackage(
      uiautomator_jar, uiautomator_info_jar)
  tests = test_pkg._GetAllMatchingTests(
      annotations, exclude_annotations, test_filter)

  if not tests:
    logging.error('No uiautomator tests to run with current args.')

  def TestRunnerFactory(device, shard_index):
    return test_runner.TestRunner(
        package_name, build_type, test_data, save_perf_json,
        screenshot_failures, tool, False, disable_assertions, push_deps,
        cleanup_test_files, device, shard_index, test_pkg, [])

  return (TestRunnerFactory, tests)
