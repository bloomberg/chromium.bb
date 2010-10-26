#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import pyauto_functional
import pyauto


class FindMatchTests(pyauto.PyUITest):

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
    self.NavigateToURL(self.GetFileURLForPath(self.DataDir()))
    # search in Data directory
    self.assertEqual(1,
        self.FindInPage('downloads', tab_index=0)['match_count'])
    # search in History page
    self.AppendTab(pyauto.GURL('chrome://history'))
    self.assertEqual(1, self.FindInPage('data', tab_index=1)['match_count'])
    # search in Downloads page
    self._DownloadZipFile()
    self.AppendTab(pyauto.GURL('chrome://downloads'))
    self.assertEqual(2,
        self.FindInPage('a_zip_file.zip', tab_index=2)['match_count'])
    self._RemoveZipFile()

  def _DownloadZipFile(self):
    """Download a zip file."""
    zip_file = 'a_zip_file.zip'
    download_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_path = os.path.join(download_dir, zip_file)
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(), zip_file)
    # Check if zip file already exists. If so then delete it.
    if os.path.exists(downloaded_pkg):
      self._RemoveZipFile()
    self.DownloadAndWaitForStart(file_url)
    # Wait for the download to finish
    self.WaitForAllDownloadsToComplete()

  def _RemoveZipFile(self):
    """Delete a_zip_file.zip from the download directory."""
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    os.remove(downloaded_pkg)


if __name__ == '__main__':
  pyauto_functional.Main()
