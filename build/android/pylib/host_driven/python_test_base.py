# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for Android Python-driven tests.

This test case is intended to serve as the base class for any Python-driven
tests. It is similar to the Python unitttest module in that the user's tests
inherit from this case and add their tests in that case.

When a PythonTestBase object is instantiated, its purpose is to run only one of
its tests. The test runner gives it the name of the test the instance will
run. The test runner calls SetUp with the Android device ID which the test will
run against. The runner runs the test method itself, collecting the result,
and calls TearDown.

Tests can basically do whatever they want in the test methods, such as call
Java tests using _RunJavaTests. Those methods have the advantage of massaging
the Java test results into Python test results.
"""

import logging
import os
import time

from pylib import android_commands
from pylib.base import base_test_result
from pylib.instrumentation import test_package
from pylib.instrumentation import test_result
from pylib.instrumentation import test_runner


# aka the parent of com.google.android
BASE_ROOT = 'src' + os.sep


class PythonTestBase(object):
  """Base class for Python-driven tests."""

  def __init__(self, test_name):
    # test_name must match one of the test methods defined on a subclass which
    # inherits from this class.
    # It's stored so we can do the attr lookup on demand, allowing this class
    # to be pickled, a requirement for the multiprocessing module.
    self.test_name = test_name
    class_name = self.__class__.__name__
    self.qualified_name = class_name + '.' + self.test_name

  def SetUp(self, options):
    self.options = options
    self.shard_index = self.options.shard_index
    self.device_id = self.options.device_id
    self.adb = android_commands.AndroidCommands(self.device_id)
    self.ports_to_forward = []

  def TearDown(self):
    pass

  def GetOutDir(self):
    return os.path.join(os.environ['CHROME_SRC'], 'out',
        self.options.build_type)

  def Run(self):
    logging.warning('Running Python-driven test: %s', self.test_name)
    return getattr(self, self.test_name)()

  def _RunJavaTest(self, fname, suite, test):
    """Runs a single Java test with a Java TestRunner.

    Args:
      fname: filename for the test (e.g. foo/bar/baz/tests/FooTest.py)
      suite: name of the Java test suite (e.g. FooTest)
      test: name of the test method to run (e.g. testFooBar)

    Returns:
      TestRunResults object with a single test result.
    """
    test = self._ComposeFullTestName(fname, suite, test)
    test_pkg = test_package.TestPackage(
        self.options.test_apk_path, self.options.test_apk_jar_path)
    java_test_runner = test_runner.TestRunner(self.options, self.device_id,
                                              self.shard_index, False,
                                              test_pkg,
                                              self.ports_to_forward)
    try:
      java_test_runner.SetUp()
      return java_test_runner.RunTest(test)[0]
    finally:
      java_test_runner.TearDown()

  def _RunJavaTests(self, fname, tests):
    """Calls a list of tests and stops at the first test failure.

    This method iterates until either it encounters a non-passing test or it
    exhausts the list of tests. Then it returns the appropriate Python result.

    Args:
      fname: filename for the Python test
      tests: a list of Java test names which will be run

    Returns:
      A TestRunResults object containing a result for this Python test.
    """
    test_type = base_test_result.ResultType.PASS
    log = ''

    start_ms = int(time.time()) * 1000
    for test in tests:
      # We're only running one test at a time, so this TestRunResults object
      # will hold only one result.
      suite, test_name = test.split('.')
      java_results = self._RunJavaTest(fname, suite, test_name)
      assert len(java_results.GetAll()) == 1
      if not java_results.DidRunPass():
        result = java_results.GetNotPass().pop()
        log = result.GetLog()
        test_type = result.GetType()
        break
    duration_ms = int(time.time()) * 1000 - start_ms

    python_results = base_test_result.TestRunResults()
    python_results.AddResult(
        test_result.InstrumentationTestResult(
            self.qualified_name, test_type, start_ms, duration_ms, log=log))
    return python_results

  def _ComposeFullTestName(self, fname, suite, test):
    package_name = self._GetPackageName(fname)
    return package_name + '.' + suite + '#' + test

  def _GetPackageName(self, fname):
    """Extracts the package name from the test file path."""
    dirname = os.path.dirname(fname)
    package = dirname[dirname.rfind(BASE_ROOT) + len(BASE_ROOT):]
    return package.replace(os.sep, '.')
