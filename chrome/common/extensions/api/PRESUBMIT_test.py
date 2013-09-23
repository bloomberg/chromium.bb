#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import json
import os
import subprocess
import sys
import unittest

import PRESUBMIT


class MockInputApi(object):
  def __init__(self):
    self.json = json
    self.os_path = os.path
    self.subprocess = subprocess
    self.python_executable = sys.executable

  def PresubmitLocalPath(self):
    return os.path.dirname(__file__)

  def ReadFile(self, filename, mode='rU'):
    with open(filename, mode=mode) as f:
      return f.read()


class JSONParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    filename = 'test_presubmit/valid_json.json'
    self.assertEqual(None,
                     PRESUBMIT._GetJSONParseError(input_api, filename))

  def testFailure(self):
    input_api = MockInputApi()
    expected_errors = [
      'Expecting property name: line 8 column 3 (char 9)',
      'Invalid control character at: line 8 column 19 (char 25)',
      'Expecting property name: line 8 column 23 (char 29)',
      'Expecting , delimiter: line 8 column 12 (char 18)',
    ]
    actual_errors = [
      str(PRESUBMIT._GetJSONParseError(input_api, filename))
      for filename in sorted(glob.glob('test_presubmit/invalid_*.json'))
    ]
    self.assertEqual(expected_errors, actual_errors)


class IDLParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    filename = 'test_presubmit/valid_idl_basics.idl'
    self.assertEqual(None,
                     PRESUBMIT._GetIDLParseError(input_api, filename))

  def testFailure(self):
    input_api = MockInputApi()
    expected_errors = [
      'Unexpected "{" after keyword "dictionary".',
      'Unexpected symbol DOMString after symbol a.',
      'Unexpected symbol name2 after symbol name1.',
      'Trailing comma in block.',
      'Unexpected ";" after "(".',
      'Unexpected ")" after symbol long.',
      'Unexpected symbol Events after symbol interace.',
      'Did not process Interface Interface(NotEvent)',
      'Interface missing name.',
    ]
    actual_errors = [
      PRESUBMIT._GetIDLParseError(input_api, filename)
      for filename in sorted(glob.glob('test_presubmit/invalid_*.idl'))
    ]
    for (expected_error, actual_error) in zip(expected_errors, actual_errors):
      self.assertTrue(expected_error in actual_error)


if __name__ == "__main__":
  unittest.main()
