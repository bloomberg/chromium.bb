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
    os.path.normpath(os.path.join(test_dir, '..', '..', 'tools')),
    os.path.join(test_dir),
])

import find_depot_tools # pylint: disable=W0611
from testing_support.super_mox import SuperMoxTestBase
from web_dev_style import css_checker, js_checker # pylint: disable=F0401


class JsStyleGuideTest(SuperMoxTestBase):
  def setUp(self):
    SuperMoxTestBase.setUp(self)

    input_api = self.mox.CreateMockAnything()
    input_api.re = re
    output_api = self.mox.CreateMockAnything()
    self.checker = js_checker.JSChecker(input_api, output_api)

  def GetHighlight(self, line, error):
    """Returns the substring of |line| that is highlighted in |error|."""
    error_lines = error.split('\n')
    highlight = error_lines[error_lines.index(line) + 1]
    return ''.join(ch1 for (ch1, ch2) in zip(line, highlight) if ch2 == '^')

  def ShouldFailConstCheck(self, line):
    """Checks that the 'const' checker flags |line| as a style error."""
    error = self.checker.ConstCheck(1, line)
    self.assertNotEqual('', error,
        'Should be flagged as style error: ' + line)
    self.assertEqual(self.GetHighlight(line, error), 'const')

  def ShouldPassConstCheck(self, line):
    """Checks that the 'const' checker doesn't flag |line| as a style error."""
    self.assertEqual('', self.checker.ConstCheck(1, line),
        'Should not be flagged as style error: ' + line)

  def testConstFails(self):
    lines = [
        "const foo = 'bar';",
        "    const bar = 'foo';",

        # Trying to use |const| as a variable name
        "var const = 0;",

        "var x = 5; const y = 6;",
        "for (var i=0, const e=10; i<e; i++) {",
        "for (const x=0; x<foo; i++) {",
        "while (const x = 7) {",
    ]
    for line in lines:
      self.ShouldFailConstCheck(line)

  def testConstPasses(self):
    lines = [
        # sanity check
        "var foo = 'bar'",

        # @const JsDoc tag
        "/** @const */ var SEVEN = 7;",

        # @const tag in multi-line comment
        " * @const",
        "   * @const",

        # @constructor tag in multi-line comment
        " * @constructor",
        "   * @constructor",

        # words containing 'const'
        "if (foo.constructor) {",
        "var deconstruction = 'something';",
        "var madeUpWordconst = 10;",

        # Strings containing the word |const|
        "var str = 'const at the beginning';",
        "var str = 'At the end: const';",

        # doing this one with regex is probably not practical
        #"var str = 'a const in the middle';",
    ]
    for line in lines:
      self.ShouldPassConstCheck(line)

  def ShouldFailChromeSendCheck(self, line):
    """Checks that the 'chrome.send' checker flags |line| as a style error."""
    error = self.checker.ChromeSendCheck(1, line)
    self.assertNotEqual('', error,
        'Should be flagged as style error: ' + line)
    self.assertEqual(self.GetHighlight(line, error), ', []')

  def ShouldPassChromeSendCheck(self, line):
    """Checks that the 'chrome.send' checker doesn't flag |line| as a style
       error.
    """
    self.assertEqual('', self.checker.ChromeSendCheck(1, line),
        'Should not be flagged as style error: ' + line)

  def testChromeSendFails(self):
    lines = [
        "chrome.send('message', []);",
        "  chrome.send('message', []);",
    ]
    for line in lines:
      self.ShouldFailChromeSendCheck(line)

  def testChromeSendPasses(self):
    lines = [
        "chrome.send('message', constructArgs('foo', []));",
        "  chrome.send('message', constructArgs('foo', []));",
        "chrome.send('message', constructArgs([]));",
        "  chrome.send('message', constructArgs([]));",
    ]
    for line in lines:
      self.ShouldPassChromeSendCheck(line)

  def ShouldFailEndJsDocCommentCheck(self, line):
    """Checks that the **/ checker flags |line| as a style error."""
    error = self.checker.EndJsDocCommentCheck(1, line)
    self.assertNotEqual('', error,
        'Should be flagged as style error: ' + line)
    self.assertEqual(self.GetHighlight(line, error), '**/')

  def ShouldPassEndJsDocCommentCheck(self, line):
    """Checks that the **/ checker doesn't flag |line| as a style error."""
    self.assertEqual('', self.checker.EndJsDocCommentCheck(1, line),
        'Should not be flagged as style error: ' + line)

  def testEndJsDocCommentFails(self):
    lines = [
        "/** @override **/",
        "/** @type {number} @const **/",
        "  **/",
        "**/  ",
    ]
    for line in lines:
      self.ShouldFailEndJsDocCommentCheck(line)

  def testEndJsDocCommentPasses(self):
    lines = [
        "/***************/",  # visual separators
        "  */",  # valid JSDoc comment ends
        "*/  ",
        "/**/",  # funky multi-line comment enders
        "/** @override */",  # legit JSDoc one-liners
    ]
    for line in lines:
      self.ShouldPassEndJsDocCommentCheck(line)

  def ShouldFailGetElementByIdCheck(self, line):
    """Checks that the 'getElementById' checker flags |line| as a style
       error.
    """
    error = self.checker.GetElementByIdCheck(1, line)
    self.assertNotEqual('', error,
        'Should be flagged as style error: ' + line)
    self.assertEqual(self.GetHighlight(line, error), 'document.getElementById')

  def ShouldPassGetElementByIdCheck(self, line):
    """Checks that the 'getElementById' checker doesn't flag |line| as a style
       error.
    """
    self.assertEqual('', self.checker.GetElementByIdCheck(1, line),
        'Should not be flagged as style error: ' + line)

  def testGetElementByIdFails(self):
    lines = [
        "document.getElementById('foo');",
        "  document.getElementById('foo');",
        "var x = document.getElementById('foo');",
        "if (document.getElementById('foo').hidden) {",
    ]
    for line in lines:
      self.ShouldFailGetElementByIdCheck(line)

  def testGetElementByIdPasses(self):
    lines = [
        "elem.ownerDocument.getElementById('foo');",
        "  elem.ownerDocument.getElementById('foo');",
        "var x = elem.ownerDocument.getElementById('foo');",
        "if (elem.ownerDocument.getElementById('foo').hidden) {",
        "doc.getElementById('foo');",
        "  doc.getElementById('foo');",
        "cr.doc.getElementById('foo');",
        "  cr.doc.getElementById('foo');",
        "var x = doc.getElementById('foo');",
        "if (doc.getElementById('foo').hidden) {",
    ]
    for line in lines:
      self.ShouldPassGetElementByIdCheck(line)

  def ShouldFailInheritDocCheck(self, line):
    """Checks that the '@inheritDoc' checker flags |line| as a style error."""
    error = self.checker.InheritDocCheck(1, line)
    self.assertNotEqual('', error,
        msg='Should be flagged as style error: ' + line)
    self.assertEqual(self.GetHighlight(line, error), '@inheritDoc')

  def ShouldPassInheritDocCheck(self, line):
    """Checks that the '@inheritDoc' checker doesn't flag |line| as a style
       error.
    """
    self.assertEqual('', self.checker.InheritDocCheck(1, line),
        msg='Should not be flagged as style error: ' + line)

  def testInheritDocFails(self):
    lines = [
        " /** @inheritDoc */",
        "   * @inheritDoc",
    ]
    for line in lines:
      self.ShouldFailInheritDocCheck(line)

  def testInheritDocPasses(self):
    lines = [
        "And then I said, but I won't @inheritDoc! Hahaha!",
        " If your dad's a doctor, do you inheritDoc?",
        "  What's up, inherit doc?",
        "   this.inheritDoc(someDoc)",
    ]
    for line in lines:
      self.ShouldPassInheritDocCheck(line)

  def ShouldFailWrapperTypeCheck(self, line):
    """Checks that the use of wrapper types (i.e. new Number(), @type {Number})
       is a style error.
    """
    error = self.checker.WrapperTypeCheck(1, line)
    self.assertNotEqual('', error,
        msg='Should be flagged as style error: ' + line)
    highlight = self.GetHighlight(line, error)
    self.assertTrue(highlight in ('Boolean', 'Number', 'String'))

  def ShouldPassWrapperTypeCheck(self, line):
    """Checks that the wrapper type checker doesn't flag |line| as a style
       error.
    """
    self.assertEqual('', self.checker.WrapperTypeCheck(1, line),
        msg='Should not be flagged as style error: ' + line)

  def testWrapperTypePasses(self):
    lines = [
        "/** @param {!ComplexType} */",
        "  * @type {Object}",
        "   * @param {Function=} opt_callback",
        "    * @param {} num Number of things to add to {blah}.",
        "   *  @return {!print_preview.PageNumberSet}",
        " /* @returns {Number} */",  # Should be /** @return {Number} */
        "* @param {!LocalStrings}"
        " Your type of Boolean is false!",
        "  Then I parameterized her Number from her friend!",
        "   A String of Pearls",
        "    types.params.aBoolean.typeString(someNumber)",
    ]
    for line in lines:
      self.ShouldPassWrapperTypeCheck(line)

  def testWrapperTypeFails(self):
    lines = [
        "  /**@type {String}*/(string)",
        "   * @param{Number=} opt_blah A number",
        "/** @private @return {!Boolean} */",
        " * @param {number|String}",
    ]
    for line in lines:
      self.ShouldFailWrapperTypeCheck(line)

  def ShouldFailVarNameCheck(self, line):
    """Checks that var unix_hacker, $dollar are style errors."""
    error = self.checker.VarNameCheck(1, line)
    self.assertNotEqual('', error,
        msg='Should be flagged as style error: ' + line)
    highlight = self.GetHighlight(line, error)
    self.assertFalse('var ' in highlight);

  def ShouldPassVarNameCheck(self, line):
    """Checks that variableNamesLikeThis aren't style errors."""
    self.assertEqual('', self.checker.VarNameCheck(1, line),
        msg='Should not be flagged as style error: ' + line)

  def testVarNameFails(self):
    lines = [
        "var private_;",
        " var _super_private",
        "  var unix_hacker = someFunc();",
    ]
    for line in lines:
      self.ShouldFailVarNameCheck(line)

  def testVarNamePasses(self):
    lines = [
        "  var namesLikeThis = [];",
        " for (var i = 0; i < 10; ++i) { ",
        "for (var i in obj) {",
        " var one, two, three;",
        "  var magnumPI = {};",
        " var g_browser = 'da browzer';",
        "/** @const */ var Bla = options.Bla;",  # goog.scope() replacement.
        " var $ = function() {",                 # For legacy reasons.
        "  var StudlyCaps = cr.define('bla')",   # Classes.
        " var SCARE_SMALL_CHILDREN = [",         # TODO(dbeam): add @const in
                                                 # front of all these vars like
        "/** @const */ CONST_VAR = 1;",          # this line has (<--).
    ]
    for line in lines:
      self.ShouldPassVarNameCheck(line)


class CssStyleGuideTest(SuperMoxTestBase):
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

    # Actual creations of PresubmitPromptWarning are defined in each test.
    self.output_api = self.mox.CreateMockAnything()
    self.mox.StubOutWithMock(self.output_api, 'PresubmitPromptWarning',
                             use_mock_anything=True)

    author_msg = ('Was the CSS checker useful? '
                  'Send feedback or hate mail to dbeam@chromium.org.')
    self.output_api = self.mox.CreateMockAnything()
    self.mox.StubOutWithMock(self.output_api, 'PresubmitNotifyResult',
                             use_mock_anything=True)
    self.output_api.PresubmitNotifyResult(author_msg).AndReturn(None)

  def VerifyContentsProducesOutput(self, contents, output):
    self.fake_file.NewContents().AndReturn(contents.splitlines())
    self.output_api.PresubmitPromptWarning(
        self.fake_file_name + ':\n' + output.strip()).AndReturn(None)
    self.mox.ReplayAll()
    css_checker.CSSChecker(self.input_api, self.output_api).RunChecks()

  def testCssAlphaWithAtBlock(self):
    self.VerifyContentsProducesOutput("""
<include src="../shared/css/cr/ui/overlay.css">
<include src="chrome://resources/totally-cool.css" />

/* A hopefully safely ignored comment and @media statement. /**/
@media print {
  div {
    display: block;
    color: red;
  }
}

.rule {
  z-index: 5;
<if expr="not is macosx">
  background-image: url(chrome://resources/BLAH); /* TODO(dbeam): Fix this. */
  background-color: rgb(235, 239, 249);
</if>
<if expr="is_macosx">
  background-color: white;
  background-image: url(chrome://resources/BLAH2);
</if>
  color: black;
}

<if expr="is_macosx">
.language-options-right {
  visibility: hidden;
  opacity: 1; /* TODO(dbeam): Fix this. */
}
</if>""", """
- Alphabetize properties and list vendor specific (i.e. -webkit) above standard.
    display: block;
    color: red;

    z-index: 5;
    color: black;""")

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
@media { /* TODO(dbeam) Fix this case. */
  .rule {
    display: block;
  }}

@-webkit-keyframe blah {
  100% { height: -500px 0; }
}

#rule {
  rule: value; }""", """
- Always put a rule closing brace (}) on a new line.
    rule: value; }""")

  def testCssColonsHaveSpaceAfter(self):
    self.VerifyContentsProducesOutput("""
div:not(.class):not([attr=5]), /* We should not catch this. */
div:not(.class):not([attr]) /* Nor this. */ {
  background: url(data:image/jpeg,asdfasdfsadf); /* Ignore this. */
  background: -webkit-linear-gradient(left, red,
                                      80% blah blee blar);
  color: red;
  display:block;
}""", """
- Colons (:) should have a space after them.
    display:block;

- Don't use data URIs in source files. Use grit instead.
    background: url(data:image/jpeg,asdfasdfsadf);""")

  def testCssFavorSingleQuotes(self):
    self.VerifyContentsProducesOutput("""
html[dir="rtl"] body,
html[dir=ltr] body /* TODO(dbeam): Require '' around rtl in future? */ {
  background: url("chrome://resources/BLAH");
  font-family: "Open Sans";
<if expr="is_macosx">
  blah: blee;
</if>
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
  background-color: #336699; /* Ignore short hex rule if not gray. */
  color: #999999;
  color: #666;
}""", """
- Use abbreviated hex (#rgb) when in form #rrggbb.
    color: #999999; (replace with #999)

- Use rgb() over #hex when not a shade of gray (like #333).
    background-color: #336699; (replace with rgb(51, 102, 153))""")

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

  def testCssNoDataUrisInSourceFiles(self):
    self.VerifyContentsProducesOutput("""
img {
  background: url( data:image/jpeg,4\/\/350|\/|3|2 );
  background: url('data:image/jpeg,4\/\/350|\/|3|2');
}""", """
- Don't use data URIs in source files. Use grit instead.
    background: url( data:image/jpeg,4\/\/350|\/|3|2 );
    background: url('data:image/jpeg,4\/\/350|\/|3|2');""")

  def testCssOneRulePerLine(self):
    self.VerifyContentsProducesOutput("""
a:not([hidden]):not(.custom-appearance):not([version=1]):first-of-type,
a:not([hidden]):not(.custom-appearance):not([version=1]):first-of-type ~
    input[type='checkbox']:not([hidden]),
div {
  background: url(chrome://resources/BLAH);
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
}

a,
div,a {
  some-other: rule here;
}""", """
- One selector per line (what not to do: a, b {}).
    div,a,
    div, span,
    div,a {""")

  def testCssPseudoElementDoubleColon(self):
    self.VerifyContentsProducesOutput("""
a:href,
br::after,
::-webkit-scrollbar-thumb,
a:not([empty]):hover:focus:active, /* shouldn't catch here and above */
abbr:after,
.tree-label:empty:after,
b:before,
:-WebKit-ScrollBar {
  rule: value;
}""", """
- Pseudo-elements should use double colon (i.e. ::after).
    :after (should be ::after)
    :after (should be ::after)
    :before (should be ::before)
    :-WebKit-ScrollBar (should be ::-WebKit-ScrollBar)
    """)

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

/* http://crbug.com/359682 */
#spinner-container #spinner {
  -webkit-animation-duration: 1.0s;
}

.media-button.play > .state0.active,
.media-button[state='0'] > .state0.normal /* blah */, /* blee */
.media-button[state='0']:not(.disabled):hover > .state0.hover {
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
  opacity: .0;
  opacity: 0.0;
  opacity: 0.;
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
    opacity: .0;
    opacity: 0.0;
    opacity: 0.;
    border-width: 0mm;
    height: 0cm;
    width: 0in;
""")

if __name__ == '__main__':
  unittest.main()
