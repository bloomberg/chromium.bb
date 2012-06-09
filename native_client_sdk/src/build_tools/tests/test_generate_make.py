#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import datetime
import os
import posixpath
import subprocess
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(BUILD_TOOLS_DIR)
import generate_make

BASIC_DESC = {
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'hello_world',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.c'],
    },
  ],
  'DEST' : 'examples'
}

class TestFunctions(unittest.TestCase):
  def testPatsubst(self):
    val = generate_make.GenPatsubst(32, 'FOO', 'cc', 'CXX')
    gold = '$(patsubst %.cc,%_32.o,$(FOO_CXX))'
    self.assertEqual(val, gold)

  def testPatsubst(self):
    val = generate_make.GenPatsubst(32, 'FOO', 'cc', 'CXX')
    gold = '$(patsubst %.cc,%_32.o,$(FOO_CXX))'
    self.assertEqual(val, gold)

  def testSetVar(self):
    val = generate_make.SetVar('FOO',[])
    self.assertEqual(val, 'FOO:=\n')

    val = generate_make.SetVar('FOO',['BAR'])
    self.assertEqual(val, 'FOO:=BAR\n')

    items = ['FOO_' + 'x' * (i % 13) for i in range(50)]
    for i in range(10):
      wrapped = generate_make.SetVar('BAR_' + 'x' * i, items)
      lines = wrapped.split('\n')
      for line in lines:
        if len(line) > 79:
          self.assertEqual(line, 'Less than 80 at ' + str(i))


class TestValidateFormat(unittest.TestCase):
  def _append_result(self, msg):
    self.result += msg
    return self.result

  def _validate(self, src, msg):
    format = generate_make.DSC_FORMAT
    self.result = ''
    result = generate_make.ValidateFormat(src, format,
        lambda msg: self._append_result(msg))
    if msg:
      self.assertEqual(self.result, msg)
    else:
      self.assertEqual(result, True)

  def testGoodDesc(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    self._validate(testdesc, None)

  def testMissingKey(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    del testdesc['TOOLS']
    self._validate(testdesc, 'Missing required key TOOLS.')

    testdesc = copy.deepcopy(BASIC_DESC)
    del testdesc['TARGETS'][0]['NAME']
    self._validate(testdesc, 'Missing required key NAME.')

  def testNonEmpty(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TOOLS'] = []
    self._validate(testdesc, 'Expected non-empty value for TOOLS.')

    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TARGETS'] = []
    self._validate(testdesc, 'Expected non-empty value for TARGETS.')

    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TARGETS'][0]['NAME'] = ''
    self._validate(testdesc, 'Expected non-empty value for NAME.')

  def testBadValue(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TOOLS'] = ['newlib', 'glibc', 'badtool']
    self._validate(testdesc, 'Value badtool not expected in TOOLS.')

  def testExpectStr(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TOOLS'] = ['newlib', True, 'glibc']
    self._validate(testdesc, 'Value True not expected in TOOLS.')

  def testExpectList(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TOOLS'] = 'newlib'
    self._validate(testdesc, 'Key TOOLS expects LIST not STR.')

# TODO(noelallen):  Add test which generates a real make and runs it.

def main():
  suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(main())
