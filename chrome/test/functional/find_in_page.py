#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import os
import unittest

import pyauto_functional
import pyauto
import test_utils


class FindMatchTests(pyauto.PyUITest):

  # Data dir where all find test data files are kept
  find_test_data_dir = 'find_in_page'

  def testCanFindMatchCount(self):
    """Verify Find match count for valid search"""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    self.NavigateToURL(url)
    self.assertEqual(1, self.FindInPage('title')['match_count'])

  def testCanFindMatchCountFail(self):
    """Verify Find match count for invalid search"""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    self.NavigateToURL(url)
    self.assertEqual(0, self.FindInPage('blah')['match_count'])

  def testFindIsNotCaseSensitive(self):
    """Verify that find is not case sensitive.

    Manually Find is case insensitive. But since FindInPage is
    case-sensitive by default we are confirming that we get a
    different result when we turn off case matching.
    """
    url = self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'find_in_page', 'largepage.html'))
    self.NavigateToURL(url)
    case_sensitive_result = self.FindInPage('The')['match_count']
    case_insenstive_result = (self.FindInPage('The', match_case=False)
                              ['match_count'])
    self.assertTrue(case_insenstive_result >= case_sensitive_result)

  def testLocalizationAndCaseOrder(self):
    """Verify that we check for localization.

    Here we check the Turkish-i scenario where we verify that we
    find both dotted and dotless I's.
    """
    url = self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'find_in_page', 'turkish.html'))
    self.NavigateToURL(url)
    dotless = self.FindInPage(u'\u0131')['match_count']
    dotted = self.FindInPage('i')['match_count']
    capital_i_with_dot = self.FindInPage(u'\u0130')['match_count']
    capital_i = self.FindInPage('I')['match_count']
    self.assertNotEqual(dotless, dotted)
    self.assertNotEqual(capital_i_with_dot, capital_i)
    self.assertNotEqual(dotted, capital_i_with_dot)

  def testSearchInTextAreas(self):
    """Verify search for text within various forms and text areas."""
    urls = []
    urls.append(self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'find_in_page', 'textintextarea.html')))
    urls.append(self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'find_in_page', 'smalltextarea.html')))
    urls.append(self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'find_in_page', 'populatedform.html')))
    for url in urls:
      self.NavigateToURL(url)
      self.assertEqual(1, self.FindInPage('cat')['match_count'])
      self.assertEqual(0, self.FindInPage('bat')['match_count'])

  def testSearchWithinSpecialURL(self):
    """Verify search for text within special URLs such as chrome:history.
       chrome://history, chrome://downloads, pyAuto Data directory
    """
    zip_file = 'a_zip_file.zip'
    self.NavigateToURL(self.GetFileURLForPath(self.DataDir()))
    # search in Data directory
    self.assertEqual(1,
        self.FindInPage('downloads', tab_index=0)['match_count'])
    # search in History page
    self.AppendTab(pyauto.GURL('chrome://history'))
    # the contents in the history page load asynchronously after tab loads
    search_query = os.path.join('chrome', 'test', 'data')
    self.WaitUntil(
        lambda: self.FindInPage(search_query, tab_index=1)['match_count'],
        expect_retval=1)
    self.assertEqual(
        1, self.FindInPage(search_query, tab_index=1)['match_count'])
    # search in Downloads page
    test_utils.DownloadFileFromDownloadsDataDir(self, zip_file)
    self.AppendTab(pyauto.GURL('chrome://downloads'))
    # the contents in the downloads page load asynchronously after tab loads
    self.WaitUntil(
        lambda: self.FindInPage(zip_file, tab_index=2)['match_count'],
        expect_retval=2)
    self.assertEqual(2,
        self.FindInPage(zip_file, tab_index=2)['match_count'])
    test_utils.RemoveDownloadedTestFile(self, zip_file)

  def testFindNextAndPrevious(self):
    """Verify search selection coordinates.

    The data file used is set-up such that the text occurs on the same line,
    and we verify their positions by verifying their relative positions.
    """
    search_string = u'\u5728\u897f\u660c\u536b\u661f\u53d1'
    url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), self.find_test_data_dir, 'specialchar.html'))
    self.NavigateToURL(url)
    first_find = self.FindInPage(search_string)
    second_find = self.FindInPage(search_string, find_next=True)
    # We have search occurrence in the same row, so top-bottom
    # coordinates should be the same even for second search.
    self.assertEqual(first_find['match_top'], second_find['match_top'],
                     'Words\' top coordinates should be same')
    self.assertEqual(first_find['match_bottom'], second_find['match_bottom'],
                     'Words\' bottom coordinates should be same')
    # And left-right coordinates should be in order.
    self.assertTrue(first_find['match_left'] < second_find['match_left'],
                    'Second find left coordinate should be greater than '
                    'the first find left coordinate')
    self.assertTrue(first_find['match_right'] < second_find['match_right'],
                    'Second find right coordinate should be greater than '
                    'the first find right coordinate')
    first_find_reverse = self.FindInPage(
        search_string, find_next=True, forward=False)
    # We find next and we go back so find coordinates should be the same
    # as previous ones.
    self.assertEqual(first_find, first_find_reverse,
                     'First occurrence must be selected, since we went back')

  def testSpecialChars(self):
    """Test find in page with unicode and special characters.

    Finds from page content, comments and meta data and verifies that comments
    and meta data are not searchable.
    """
    search_string = u'\u5728\u897f\u660c\u536b\u661f\u53d1'
    url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), self.find_test_data_dir, 'specialchar.html'))
    self.NavigateToURL(url)
    self.assertEqual(4, self.FindInPage(search_string)['match_count'])
    search_string = u'240^*&%!#~!*&\u518d\u5c31\u8077\u624b\u5f53'
    self.assertEqual(2, self.FindInPage(search_string)['match_count'])
    # Find for the special chars in the comment and in the meta tag
    search_string = u'\u4e2d\u65b0\u793e\u8bb0\u8005\u5b8b\u5409'\
                  u'\u6cb3\u6444\u4e2d\u65b0\u7f51'
    self.assertEqual(0, self.FindInPage(search_string)['match_count'],
                     'Chrome should not find chars from comment or meta tags')

  def testFindInLargePage(self):
    """Find in a very large page"""
    url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), self.find_test_data_dir, 'largepage.html'))
    self.NavigateToURL(url)
    self.assertEqual(373, self.FindInPage('daughter of Prince')['match_count'])

  def testFindLongString(self):
    """Find a very long string in a large page"""
    url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), self.find_test_data_dir, 'largepage.html'))
    self.NavigateToURL(url)
    file = codecs.open(os.path.join(self.DataDir(), self.find_test_data_dir,
                                    'LongFind.txt'), 'r', 'utf-8')
    search = file.read()
    self.assertEqual(1, self.FindInPage(search)['match_count'])

  def testFindBigString(self):
    """Find a big font string in a page"""
    url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), self.find_test_data_dir, 'BigText.html'))
    self.NavigateToURL(url)
    self.assertEqual(1, self.FindInPage('SomeLargeString')['match_count'])

  def testVariousFindTests(self):
    """Test find in page for <span> style text, lists, html comments, etc."""
    url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), self.find_test_data_dir, 'FindRandomTests.html'))
    self.NavigateToURL(url)
    search = 'has light blue eyes and my father has dark'
    self.assertEqual(1, self.FindInPage(search)['match_count'],
                     'Failed to find text with <span> tag')
    # Find for list items
    search = 'Google\nApple\nandroid'
    self.assertEqual(1, self.FindInPage(search)['match_count'],
                     'Failed to find the list items')
    # Find HTML comments
    self.assertEqual(0, self.FindInPage('example comment')['match_count'],
                     'We should not find HTML comments')

  def testFindWholeFileContent(self):
    """Find the whole text file page and find count should be 1"""
    find_test_file = os.path.join(self.DataDir(), self.find_test_data_dir,
                                  'find_test.txt')
    url = self.GetFileURLForPath(find_test_file)
    self.NavigateToURL(url)
    file = open(find_test_file)
    search = file.read()
    self.assertEqual(1, self.FindInPage(search)['match_count'],
                     'Failed to find the whole page')

  def testSingleOccurrence(self):
    """Search Back and Forward on a single occurrence"""
    url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), self.find_test_data_dir, 'FindRandomTests.html'))
    self.NavigateToURL(url)
    self.assertEqual(1, self.FindInPage('2010 Pro Bowl')['match_count'])
    # First occurrence find
    first_occurence_dict = self.FindInPage('2010 Pro Bowl')
    # Finding next occurrence
    next_occurence_dict = self.FindInPage('2010 Pro Bowl', find_next = True)
    self.assertEqual(first_occurence_dict, next_occurence_dict,
                     'We have only one occurrence in this page so'
                     'first and next coordinates must be same')
    # Doing a fake find so we have no previous search
    self.FindInPage('ghgfjgfh201232rere')

    first_occurence_dict = self.FindInPage('2010 Pro Bowl')
    # Finding previous occurrence
    back_occurence_dict = self.FindInPage('2010 Pro Bowl',
                                          find_next = True, forward = False)
    self.assertEqual(first_occurence_dict, back_occurence_dict,
                     'We have only one occurrence in this page so '
                     'first and back search coordinates must be same')

  def _VerifySearchInPDFURL(self, url, word, expected_count):
    """Verify that we can find in a pdf file."""
    self.NavigateToURL(url)
    search_count = self.FindInPage(word)['match_count']
    self.assertEqual(expected_count, search_count,
                     'Failed to find in the %s pdf file' % url)

  def testSearchInPDF(self):
    """Verify that we can find in a pdf file.

    Only for Google Chrome builds (Chromium builds do not have internal pdf).
    """
    # bail out if not a branded build
    properties = self.GetBrowserInfo()['properties']
    if properties['branding'] != 'Google Chrome':
      return
    # Search in pdf file over file://.
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'plugin', 'Embed.pdf'))
    self._VerifySearchInPDFURL(file_url, 'adobe', 8) 

    # Disabling this test crbug.com/70927 
    # Search in pdf file over http://.
    # http_url = 'http://www.irs.gov/pub/irs-pdf/fw4.pdf'
    # self._VerifySearchInPDFURL(http_url, 'Allowances', 16) 

if __name__ == '__main__':
  pyauto_functional.Main()
