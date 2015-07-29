#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from xml.etree import ElementTree

import emma_coverage_stats
from pylib import constants

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'pymock'))
import mock  # pylint: disable=F0401


class _EmmaHtmlParserTest(unittest.TestCase):
  """Tests for _EmmaHtmlParser.

  Uses modified EMMA report HTML that contains only the subset of tags needed
  for test verification.
  """

  def setUp(self):
    self.emma_dir = 'fake/dir/'
    self.parser = emma_coverage_stats._EmmaHtmlParser(self.emma_dir)
    self.simple_html = '<TR><TD CLASS="p">Test HTML</TD></TR>'
    self.index_html = (
      '<HTML>'
        '<BODY>'
          '<TABLE CLASS="hdft" CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
          '<TABLE CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
          '<TABLE CLASS="it" CELLSPACING="0">'
          '</TABLE>'
          '<TABLE CELLSPACING="0" WIDTH="100%">'
            '<TR>'
              '<TH CLASS="f">name</TH>'
              '<TH>class, %</TH>'
              '<TH>method, %</TH>'
              '<TH>block, %</TH>'
              '<TH>line, %</TH>'
            '</TR>'
            '<TR CLASS="o">'
              '<TD><A HREF="_files/0.html"'
              '>org.chromium.chrome.browser</A></TD>'
              '<TD CLASS="h">0%   (0/3)</TD>'
            '</TR>'
            '<TR>'
              '<TD><A HREF="_files/1.html"'
              '>org.chromium.chrome.browser.tabmodel</A></TD>'
              '<TD CLASS="h">0%   (0/8)</TD>'
            '</TR>'
          '</TABLE>'
          '<TABLE CLASS="hdft" CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
        '</BODY>'
      '</HTML>'
    )
    self.package_1_class_list_html = (
      '<HTML>'
        '<BODY>'
          '<TABLE CLASS="hdft" CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
          '<TABLE CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
          '<TABLE CELLSPACING="0" WIDTH="100%">'
            '<TR>'
              '<TH CLASS="f">name</TH>'
              '<TH>class, %</TH>'
              '<TH>method, %</TH>'
              '<TH>block, %</TH>'
              '<TH>line, %</TH>'
            '</TR>'
            '<TR CLASS="o">'
              '<TD><A HREF="1e.html">IntentHelper.java</A></TD>'
              '<TD CLASS="h">0%   (0/3)</TD>'
              '<TD CLASS="h">0%   (0/9)</TD>'
              '<TD CLASS="h">0%   (0/97)</TD>'
              '<TD CLASS="h">0%   (0/26)</TD>'
            '</TR>'
          '</TABLE>'
          '<TABLE CLASS="hdft" CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
        '</BODY>'
      '</HTML>'
    )
    self.package_2_class_list_html = (
      '<HTML>'
        '<BODY>'
          '<TABLE CLASS="hdft" CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
          '<TABLE CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
          '<TABLE CELLSPACING="0" WIDTH="100%">'
            '<TR>'
              '<TH CLASS="f">name</TH>'
              '<TH>class, %</TH>'
              '<TH>method, %</TH>'
              '<TH>block, %</TH>'
              '<TH>line, %</TH>'
            '</TR>'
            '<TR CLASS="o">'
              '<TD><A HREF="1f.html">ContentSetting.java</A></TD>'
              '<TD CLASS="h">0%   (0/1)</TD>'
            '</TR>'
            '<TR>'
              '<TD><A HREF="20.html">DevToolsServer.java</A></TD>'
            '</TR>'
            '<TR CLASS="o">'
              '<TD><A HREF="21.html">FileProviderHelper.java</A></TD>'
            '</TR>'
            '<TR>'
              '<TD><A HREF="22.html">ContextualMenuBar.java</A></TD>'
            '</TR>'
            '<TR CLASS="o">'
              '<TD><A HREF="23.html">AccessibilityUtil.java</A></TD>'
            '</TR>'
            '<TR>'
              '<TD><A HREF="24.html">NavigationPopup.java</A></TD>'
            '</TR>'
          '</TABLE>'
          '<TABLE CLASS="hdft" CELLSPACING="0" WIDTH="100%">'
          '</TABLE>'
        '</BODY>'
      '</HTML>'
    )
    self.partially_covered_tr_html = (
      '<TR CLASS="p">'
        '<TD CLASS="l" TITLE="78% line coverage (7 out of 9)">108</TD>'
        '<TD TITLE="78% line coverage (7 out of 9 instructions)">'
          'if (index &lt; 0 || index = mSelectors.size()) index = 0;</TD>'
      '</TR>'
    )
    self.covered_tr_html = (
      '<TR CLASS="c">'
        '<TD CLASS="l">110</TD>'
        '<TD>        if (mSelectors.get(index) != null) {</TD>'
      '</TR>'
    )
    self.not_executable_tr_html = (
      '<TR>'
        '<TD CLASS="l">109</TD>'
        '<TD> </TD>'
      '</TR>'
    )
    self.tr_with_extra_a_tag = (
      '<TR CLASS="z">'
        '<TD CLASS="l">'
          '<A name="1f">54</A>'
        '</TD>'
        '<TD>            }</TD>'
      '</TR>'
    )

  def testInit(self):
    emma_dir = self.emma_dir
    parser = emma_coverage_stats._EmmaHtmlParser(emma_dir)
    self.assertEqual(parser._base_dir, emma_dir)
    self.assertEqual(parser._emma_files_path, 'fake/dir/_files')
    self.assertEqual(parser._index_path, 'fake/dir/index.html')

  def testFindElements_basic(self):
    read_values = [self.simple_html]
    found, _ = MockOpenForFunction(self.parser._FindElements, read_values,
                                   file_path='fake', xpath_selector='.//TD')
    self.assertIs(type(found), list)
    self.assertIs(type(found[0]), ElementTree.Element)
    self.assertEqual(found[0].text, 'Test HTML')

  def testFindElements_multipleElements(self):
    multiple_trs = self.not_executable_tr_html + self.covered_tr_html
    read_values = ['<div>' + multiple_trs + '</div>']
    found, _ = MockOpenForFunction(self.parser._FindElements, read_values,
                                   file_path='fake', xpath_selector='.//TR')
    self.assertEquals(2, len(found))

  def testFindElements_noMatch(self):
    read_values = [self.simple_html]
    found, _ = MockOpenForFunction(self.parser._FindElements, read_values,
                                   file_path='fake', xpath_selector='.//TR')
    self.assertEqual(found, [])

  def testFindElements_badFilePath(self):
    with self.assertRaises(IOError):
      with mock.patch('os.path.exists', return_value=False):
        self.parser._FindElements('fake', xpath_selector='//tr')

  def testGetPackageNameToEmmaFileDict_basic(self):
    expected_dict = {
      'org.chromium.chrome.browser.AccessibilityUtil.java':
      'fake/dir/_files/23.html',
      'org.chromium.chrome.browser.ContextualMenuBar.java':
      'fake/dir/_files/22.html',
      'org.chromium.chrome.browser.tabmodel.IntentHelper.java':
      'fake/dir/_files/1e.html',
      'org.chromium.chrome.browser.ContentSetting.java':
      'fake/dir/_files/1f.html',
      'org.chromium.chrome.browser.DevToolsServer.java':
      'fake/dir/_files/20.html',
      'org.chromium.chrome.browser.NavigationPopup.java':
      'fake/dir/_files/24.html',
      'org.chromium.chrome.browser.FileProviderHelper.java':
      'fake/dir/_files/21.html'}

    read_values = [self.index_html, self.package_1_class_list_html,
                   self.package_2_class_list_html]
    return_dict, mock_open = MockOpenForFunction(
        self.parser.GetPackageNameToEmmaFileDict, read_values)

    self.assertDictEqual(return_dict, expected_dict)
    self.assertEqual(mock_open.call_count, 3)
    calls = [mock.call('fake/dir/index.html'),
             mock.call('fake/dir/_files/1.html'),
             mock.call('fake/dir/_files/0.html')]
    mock_open.assert_has_calls(calls)

  def testGetPackageNameToEmmaFileDict_noPackageElements(self):
    self.parser._FindElements = mock.Mock(return_value=[])
    return_dict = self.parser.GetPackageNameToEmmaFileDict()
    self.assertDictEqual({}, return_dict)

  def testGetLineCoverage_status_basic(self):
    line_coverage = self.GetLineCoverageWithFakeElements([self.covered_tr_html])
    self.assertEqual(line_coverage[0].covered_status,
                     emma_coverage_stats.COVERED)

  def testGetLineCoverage_status_statusMissing(self):
    line_coverage = self.GetLineCoverageWithFakeElements(
        [self.not_executable_tr_html])
    self.assertEqual(line_coverage[0].covered_status,
                     emma_coverage_stats.NOT_EXECUTABLE)

  def testGetLineCoverage_fractionalCoverage_basic(self):
    line_coverage = self.GetLineCoverageWithFakeElements([self.covered_tr_html])
    self.assertEqual(line_coverage[0].fractional_line_coverage, 1.0)

  def testGetLineCoverage_fractionalCoverage_partial(self):
    line_coverage = self.GetLineCoverageWithFakeElements(
        [self.partially_covered_tr_html])
    self.assertEqual(line_coverage[0].fractional_line_coverage, 0.78)

  def testGetLineCoverage_lineno_basic(self):
    line_coverage = self.GetLineCoverageWithFakeElements([self.covered_tr_html])
    self.assertEqual(line_coverage[0].lineno, 110)

  def testGetLineCoverage_lineno_withAlternativeHtml(self):
    line_coverage = self.GetLineCoverageWithFakeElements(
        [self.tr_with_extra_a_tag])
    self.assertEqual(line_coverage[0].lineno, 54)

  def testGetLineCoverage_source(self):
    self.parser._FindElements = mock.Mock(
        return_value=[ElementTree.fromstring(self.covered_tr_html)])
    line_coverage = self.parser.GetLineCoverage('fake_path')
    self.assertEqual(line_coverage[0].source,
                     '        if (mSelectors.get(index) != null) {')

  def testGetLineCoverage_multipleElements(self):
    line_coverage = self.GetLineCoverageWithFakeElements(
        [self.covered_tr_html, self.partially_covered_tr_html,
         self.tr_with_extra_a_tag])
    self.assertEqual(len(line_coverage), 3)

  def GetLineCoverageWithFakeElements(self, html_elements):
    """Wraps GetLineCoverage to work with extra whitespace characters.

    The test HTML strings include extra whitespace characters to make the HTML
    human readable. This isn't the case with EMMA HTML files, so we need to
    remove all the unnecessary whitespace.

    Args:
      html_elements: List of strings each representing an HTML element.

    Returns:
      A list of LineCoverage objects.
    """
    elements = [ElementTree.fromstring(string) for string in html_elements]
    with mock.patch('emma_coverage_stats._EmmaHtmlParser._FindElements',
                    return_value=elements):
      return self.parser.GetLineCoverage('fake_path')


def MockOpenForFunction(func, side_effects, **kwargs):
  """Allows easy mock open and read for callables that open multiple files.

  Args:
    func: The callable to invoke once mock files are setup.
    side_effects: A list of return values for each file to return once read.
      Length of list should be equal to the number calls to open in |func|.
    **kwargs: Keyword arguments to be passed to |func|.

  Returns:
    A tuple containing the return value of |func| and the MagicMock object used
      to mock all calls to open respectively.
  """
  mock_open = mock.mock_open()
  mock_open.side_effect = [mock.mock_open(read_data=side_effect).return_value
                           for side_effect in side_effects]
  with mock.patch('__builtin__.open', mock_open):
    return func(**kwargs), mock_open


if __name__ == '__main__':
  unittest.main()
