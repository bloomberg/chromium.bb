#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from pylib.gtest import test_package

# pylint: disable=W0212


class TestPackageTest(unittest.TestCase):

  def testParseGTestListTests_simple(self):
    raw_output = [
      'TestCaseOne.',
      '  testOne',
      '  testTwo',
      'TestCaseTwo.',
      '  testThree',
      '  testFour',
    ]
    actual = test_package.TestPackage._ParseGTestListTests(raw_output)
    expected = [
      'TestCaseOne.testOne',
      'TestCaseOne.testTwo',
      'TestCaseTwo.testThree',
      'TestCaseTwo.testFour',
    ]
    self.assertEqual(expected, actual)

  def testParseGTestListTests_typeParameterized_old(self):
    raw_output = [
      'TPTestCase/WithTypeParam/0.',
      '  testOne',
      '  testTwo',
    ]
    actual = test_package.TestPackage._ParseGTestListTests(raw_output)
    expected = [
      'TPTestCase/WithTypeParam/0.testOne',
      'TPTestCase/WithTypeParam/0.testTwo',
    ]
    self.assertEqual(expected, actual)

  def testParseGTestListTests_typeParameterized_new(self):
    raw_output = [
      'TPTestCase/WithTypeParam/0.  # TypeParam = TypeParam0',
      '  testOne',
      '  testTwo',
    ]
    actual = test_package.TestPackage._ParseGTestListTests(raw_output)
    expected = [
      'TPTestCase/WithTypeParam/0.testOne',
      'TPTestCase/WithTypeParam/0.testTwo',
    ]
    self.assertEqual(expected, actual)

  def testParseGTestListTests_valueParameterized_old(self):
    raw_output = [
      'VPTestCase.',
      '  testWithValueParam/0',
      '  testWithValueParam/1',
    ]
    actual = test_package.TestPackage._ParseGTestListTests(raw_output)
    expected = [
      'VPTestCase.testWithValueParam/0',
      'VPTestCase.testWithValueParam/1',
    ]
    self.assertEqual(expected, actual)

  def testParseGTestListTests_valueParameterized_new(self):
    raw_output = [
      'VPTestCase.',
      '  testWithValueParam/0  # GetParam() = 0',
      '  testWithValueParam/1  # GetParam() = 1',
    ]
    actual = test_package.TestPackage._ParseGTestListTests(raw_output)
    expected = [
      'VPTestCase.testWithValueParam/0',
      'VPTestCase.testWithValueParam/1',
    ]
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  unittest.main(verbosity=2)

