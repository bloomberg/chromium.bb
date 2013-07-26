# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates test runner factory and tests for instrumentation tests."""

import logging
import os

from pylib import android_commands
from pylib import constants
from pylib.base import base_test_result
from pylib.utils import report_results

import test_package
import test_runner


def Setup(test_apk_path, test_apk_jar_path, annotations, exclude_annotations,
          test_filter, build_type, test_data, save_perf_json,
          screenshot_failures, tool, wait_for_debugger, disable_assertions,
          push_deps, cleanup_test_files):
  """Create and return the test runner factory and tests.

  Args:
    test_apk_path: Path to the test apk file.
    test_apk_jar_path: Path to the jar associated with the test apk.
    annotations: Annotations for the tests.
    exclude_annotations: Any annotations to exclude from running.
    test_filter: Filter string for tests.
    build_type: 'Release' or 'Debug'.
    test_data: Location of the test data.
    save_perf_json: Whether or not to save the JSON file from UI perf tests.
    screenshot_failures: Take a screenshot for a test failure
    tool: Name of the Valgrind tool.
    wait_for_debugger: blocks until the debugger is connected.
    disable_assertions: Whether to disable java assertions on the device.
    push_deps: If True, push all dependencies to the device.
    cleanup_test_files: Whether or not to cleanup test files on device.

  Returns:
    A tuple of (TestRunnerFactory, tests).
  """
  test_pkg = test_package.TestPackage(test_apk_path, test_apk_jar_path)
  tests = test_pkg._GetAllMatchingTests(annotations, exclude_annotations,
                                        test_filter)
  if not tests:
    logging.error('No instrumentation tests to run with current args.')

  def TestRunnerFactory(device, shard_index):
    return test_runner.TestRunner(
        build_type, test_data, save_perf_json, screenshot_failures,
        tool, wait_for_debugger, disable_assertions, push_deps,
        cleanup_test_files, device, shard_index, test_pkg, [])

  return (TestRunnerFactory, tests)
