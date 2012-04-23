# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit Tests for bug hunter."""

import logging
from optparse import Values
import smtplib
import sys
import unittest

from bug_hunter import BugHunter
from bug_hunter import BugHunterUtils

try:
  import atom.data
  import gdata.data
  import gdata.projecthosting.client
except ImportError:
  logging.error('gdata-client needs to be installed. Please install\n'
                'and try again (http://code.google.com/p/gdata-python-client/)')
  sys.exit(1)


class MockClient(object):
  """A mock class for gdata.projecthosting.client.ProjectHostingClient.

  Mocking the very simple method invocations for get_issues() and
  get_comments().
  """

  def _CreateIssues(self, n_issues):
    feed = gdata.projecthosting.data.IssuesFeed()
    for i in xrange(n_issues):
      feed.entry.append(gdata.projecthosting.data.IssueEntry(
          title=atom.data.Title(text='title'),
          content=atom.data.Content(text='http://www.content.com'),
          id=atom.data.Id(text='/' + str(i)),
          status=gdata.projecthosting.data.Status(text='Unconfirmed'),
          state=gdata.projecthosting.data.State(text='open'),
          label=[gdata.projecthosting.data.Label('label1')],
          author=[atom.data.Author(name=atom.data.Name(text='author'))]))
    return feed

  def get_issues(self, project_name, query):
    """Get issues using mock object without calling the issue tracker API.

    Based on query argument, this returns the dummy issues. The number of
    dummy issues are specified in query.text_query.

    Args:
      project_name: A string for project name in the issue tracker.
      query: A query object for querying the issue tracker.

    Returns:
       A IssuesFeed object that contains a simple test issue.
    """
    n_issues = 1
    if query.text_query.isdigit():
      n_issues = int(query.text_query)
    return self._CreateIssues(n_issues)

  def get_comments(self, project_name, issue_id):
    """Get comments using mock object without calling the issue tracker API.

    Args:
      project_name: A string for project name in the issue tracker.
      issue_id: Issue_id string.

    Returns:
       A CommentsFeed object that contains a simple test comment.
    """
    feed = gdata.projecthosting.data.CommentsFeed()
    feed.entry = [gdata.projecthosting.data.CommentEntry(
        id=atom.data.Id(text='/0'),
        content=atom.data.Content(text='http://www.comments.com'),
        updated=atom.data.Updated(text='Updated'),
        author=[atom.data.Author(name=atom.data.Name(text='cauthor'))])]
    return feed


class BugHunterUnitTest(unittest.TestCase):
  """Unit tests for the Bug Hunter class."""

  def setUp(self):
    self._old_client = gdata.projecthosting.client.ProjectHostingClient
    gdata.projecthosting.client.ProjectHostingClient = MockClient

  def tearDown(self):
    gdata.projecthosting.client.ProjectHostingClient = self._old_client

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

  def _GetIssue(self, n_issues):
    issues = []
    for i in xrange(n_issues):
      issues.append({'issue_id': str(i), 'title': 'title', 'author': 'author',
                     'status': 'status', 'state': 'state',
                     'content': 'content', 'comments': [],
                     'labels': [], 'urls': []})
    return issues

  def testSetUpEmailSubjectMsg(self):
    bh = BugHunter(self._GetDefaultOption(False))
    subject, content = bh._SetUpEmailSubjectMsg(self._GetIssue(1))
    self.assertEquals(subject,
                      'BugHunter found 1 query title bug!')
    self.assertEquals(content,
                      ('<a href="http://code.google.com/p/chromium/issues/'
                       'list?can=2&colspec=ID+Pri+Mstone+ReleaseBlock+Area+'
                       'Feature+Status+Owner+Summary&cells=tiles&sort=-id&'
                       'q=steps">Used Query</a>: steps<br><br>The number of '
                       'issues : 1<br><ul><li><a href="http://crbug.com/0">0 '
                       'title</a> [] </li></ul>'))

  def testSetUpEmailSubjectMsgMultipleIssues(self):
    bh = BugHunter(self._GetDefaultOption(False))
    subject, content = bh._SetUpEmailSubjectMsg(self._GetIssue(2))
    self.assertEquals(subject,
                      'BugHunter found 2 query title bugs!')

  def testSetUpEmailSubjectMsgWith10DaysAgoAndAssertSubject(self):
    bh = BugHunter(self._GetDefaultOption(True))
    subject, _ = bh._SetUpEmailSubjectMsg(self._GetIssue(1))
    self.assertEquals(subject,
                      ('BugHunter found 1 query title bug in the past 10 '
                       'days!'))

  def testGetIssuesWithMockClient(self):
    bh = BugHunter(self._GetDefaultOption(False,
                                          query=('dummy')))
    expected_issues = [{'issue_id': '0', 'title': 'title', 'author': 'author',
                        'status': 'Unconfirmed', 'state': 'open',
                        'content': 'http://www.content.com',
                        'comments': '', 'labels': ['label1'],
                        'urls': ['http://www.content.com']}]
    self.assertEquals(expected_issues, bh.GetIssues())


class MockSmtp(object):
  """A mock class for SMTP."""

  def __init__(self, server):
    pass

  def sendmail(self, sender_email_address, receivers_email_addresses,
               msg):
    # TODO(imasaki): Do something here.
    return True

  def quit(self):
    pass


class BugHunterUtilsTest(unittest.TestCase):
  """Unit tests for the Bug Hunter utility."""

  def testStripHTML(self):
    self.assertEquals(BugHunterUtils.StripHTML('<p>X</p>'), 'X')

  def testStripHTMLEmpty(self):
    self.assertEquals(BugHunterUtils.StripHTML(''), '')

  def testSendEmail(self):
    smtplib.SMTP = MockSmtp
    self.assertEqual(BugHunterUtils.SendEmail('message', 'sender_email_address',
                                              'receivers_email_addresses',
                                              'subject'),
                     True)
