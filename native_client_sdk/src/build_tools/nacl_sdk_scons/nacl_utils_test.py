#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for nacl_utils.py."""

import fileinput
import mox
import nacl_utils
import os
import sys
import unittest


def TestMock(file_path, open_func):
  temp_file = open_func(file_path)
  temp_file.close()


class TestNaClUtils(unittest.TestCase):
  """Class for test cases to cover globally declared helper functions."""

  def setUp(self):
    self.script_dir = os.path.abspath(os.path.dirname(__file__))
    self.mock_factory = mox.Mox()
    self.InitializeResourceMocks()

  def InitializeResourceMocks(self):
    """Can be called multiple times if multiple functions need to be tested."""
    self.fileinput_mock = self.mock_factory.CreateMock(fileinput)
    self.os_mock = self.mock_factory.CreateMock(os)
    self.sys_mock = self.mock_factory.CreateMock(sys)

  def testToolchainPath(self):
    output = nacl_utils.ToolchainPath('nacl_sdk_root')
    head, tail = os.path.split(output)
    base, toolchain = os.path.split(head)
    self.assertEqual('nacl_sdk_root', base)
    self.assertEqual('toolchain', toolchain)
    self.assertRaises(ValueError,
                      nacl_utils.ToolchainPath,
                      'nacl_sdk_root',
                      arch='nosucharch')
    self.assertRaises(ValueError,
                      nacl_utils.ToolchainPath,
                      'nacl_sdk_root',
                      variant='nosuchvariant')

  def testGetJSONFromNexeSpec(self):
    valid_empty_json = '{\n  "program": {\n  }\n}\n'
    null_json = nacl_utils.GetJSONFromNexeSpec(None)
    self.assertEqual(null_json, valid_empty_json)
    empty_json = nacl_utils.GetJSONFromNexeSpec({})
    self.assertEqual(empty_json, valid_empty_json)
    nexes = {'x86-32': 'nacl_x86_32.nexe',
             'x86-64': 'nacl_x86_64.nexe',
             'arm': 'nacl_ARM.nexe'}
    json = nacl_utils.GetJSONFromNexeSpec(nexes)
    # Assert that the resulting JSON has all the right parts: the "nexes"
    # dict, followed by one entry for each architecture.  Also make sure that
    # the last entry doesn't have a trailing ','
    json_lines = json.splitlines()
    self.assertEqual(len(json_lines), 7)
    self.assertEqual(json_lines[0], '{')
    self.assertEqual(json_lines[1], '  "program": {')
    self.assertTrue(json_lines[2].endswith(','))
    self.assertTrue(json_lines[3].endswith(','))
    self.assertFalse(json_lines[4].endswith(','))
    self.assertEqual(json_lines[5], '  }')
    self.assertEqual(json_lines[6], '}')
    # Assert that the key-value pair lines have the right form.  The order
    # of the keys doesn't matter.  Note that the key values are enclosed in
    # "" (e.g. "x86-32") - this is intentional.
    valid_arch_keys = ['"x86-32"', '"x86-64"', '"arm"']
    for line in json_lines[2:4]:
      key_value = line.split(':')
      self.assertEqual(len(key_value), 3)
      self.assertTrue(key_value[0].lstrip().rstrip() in valid_arch_keys)

  def testGenerateNmf(self):
    # Assert that failure cases properly fail.
    self.assertRaises(ValueError, nacl_utils.GenerateNmf, None, None, None)
    self.assertRaises(ValueError, nacl_utils.GenerateNmf, [], [], {})

  def testGetArchFromSpec(self):
    default_arch, default_subarch = nacl_utils.GetArchFromSpec(None)
    self.assertEqual(default_arch, nacl_utils.DEFAULT_ARCH)
    self.assertEqual(default_subarch, nacl_utils.DEFAULT_SUBARCH)
    default_arch, subarch = nacl_utils.GetArchFromSpec({'subarch': '64'})
    self.assertEqual(default_arch, nacl_utils.DEFAULT_ARCH)
    self.assertEqual(subarch, '64')
    arch, default_subarch = nacl_utils.GetArchFromSpec({'arch': 'x86'})
    self.assertEqual(arch, 'x86')
    self.assertEqual(default_subarch, nacl_utils.DEFAULT_SUBARCH)
    arch, subarch = nacl_utils.GetArchFromSpec({'arch': 'x86', 'subarch': '64'})
    self.assertEqual(arch, 'x86')
    self.assertEqual(subarch, '64')


def RunTests():
  return_value = 1
  test_suite = unittest.TestLoader().loadTestsFromTestCase(TestNaClUtils)
  test_results = unittest.TextTestRunner(verbosity=2).run(test_suite)
  if test_results.wasSuccessful():
    return_value = 0
  return return_value


if __name__ == '__main__':
  sys.exit(RunTests())
