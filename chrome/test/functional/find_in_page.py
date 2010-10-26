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


if __name__ == '__main__':
  pyauto_functional.Main()
