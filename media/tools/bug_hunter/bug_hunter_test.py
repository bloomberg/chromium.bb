# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration tests for bug hunter."""

import csv
from optparse import Values
import os
import unittest

from bug_hunter import BugHunter

try:
  import gdata.data
  import gdata.projecthosting.client
except ImportError:
  logging.error('gdata-client needs to be installed. Please install\n'
                'and try again (http://code.google.com/p/gdata-python-client/)')
  sys.exit(1)


class BugHunterTest(unittest.TestCase):
  """Unit tests for the Bug Hunter class."""
  _TEST_FILENAME = 'test.csv'

  def _CleanTestFile(self):
    if os.path.exists(self._TEST_FILENAME):
      os.remove(self._TEST_FILENAME)

  def setUp(self):
    self._CleanTestFile()

  def tearDown(self):
    self._CleanTestFile()

  def _GetIssue(self):
    return [{'issue_id': '0', 'title': 'title', 'author': 'author',
             'status': 'status', 'state': 'state', 'content': 'content',
             'comments': [], 'labels': [], 'urls': []}]

  def _GetDefaultOption(self, set_10_days_ago, query='steps'):
    ops = Values()
    ops.query = query
    if set_10_days_ago:
      ops.interval_value = 10
      ops.interval_unit = 'days'
    else:
      ops.interval_value = None
    ops.email_entries = ['comments']
    ops.project_name = 'chromium'
    ops.query_title = 'query title'
    ops.max_comments = None
    return ops

  def testGetIssueReturnedIssue(self):
    bh = BugHunter(
        self._GetDefaultOption(False,
                               query=('audio opened-after:2010/10/10'
                                      ' opened-before:2010/10/20')))
    self.assertEquals(len(bh.GetIssues()), 18)

  def testGetIssueReturnedIssueWithStatus(self):
    ops = self._GetDefaultOption(False)
    ops.query = 'Feature:Media* Status:Unconfirmed'
    issues = BugHunter(ops).GetIssues()
    for issue in issues:
      self.assertEquals(issue['status'], 'Unconfirmed')

  def testGetIssueReturnNoIssue(self):
    ops = self._GetDefaultOption(True)
    ops.query = 'thisshouldnotmatchpleaseignorethis*'
    self.assertFalse(BugHunter(ops).GetIssues())

  def testGetComments(self):
    comments = BugHunter(self._GetDefaultOption(False)).GetComments(100000, 2)
    self.assertEquals(len(comments), 2)
    expected_comments = [(None, 'rby...@chromium.org',
                          '2011-10-31T19:54:40.000Z'),
                         (None, 'backer@chromium.org',
                          '2011-10-14T13:59:37.000Z')]
    self.assertEquals(comments, expected_comments)

  def testWriteIssuesToFileInCSV(self):
    ops = self._GetDefaultOption(False)
    bh = BugHunter(ops)
    bh.WriteIssuesToFileInCSV(self._GetIssue(), self._TEST_FILENAME)

    with open(self._TEST_FILENAME, 'r') as f:
      reader = csv.reader(f)
      self.assertEquals(reader.next(), ['status', 'content', 'state',
                                        'issue_id', 'urls', 'title', 'labels',
                                        'author', 'comments'])
      self.assertEquals(reader.next(), ['status', 'content', 'state', '0',
                                        '[]', 'title', '[]', 'author', '[]'])
      self.assertRaises(StopIteration, reader.next)
