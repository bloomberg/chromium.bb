#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for Web Development Style Guide checker."""

import os
import re
import sys
import unittest

test_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.extend([
    os.path.normpath(os.path.join(test_dir, '..', '..', '..', 'tools')),
    os.path.join(test_dir),
])

import find_depot_tools # pylint: disable=W0611
from testing_support.super_mox import SuperMoxTestBase
from web_dev_style import css_checker # pylint: disable=F0401


class WebDevStyleGuideTest(SuperMoxTestBase):
  def setUp(self):
    SuperMoxTestBase.setUp(self)

    self.fake_file_name = 'fake.css'

    self.fake_file = self.mox.CreateMockAnything()
    self.mox.StubOutWithMock(self.fake_file, 'LocalPath')
    self.fake_file.LocalPath().AndReturn(self.fake_file_name)
    # Actual calls to NewContents() are defined in each test.
    self.mox.StubOutWithMock(self.fake_file, 'NewContents')

    self.input_api = self.mox.CreateMockAnything()
    self.input_api.re = re
    self.mox.StubOutWithMock(self.input_api, 'AffectedSourceFiles')
    self.input_api.AffectedFiles(
        include_deletes=False, file_filter=None).AndReturn([self.fake_file])

    # Actual creations of PresubmitError are defined in each test.
    self.output_api = self.mox.CreateMockAnything()
    self.mox.StubOutWithMock(self.output_api, 'PresubmitError',
                             use_mock_anything=True)

    author_msg = ('Was the CSS checker useful? '
                  'Send feedback or hate mail to dbeam@chromium.org.')
    self.output_api = self.mox.CreateMockAnything()
    self.mox.StubOutWithMock(self.output_api, 'PresubmitNotifyResult',
                             use_mock_anything=True)
    self.output_api.PresubmitNotifyResult(author_msg).AndReturn(None)

  def VerifyContentsProducesOutput(self, contents, output):
    self.fake_file.NewContents().AndReturn(contents.splitlines())
    self.output_api.PresubmitError(
        self.fake_file_name + ':\n' + output.strip()).AndReturn(None)
    self.mox.ReplayAll()
    css_checker.CSSChecker(self.input_api, self.output_api).RunChecks()

  def testCssAlphaWithAtBlock(self):
    self.VerifyContentsProducesOutput("""
/* A hopefully safely ignored comment and @media statement. /**/
@media print {
  div {
    display: block;
    color: red;
  }
}""", """
- Alphabetize properties and list vendor specific (i.e. -webkit) above standard.
    display: block;
    color: red;""")

  def testCssAlphaWithNonStandard(self):
    self.VerifyContentsProducesOutput("""
div {
  /* A hopefully safely ignored comment and @media statement. /**/
  color: red;
  -webkit-margin-start: 5px;
}""", """
- Alphabetize properties and list vendor specific (i.e. -webkit) above standard.
    color: red;
    -webkit-margin-start: 5px;""")

  def testCssAlphaWithLongerDashedProps(self):
    self.VerifyContentsProducesOutput("""
div {
  border-left: 5px;  /* A hopefully removed comment. */
  border: 5px solid red;
}""", """
- Alphabetize properties and list vendor specific (i.e. -webkit) above standard.
    border-left: 5px;
    border: 5px solid red;""")

  def testCssBracesHaveSpaceBeforeAndNothingAfter(self):
    self.VerifyContentsProducesOutput("""
/* Hello! */div/* Comment here*/{
  display: block;
}

blah /* hey! */
{
  rule: value;
}

.this.is { /* allowed */
  rule: value;
}""", """
- Start braces ({) end a selector, have a space before them and no rules after.
    div{
    {""")

  def testCssClassesUseDashes(self):
    self.VerifyContentsProducesOutput("""
.className,
.ClassName,
.class-name /* We should not catch this. */,
.class_name {
  display: block;
}""", """
 - Classes use .dash-form.
    .className,
    .ClassName,
    .class_name {""")

  def testCssCloseBraceOnNewLine(self):
    self.VerifyContentsProducesOutput("""
@media { /* TODO(dbeam) Fix this case.
  .rule {
    display: block;
  }}

#rule {
  rule: value; }""", """
- Always put a rule closing brace (}) on a new line.
    rule: value; }""")

  def testCssColonsHaveSpaceAfter(self):
    self.VerifyContentsProducesOutput("""
div:not(.class):not([attr]) /* We should not catch this. */ {
  display:block;
}""", """
- Colons (:) should have a space after them.
    display:block;""")

  def testCssFavorSingleQuotes(self):
    self.VerifyContentsProducesOutput("""
html[dir="rtl"] body,
html[dir=ltr] body /* TODO(dbeam): Require '' around rtl in future? */ {
  background: url("chrome://resources/BLAH");
  font-family: "Open Sans";
}""", """
- Use single quotes (') instead of double quotes (") in strings.
    html[dir="rtl"] body,
    background: url("chrome://resources/BLAH");
    font-family: "Open Sans";""")

  def testCssHexCouldBeShorter(self):
    self.VerifyContentsProducesOutput("""
#abc,
#abc-,
#abc-ghij,
#abcdef-,
#abcdef-ghij,
#aaaaaa,
#bbaacc {
  color: #999999;
  color: #666;
}""", """
- Use abbreviated hex (#rgb) when in form #rrggbb.
    color: #999999; (replace with #999)""")

  def testCssUseMillisecondsForSmallTimes(self):
    self.VerifyContentsProducesOutput("""
.transition-0s /* This is gross but may happen. */ {
  transform: one 0.2s;
  transform: two .1s;
  transform: tree 1s;
  transform: four 300ms;
}""", """
- Use milliseconds for time measurements under 1 second.
    transform: one 0.2s; (replace with 200ms)
    transform: two .1s; (replace with 100ms)""")

  def testCssOneRulePerLine(self):
    self.VerifyContentsProducesOutput("""
div {
  rule: value; /* rule: value; */
  rule: value; rule: value;
}""", """
- One rule per line (what not to do: color: red; margin: 0;).
    rule: value; rule: value;""")

  def testCssOneSelectorPerLine(self):
    self.VerifyContentsProducesOutput("""
a,
div,a,
div,/* Hello! */ span,
#id.class([dir=rtl):not(.class):any(a, b, d) {
  rule: value;
}""", """
- One selector per line (what not to do: a, b {}).
    div,a,
    div, span,""")


  def testCssRgbIfNotGray(self):
    self.VerifyContentsProducesOutput("""
#abc,
#aaa,
#aabbcc {
  background: -webkit-linear-gradient(left, from(#abc), to(#def));
  color: #bad;
  color: #bada55;
}""", """
- Use rgb() over #hex when not a shade of gray (like #333).
    background: -webkit-linear-gradient(left, from(#abc), to(#def)); """
"""(replace with rgb(170, 187, 204), rgb(221, 238, 255))
    color: #bad; (replace with rgb(187, 170, 221))
    color: #bada55; (replace with rgb(186, 218, 85))""")

  def testCssZeroLengthTerms(self):
    self.VerifyContentsProducesOutput("""
@-webkit-keyframe anim {
  0% { /* Ignore key frames */
    width: 0px;
  }
  10% {
    width: 10px;
  }
  100% {
    width: 100px;
  }
}
.animating {
  -webkit-animation: anim 0s;
  -webkit-animation-duration: anim 0ms;
  -webkit-transform: scale(0%),
                     translateX(0deg),
                     translateY(0rad),
                     translateZ(0grad);
  background-position-x: 0em;
  background-position-y: 0ex;
  border-width: 0em;
  color: hsl(0, 0%, 85%); /* Shouldn't trigger error. */
}

@page {
  border-width: 0mm;
  height: 0cm;
  width: 0in;
}""", """
- Make all zero length terms (i.e. 0px) 0 unless inside of hsl() or part of"""
""" @keyframe.
    width: 0px;
    -webkit-animation: anim 0s;
    -webkit-animation-duration: anim 0ms;
    -webkit-transform: scale(0%),
    translateX(0deg),
    translateY(0rad),
    translateZ(0grad);
    background-position-x: 0em;
    background-position-y: 0ex;
    border-width: 0em;
    border-width: 0mm;
    height: 0cm;
    width: 0in;
""")

if __name__ == '__main__':
  unittest.main()
