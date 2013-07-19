#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from app_yaml_helper import AppYamlHelper
from file_system import FileNotFoundError
from host_file_system_creator import HostFileSystemCreator
from mock_file_system import MockFileSystem
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem
from test_util import DisableLogging

_ExtractVersion, _IsGreater, _GenerateAppYaml = (
    AppYamlHelper.ExtractVersion,
    AppYamlHelper.IsGreater,
    AppYamlHelper.GenerateAppYaml)

class AppYamlHelperTest(unittest.TestCase):
  def testExtractVersion(self):
    def run_test(version):
      self.assertEqual(version, _ExtractVersion(_GenerateAppYaml(version)))
    run_test('0')
    run_test('0-0')
    run_test('0-0-0')
    run_test('1')
    run_test('1-0')
    run_test('1-0-0')
    run_test('1-0-1')
    run_test('1-1-0')
    run_test('1-1-1')
    run_test('2-0-9')
    run_test('2-0-12')
    run_test('2-1')
    run_test('2-1-0')
    run_test('2-11-0')
    run_test('3-1-0')
    run_test('3-1-3')
    run_test('3-12-0')

  def testIsGreater(self):
    def assert_is_greater(lhs, rhs):
      self.assertTrue(_IsGreater(lhs, rhs), '%s is not > %s' % (lhs, rhs))
      self.assertFalse(_IsGreater(rhs, lhs),
                       '%s should not be > %s' % (rhs, lhs))
    assert_is_greater('0-0', '0')
    assert_is_greater('0-0-0', '0')
    assert_is_greater('0-0-0', '0-0')
    assert_is_greater('1', '0')
    assert_is_greater('1', '0-0')
    assert_is_greater('1', '0-0-0')
    assert_is_greater('1-0', '0-0')
    assert_is_greater('1-0-0-0', '0-0-0')
    assert_is_greater('2-0-12', '2-0-9')
    assert_is_greater('2-0-12', '2-0-9-0')
    assert_is_greater('2-0-12-0', '2-0-9')
    assert_is_greater('2-0-12-0', '2-0-9-0')
    assert_is_greater('2-1', '2-0-9')
    assert_is_greater('2-1', '2-0-12')
    assert_is_greater('2-1-0', '2-0-9')
    assert_is_greater('2-1-0', '2-0-12')
    assert_is_greater('3-1-0', '2-1')
    assert_is_greater('3-1-0', '2-1-0')
    assert_is_greater('3-1-0', '2-11-0')
    assert_is_greater('3-1-3', '3-1-0')
    assert_is_greater('3-12-0', '3-1-0')
    assert_is_greater('3-12-0', '3-1-3')
    assert_is_greater('3-12-0', '3-1-3-0')

  @DisableLogging('warning')
  def testInstanceMethods(self):
    test_data = {
      'server2': {
        'app.yaml': _GenerateAppYaml('1-0'),
        'app_yaml_helper.py': 'Copyright notice etc'
      }
    }

    updates = []

    file_system_at_head = MockFileSystem(TestFileSystem(test_data))

    def apply_update(update):
      file_system_at_head.Update(update)
      updates.append(update)

    def constructor(branch, revision=None):
      return MockFileSystem.Create(TestFileSystem(test_data),
                                   updates[:revision])
    host_file_system_creator = HostFileSystemCreator(
        ObjectStoreCreator.ForTest(),
        constructor_for_test=constructor)
    helper = AppYamlHelper('server2/app.yaml',
                           file_system_at_head,
                           ObjectStoreCreator.ForTest(disable_wrappers=False),
                           host_file_system_creator)

    def assert_is_up_to_date(version):
      self.assertTrue(helper.IsUpToDate(version),
                      '%s is not up to date' % version)
      self.assertRaises(ValueError,
                        helper.GetFirstRevisionGreaterThan, version)

    self.assertEqual(0, helper.GetFirstRevisionGreaterThan('0-5-0'))
    assert_is_up_to_date('1-0-0')
    assert_is_up_to_date('1-5-0')

    # Revision 1.
    apply_update({'server2': {
      'app.yaml': _GenerateAppYaml('1-5-0')
    }})

    self.assertEqual(0, helper.GetFirstRevisionGreaterThan('0-5-0'))
    self.assertEqual(1, helper.GetFirstRevisionGreaterThan('1-0-0'))
    assert_is_up_to_date('1-5-0')
    assert_is_up_to_date('2-5-0')

    # Revision 2.
    apply_update({'server2': {
      'app_yaml_helper.py': 'fixed a bug'
    }})

    self.assertEqual(0, helper.GetFirstRevisionGreaterThan('0-5-0'))
    self.assertEqual(1, helper.GetFirstRevisionGreaterThan('1-0-0'))
    assert_is_up_to_date('1-5-0')
    assert_is_up_to_date('2-5-0')

    # Revision 3.
    apply_update({'server2': {
      'app.yaml': _GenerateAppYaml('1-6-0')
    }})

    self.assertEqual(0, helper.GetFirstRevisionGreaterThan('0-5-0'))
    self.assertEqual(1, helper.GetFirstRevisionGreaterThan('1-0-0'))
    self.assertEqual(3, helper.GetFirstRevisionGreaterThan('1-5-0'))
    assert_is_up_to_date('2-5-0')

    # Revision 4.
    apply_update({'server2': {
      'app.yaml': _GenerateAppYaml('1-8-0')
    }})
    # Revision 5.
    apply_update({'server2': {
      'app.yaml': _GenerateAppYaml('2-0-0')
    }})
    # Revision 6.
    apply_update({'server2': {
      'app.yaml': _GenerateAppYaml('2-2-0')
    }})
    # Revision 7.
    apply_update({'server2': {
      'app.yaml': _GenerateAppYaml('2-4-0')
    }})
    # Revision 8.
    apply_update({'server2': {
      'app.yaml': _GenerateAppYaml('2-6-0')
    }})

    self.assertEqual(0, helper.GetFirstRevisionGreaterThan('0-5-0'))
    self.assertEqual(1, helper.GetFirstRevisionGreaterThan('1-0-0'))
    self.assertEqual(3, helper.GetFirstRevisionGreaterThan('1-5-0'))
    self.assertEqual(5, helper.GetFirstRevisionGreaterThan('1-8-0'))
    self.assertEqual(6, helper.GetFirstRevisionGreaterThan('2-0-0'))
    self.assertEqual(6, helper.GetFirstRevisionGreaterThan('2-1-0'))
    self.assertEqual(7, helper.GetFirstRevisionGreaterThan('2-2-0'))
    self.assertEqual(7, helper.GetFirstRevisionGreaterThan('2-3-0'))
    self.assertEqual(8, helper.GetFirstRevisionGreaterThan('2-4-0'))
    self.assertEqual(8, helper.GetFirstRevisionGreaterThan('2-5-0'))
    assert_is_up_to_date('2-6-0')
    assert_is_up_to_date('2-7-0')

if __name__ == '__main__':
  unittest.main()
