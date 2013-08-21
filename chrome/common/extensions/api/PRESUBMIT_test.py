#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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


class JSONParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    input_json = '''
// This is a comment.
{
  "key1": ["value1", "value2"],
  "key2": 3  // This is an inline comment.
}
'''
    self.assertEqual(None,
                     PRESUBMIT._GetJSONParseError(input_api, input_json))

  def testFailure(self):
    input_api = MockInputApi()
    input_json = '{ x }'
    self.assertEqual('Expecting property name: line 1 column 2 (char 2)',
                     str(PRESUBMIT._GetJSONParseError(input_api, input_json)))

    input_json = '{ "hello": "world }'
    self.assertEqual(
        'Unterminated string starting at: line 1 column 11 (char 11)',
        str(PRESUBMIT._GetJSONParseError(input_api, input_json)))

    input_json = '{ "a": "b", "c": "d", }'
    self.assertEqual(
        'Expecting property name: line 1 column 22 (char 22)',
        str(PRESUBMIT._GetJSONParseError(input_api, input_json)))

    input_json = '{ "a": "b" "c": "d" }'
    self.assertEqual(
        'Expecting , delimiter: line 1 column 11 (char 11)',
        str(PRESUBMIT._GetJSONParseError(input_api, input_json)))


if __name__ == "__main__":
  unittest.main()
