#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import html_checker
from os import path as os_path
import re
from sys import path as sys_path
import test_util
import unittest

_HERE = os_path.dirname(os_path.abspath(__file__))
sys_path.append(os_path.join(_HERE, '..', '..', '..', 'build'))

import find_depot_tools  # pylint: disable=W0611
from testing_support.super_mox import SuperMoxTestBase


class HtmlCheckerTest(SuperMoxTestBase):
  def setUp(self):
    SuperMoxTestBase.setUp(self)

    input_api = self.mox.CreateMockAnything()
    input_api.re = re
    output_api = self.mox.CreateMockAnything()
    self.checker = html_checker.HtmlChecker(input_api, output_api)

  def ShouldFailCheck(self, line, checker):
    """Checks that the |checker| flags |line| as a style error."""
    error = checker(1, line)
    self.assertNotEqual('', error, 'Should be flagged as style error: ' + line)
    highlight = test_util.GetHighlight(line, error).strip()

  def ShouldPassCheck(self, line, checker):
    """Checks that the |checker| doesn't flag |line| as a style error."""
    error = checker(1, line)
    self.assertEqual('', error, 'Should not be flagged as style error: ' + line)

  def testClassesUseDashFormCheckFails(self):
    lines = [
      ' <a class="Foo-bar" href="classBar"> ',
      '<b class="foo-Bar"> ',
      '<i class="foo_bar" >',
      ' <hr class="fooBar"> ',
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.ClassesUseDashFormCheck)

  def testClassesUseDashFormCheckPasses(self):
    lines = [
      ' class="abc" ',
      'class="foo-bar"',
      '<div class="foo-bar" id="classBar"',
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.ClassesUseDashFormCheck)

  def testDoNotCloseSingleTagsCheckFails(self):
    lines = [
      "<input/>",
      ' <input id="a" /> ',
      "<div/>",
      "<br/>",
      "<br />",
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.DoNotCloseSingleTagsCheck)

  def testDoNotCloseSingleTagsCheckPasses(self):
    lines = [
      "<input>",
      "<link>",
      "<div></div>",
      '<input text="/">',
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.DoNotCloseSingleTagsCheck)

  def testDoNotUseBrElementCheckFails(self):
    lines = [
      " <br>",
      "<br  >  ",
      "<br\>",
      '<br name="a">',
    ]
    for line in lines:
      self.ShouldFailCheck(
          line, self.checker.DoNotUseBrElementCheck)

  def testDoNotUseBrElementCheckPasses(self):
    lines = [
      "br",
      "br>",
      "give me a break"
    ]
    for line in lines:
      self.ShouldPassCheck(
          line, self.checker.DoNotUseBrElementCheck)

  def testDoNotUseInputTypeButtonCheckFails(self):
    lines = [
      '<input type="button">',
      ' <input id="a" type="button" >',
      '<input type="button" id="a"> ',
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.DoNotUseInputTypeButtonCheck)

  def testDoNotUseInputTypeButtonCheckPasses(self):
    lines = [
      "<input>",
      '<input type="text">',
      '<input type="result">',
      '<input type="submit">',
      "<button>",
      '<button type="button">',
      '<button type="reset">',
      '<button type="submit">',

    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.DoNotUseInputTypeButtonCheck)

  def testI18nContentJavaScriptCaseCheckFails(self):
    lines = [
      ' i18n-content="foo-bar" ',
      'i18n-content="foo_bar"',
      'i18n-content="FooBar"',
      'i18n-content="_foo"',
      'i18n-content="foo_"',
      'i18n-content="-foo"',
      'i18n-content="foo-"',
      'i18n-content="Foo"',
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.I18nContentJavaScriptCaseCheck)

  def testI18nContentJavaScriptCaseCheckPasses(self):
    lines = [
      ' i18n-content="abc" ',
      'i18n-content="fooBar"',
      'i18n-content="validName" attr="invalidName_"',
      '<div i18n-content="exampleTitle"',
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.I18nContentJavaScriptCaseCheck)

  def testLabelCheckFails(self):
    lines = [
      ' for="abc"',
      "for=    ",
      " \tfor=    ",
      "   for="
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.LabelCheck)

  def testLabelCheckPass(self):
    lines = [
      ' my-for="abc" ',
      ' myfor="abc" ',
      " <for",
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.LabelCheck)


if __name__ == '__main__':
  unittest.main()
