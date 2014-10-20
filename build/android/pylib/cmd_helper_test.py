# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the cmd_helper module."""

import unittest

from pylib import cmd_helper

# TODO(jbudorick) Make these tests run on the bots.


class CmdHelperSingleQuoteTest(unittest.TestCase):

  def testSingleQuote_basic(self):
    self.assertEquals('hello',
                      cmd_helper.SingleQuote('hello'))

  def testSingleQuote_withSpaces(self):
    self.assertEquals("'hello world'",
                      cmd_helper.SingleQuote('hello world'))

  def testSingleQuote_withUnsafeChars(self):
    self.assertEquals("""'hello'"'"'; rm -rf /'""",
                      cmd_helper.SingleQuote("hello'; rm -rf /"))

  def testSingleQuote_dontExpand(self):
    test_string = 'hello $TEST_VAR'
    cmd = 'TEST_VAR=world; echo %s' % cmd_helper.SingleQuote(test_string)
    self.assertEquals(test_string,
                      cmd_helper.GetCmdOutput(cmd, shell=True).rstrip())


class CmdHelperDoubleQuoteTest(unittest.TestCase):

  def testDoubleQuote_basic(self):
    self.assertEquals('hello',
                      cmd_helper.DoubleQuote('hello'))

  def testDoubleQuote_withSpaces(self):
    self.assertEquals('"hello world"',
                      cmd_helper.DoubleQuote('hello world'))

  def testDoubleQuote_withUnsafeChars(self):
    self.assertEquals('''"hello\\"; rm -rf /"''',
                      cmd_helper.DoubleQuote('hello"; rm -rf /'))

  def testSingleQuote_doExpand(self):
    test_string = 'hello $TEST_VAR'
    cmd = 'TEST_VAR=world; echo %s' % cmd_helper.DoubleQuote(test_string)
    self.assertEquals('hello world',
                      cmd_helper.GetCmdOutput(cmd, shell=True).rstrip())
