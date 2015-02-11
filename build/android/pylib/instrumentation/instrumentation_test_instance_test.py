#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Unit tests for instrumentation.TestRunner."""

# pylint: disable=W0212

import os
import sys
import unittest

from pylib import constants
from pylib.base import base_test_result
from pylib.instrumentation import instrumentation_test_instance

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'pymock'))
import mock  # pylint: disable=F0401


class InstrumentationTestInstanceTest(unittest.TestCase):

  def setUp(self):
    options = mock.Mock()
    options.tool = ''

  def testGenerateTestResult_noStatus(self):
    result = instrumentation_test_instance.GenerateTestResult(
        'test.package.TestClass#testMethod', [], 0, 1000)
    self.assertEqual('test.package.TestClass#testMethod', result.GetName())
    self.assertEqual(base_test_result.ResultType.UNKNOWN, result.GetType())
    self.assertEqual('', result.GetLog())
    self.assertEqual(1000, result.GetDuration())

  def testGenerateTestResult_testPassed(self):
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    result = instrumentation_test_instance.GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.PASS, result.GetType())

  def testGenerateTestResult_testSkipped_first(self):
    statuses = [
      (0, {
        'test_skipped': 'true',
      }),
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    result = instrumentation_test_instance.GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.SKIP, result.GetType())

  def testGenerateTestResult_testSkipped_last(self):
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'test_skipped': 'true',
      }),
    ]
    result = instrumentation_test_instance.GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.SKIP, result.GetType())

  def testGenerateTestResult_testSkipped_false(self):
    statuses = [
      (0, {
        'test_skipped': 'false',
      }),
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    result = instrumentation_test_instance.GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.PASS, result.GetType())

  def testGenerateTestResult_testFailed(self):
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (-2, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    result = instrumentation_test_instance.GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.FAIL, result.GetType())


if __name__ == '__main__':
  unittest.main(verbosity=2)
