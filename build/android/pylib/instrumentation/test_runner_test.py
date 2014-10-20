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
from pylib.instrumentation import test_runner

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'pymock'))
import mock  # pylint: disable=F0401


class InstrumentationTestRunnerTest(unittest.TestCase):

  def setUp(self):
    options = mock.Mock()
    options.tool = ''
    package = mock.Mock()
    self.instance = test_runner.TestRunner(options, None, 0, package)

  def testParseAmInstrumentRawOutput_nothing(self):
    code, result, statuses = (
        test_runner.TestRunner._ParseAmInstrumentRawOutput(['']))
    self.assertEqual(None, code)
    self.assertEqual([], result)
    self.assertEqual([], statuses)

  def testParseAmInstrumentRawOutput_noMatchingStarts(self):
    raw_output = [
      '',
      'this.is.a.test.package.TestClass:.',
      'Test result for =.',
      'Time: 1.234',
      '',
      'OK (1 test)',
    ]

    code, result, statuses = (
        test_runner.TestRunner._ParseAmInstrumentRawOutput(raw_output))
    self.assertEqual(None, code)
    self.assertEqual([], result)
    self.assertEqual([], statuses)

  def testParseAmInstrumentRawOutput_resultAndCode(self):
    raw_output = [
      'INSTRUMENTATION_RESULT: foo',
      'bar',
      'INSTRUMENTATION_CODE: -1',
    ]

    code, result, _ = (
        test_runner.TestRunner._ParseAmInstrumentRawOutput(raw_output))
    self.assertEqual(-1, code)
    self.assertEqual(['foo', 'bar'], result)

  def testParseAmInstrumentRawOutput_oneStatus(self):
    raw_output = [
      'INSTRUMENTATION_STATUS: foo=1',
      'INSTRUMENTATION_STATUS: bar=hello',
      'INSTRUMENTATION_STATUS: world=false',
      'INSTRUMENTATION_STATUS: class=this.is.a.test.package.TestClass',
      'INSTRUMENTATION_STATUS: test=testMethod',
      'INSTRUMENTATION_STATUS_CODE: 0',
    ]

    _, _, statuses = (
        test_runner.TestRunner._ParseAmInstrumentRawOutput(raw_output))

    expected = [
      (0, {
        'foo': ['1'],
        'bar': ['hello'],
        'world': ['false'],
        'class': ['this.is.a.test.package.TestClass'],
        'test': ['testMethod'],
      })
    ]
    self.assertEqual(expected, statuses)

  def testParseAmInstrumentRawOutput_multiStatus(self):
    raw_output = [
      'INSTRUMENTATION_STATUS: class=foo',
      'INSTRUMENTATION_STATUS: test=bar',
      'INSTRUMENTATION_STATUS_CODE: 1',
      'INSTRUMENTATION_STATUS: test_skipped=true',
      'INSTRUMENTATION_STATUS_CODE: 0',
      'INSTRUMENTATION_STATUS: class=hello',
      'INSTRUMENTATION_STATUS: test=world',
      'INSTRUMENTATION_STATUS: stack=',
      'foo/bar.py (27)',
      'hello/world.py (42)',
      'test/file.py (1)',
      'INSTRUMENTATION_STATUS_CODE: -1',
    ]

    _, _, statuses = (
        test_runner.TestRunner._ParseAmInstrumentRawOutput(raw_output))

    expected = [
      (1, {'class': ['foo'], 'test': ['bar'],}),
      (0, {'test_skipped': ['true']}),
      (-1, {
        'class': ['hello'],
        'test': ['world'],
        'stack': ['', 'foo/bar.py (27)', 'hello/world.py (42)',
                  'test/file.py (1)'],
      }),
    ]
    self.assertEqual(expected, statuses)

  def testParseAmInstrumentRawOutput_statusResultAndCode(self):
    raw_output = [
      'INSTRUMENTATION_STATUS: class=foo',
      'INSTRUMENTATION_STATUS: test=bar',
      'INSTRUMENTATION_STATUS_CODE: 1',
      'INSTRUMENTATION_RESULT: hello',
      'world',
      '',
      '',
      'INSTRUMENTATION_CODE: 0',
    ]

    code, result, statuses = (
        test_runner.TestRunner._ParseAmInstrumentRawOutput(raw_output))

    self.assertEqual(0, code)
    self.assertEqual(['hello', 'world', '', ''], result)
    self.assertEqual([(1, {'class': ['foo'], 'test': ['bar']})], statuses)

  def testGenerateTestResult_noStatus(self):
    result = self.instance._GenerateTestResult(
        'test.package.TestClass#testMethod', [], 0, 1000)
    self.assertEqual('test.package.TestClass#testMethod', result.GetName())
    self.assertEqual(base_test_result.ResultType.UNKNOWN, result.GetType())
    self.assertEqual('', result.GetLog())
    self.assertEqual(1000, result.GetDur())

  def testGenerateTestResult_testPassed(self):
    statuses = [
      (1, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
      (0, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
    ]
    result = self.instance._GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.PASS, result.GetType())

  def testGenerateTestResult_testSkipped_first(self):
    statuses = [
      (0, {
        'test_skipped': ['true'],
      }),
      (1, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
      (0, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
    ]
    result = self.instance._GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.SKIP, result.GetType())

  def testGenerateTestResult_testSkipped_last(self):
    statuses = [
      (1, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
      (0, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
      (0, {
        'test_skipped': ['true'],
      }),
    ]
    result = self.instance._GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.SKIP, result.GetType())

  def testGenerateTestResult_testSkipped_false(self):
    statuses = [
      (0, {
        'test_skipped': ['false'],
      }),
      (1, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
      (0, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
    ]
    result = self.instance._GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.PASS, result.GetType())

  def testGenerateTestResult_testFailed(self):
    statuses = [
      (1, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
      (-2, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
    ]
    result = self.instance._GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.FAIL, result.GetType())

  def testGenerateTestResult_testCrashed(self):
    self.instance.test_pkg.GetPackageName = mock.Mock(
        return_value='generate.test.result.test.package')
    self.instance.device.old_interface.DismissCrashDialogIfNeeded = mock.Mock(
        return_value='generate.test.result.test.package')
    statuses = [
      (1, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
      }),
      (-1, {
        'class': ['test.package.TestClass'],
        'test': ['testMethod'],
        'stack': ['', 'foo/bar.py (27)', 'hello/world.py (42)'],
      }),
    ]
    result = self.instance._GenerateTestResult(
        'test.package.TestClass#testMethod', statuses, 0, 1000)
    self.assertEqual(base_test_result.ResultType.CRASH, result.GetType())
    self.assertEqual('\nfoo/bar.py (27)\nhello/world.py (42)', result.GetLog())

  def test_RunTest_verifyAdbShellCommand(self):
    self.instance.options.test_runner = 'MyTestRunner'
    self.instance.device.RunShellCommand = mock.Mock()
    self.instance.test_pkg.GetPackageName = mock.Mock(
        return_value='test.package')
    self.instance._GetInstrumentationArgs = mock.Mock(
        return_value={'test_arg_key': 'test_arg_value'})
    self.instance._RunTest('test.package.TestClass#testMethod', 100)
    self.instance.device.RunShellCommand.assert_called_with(
        ['am', 'instrument', '-r',
         '-e', 'test_arg_key', "'test_arg_value'",
         '-e', 'class', "'test.package.TestClass#testMethod'",
         '-w', 'test.package/MyTestRunner'],
        timeout=100, retries=0)

if __name__ == '__main__':
  unittest.main(verbosity=2)

