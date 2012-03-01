#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script queries the Chromium issue tracker and e-mails the results.

It queries issue tracker using Issue Tracker API. The query
parameters can be specified by command-line arguments. For example, with the
following command:

  'python bug_hunter.py -q video Status:Unconfirmed OR audio Status:Unconfirmed
   -s sender@chromium.org -r receiver@chromium.org -v 100 -u days'

You will find all 'Unconfirmed' issues created in the last 100 days containing
'video' or 'audio' in their content/comments. The content of these issues are
sent to receiver@chromium.org.

TODO(imasaki): users can specify the interval as say: "100d" for "100 days".

There are two limitations in the current implementation of issue tracker API
and UI:
* only outermost OR is valid. For example, the query
  'video OR audio Status:Unconfirmed' is translated into
  'video OR (audio AND Status:Unconfirmed)'
* brackets are not supported. For example, the query
  '(video OR audio) Status:Unconfirmed' does not work.

You need to install following to run this script
  gdata-python-client (http://code.google.com/p/gdata-python-client/)
  rfc3339.py (http://henry.precheur.org/projects/rfc3339)

Links:
* Chromium issue tracker: http://code.google.com/p/chromium/issues/list
* Issue tracker API: http://code.google.com/p/support/wiki/IssueTrackerAPI
* Search tips for the issue tracker:
    http://code.google.com/p/chromium/issues/searchtips
"""

import csv
import datetime
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
import logging
from operator import itemgetter
import optparse
import re
import smtplib
import socket
import sys
import urllib

try:
  import gdata.data
  import gdata.projecthosting.client
except ImportError:
  logging.error('gdata-client needs to be installed. Please install\n'
                'and try again (http://code.google.com/p/gdata-python-client/)')
  sys.exit(1)

try:
  import rfc3339
except ImportError:
  logging.error('rfc3339 needs to be installed. Please install\n'
                'and try again (http://henry.precheur.org/projects/rfc3339)')
  sys.exit(1)

# A list of default values.
_DEFAULT_INTERVAL_UNIT = 'hours'
_DEFAULT_ISSUE_ELEMENT_IN_EMAIL = ('author', 'status', 'state', 'content',
                                   'comments', 'labels', 'urls')
_DEFAULT_PROJECT_NAME = 'chromium'
_DEFAULT_QUERY_TITLE = 'potential media bugs'
_DEFAULT_QUERY = ('video -has:Feature -has:Owner -label:nomedia '
                  'status:Unconfirmed OR audio -has:Feature -has:Owner '
                  '-label:nomedia status:Unconfirmed')
_DEFAULT_OUTPUT_FILENAME = 'output.csv'
_DETAULT_MAX_COMMENTS = 1000

_INTERVAL_UNIT_CHOICES = ('hours', 'days', 'weeks')

# URLs in this list are excluded from URL extraction from bug
# content/comments. Each list element should not contain the url ending in
# '/'. For example, the element should be 'http://www.google.com' but not
# 'http://www.google.com/'
_URL_EXCLUSION_LIST = ('http://www.youtube.com/html5',
                       'http://www.google.com')
_ISSUE_ELEMENT_IN_EMAIL_CHOICES = ('issue_id', 'author', 'status', 'state',
                                   'content', 'comments', 'labels', 'urls',
                                   'mstone')


def ParseArgs():
  """Returns options dictionary from parsed command line arguments."""
  parser = optparse.OptionParser()

  parser.add_option('-e', '--email-entries',
                    help=('A comma-separated list of issue entries that are '
                          'sent in the email content. '
                          'Possible strings are %s. Default: %%default.' %
                          ', '.join(_ISSUE_ELEMENT_IN_EMAIL_CHOICES)),
                    default=','.join(_DEFAULT_ISSUE_ELEMENT_IN_EMAIL))
  parser.add_option('-l', '--max-comments',
                    help=('The maximum number of comments returned for each '
                          'issue in a reverse chronological order. '
                          'Default: %default.'),
                    type='int', default=_DETAULT_MAX_COMMENTS)
  parser.add_option('-o', '--output-filename',
                    help=('Filename for result output in CSV format. '
                          'Default: %default.'),
                    default=_DEFAULT_OUTPUT_FILENAME, metavar='FILE')
  parser.add_option('-p', '--project-name', default=_DEFAULT_PROJECT_NAME,
                    help='Project name string. Default: %default')
  parser.add_option('-q', '--query', default=_DEFAULT_QUERY,
                    help=('Query to be used to find bugs. The detail can be '
                          'found in Chromium Issue tracker page '
                          'http://code.google.com/p/chromium/issues/searchtips.'
                          ' Default: "%default".'))
  parser.add_option('-r', '--receiver-email-address',
                    help="Receiver's email address (Required).")
  parser.add_option('-s', '--sender-email-address',
                    help="Sender's email address (Required).")
  parser.add_option('-t', '--query-title',
                    default=_DEFAULT_QUERY_TITLE, dest='query_title',
                    help=('Query title string used in the subject of the '
                          'result email. Default: %default.'))
  parser.add_option('-u', '--interval_unit', default=_DEFAULT_INTERVAL_UNIT,
                    choices=_INTERVAL_UNIT_CHOICES,
                    help=('Unit name for |interval_value|. Valid options are '
                          '%s. Default: %%default' % (
                              ', '.join(_INTERVAL_UNIT_CHOICES))))
  parser.add_option('-v', '--interval-value', type='int',
                    help=('Interval value to find bugs. '
                          'The script looks for bugs during '
                          'that interval (up to now). This option is used in '
                          'conjunction with |--interval_unit| option. '
                          'The script looks for all bugs if this is not '
                          'specified.'))

  options = parser.parse_args()[0]

  options.email_entries = options.email_entries.split(',')
  options.email_entries = [entry for entry in options.email_entries
                           if entry in _ISSUE_ELEMENT_IN_EMAIL_CHOICES]
  if not options.email_entries:
    logging.warning('No issue elements in email in option. '
                    'Default email entries will be used.')
    options.email_entries = _DEFAULT_ISSUE_ELEMENT_IN_EMAIL
  logging.info('The following is the issue elements in email: %s ' + (
      ', '.join(options.email_entries)))
  return options


class BugHunter(object):
  """This class queries issue trackers and e-mails the results."""

  _ISSUE_SEARCH_LINK_BASE = ('http://code.google.com/p/chromium/issues/list?'
                             'can=2&colspec=ID+Pri+Mstone+ReleaseBlock+Area'
                             '+Feature+Status+Owner+Summary&cells=tiles'
                             '&sort=-id')
  # TODO(imasaki): Convert these into template library.
  _EMAIL_ISSUE_TEMPLATE = ('<li><a href="http://crbug.com/%(issue_id)s">'
                           '%(issue_id)s %(title)s</a> ')
  _EMAIL_SUBJECT_TEMPLATE = ('BugHunter found %(n_issues)d %(query_title)s '
                             'bug%(plural)s%(time_msg)s!')
  _EMAIL_MSG_TEMPLATE = ('<a href="%(link_base)s&q=%(unquote_query_text)s">'
                         'Used Query</a>: %(query_text)s<br><br>'
                         'The number of issues : %(n_issues)d<br>'
                         '<ul>%(issues)s</ul>')

  def __init__(self, options):
    """Sets up initial state for Bug Hunter.

    Args:
      options: Command-line options.
    """
    self._client = gdata.projecthosting.client.ProjectHostingClient()
    self._options = options
    self._issue_template = BugHunter._EMAIL_ISSUE_TEMPLATE
    for entry in options.email_entries:
      self._issue_template += '%%(%s)s ' % entry
    self._issue_template += '</li>'

  def GetComments(self, issue_id, max_comments):
    """Get comments for a issue.

    Args:
      issue_id: Issue id for each issue in the issue tracker.
      max_comments: The maximum number of comments to be returned. The comments
        are returned in a reverse chronological order.

    Returns:
      A list of (author name, comments, updated time) tuples.
    """
    comments_feed = self._client.get_comments(self._options.project_name,
                                              issue_id)
    comment_list = [(comment.content.text, comment.author[0].name.text,
                     comment.updated.text)
                    for comment
                    in list(reversed(comments_feed.entry))[0:max_comments]]
    return comment_list

  def GetIssues(self):
    """Get issues from issue tracker and return them.

    Returns:
      A list of issues in descending order by issue_id. Each element in the
        list is a dictionary where the keys are 'issue_id', 'title', 'author',
        'status', 'state', 'content', 'comments', 'labels', 'urls'.
        Returns an empty list when there is no matching issue.
    """
    min_time = None
    if self._options.interval_value:
      # Issue Tracker Data API uses RFC 3339 timestamp format, For example:
      # 2005-08-09T10:57:00-08:00
      # (http://code.google.com/p/support/wiki/IssueTrackerAPIPython)
      delta = datetime.timedelta(
          **{self._options.interval_unit: self._options.interval_value})
      dt = datetime.datetime.now() - delta
      min_time = rfc3339.rfc3339(dt)

    query = gdata.projecthosting.client.Query(text_query=self._options.query,
                                              max_results=1000,
                                              published_min=min_time)

    feed = self._client.get_issues(self._options.project_name, query=query)
    if not feed.entry:
      logging.info('No issues available to match query %s.',
                   self._options.query)
      return []
    issues = []
    for entry in feed.entry:
      # The fully qualified id is a URL. We just want the number.
      issue_id = entry.id.text.split('/')[-1]
      if not issue_id.isdigit():
        logging.warning('Issue_id is not correct: %s. Skipping.', issue_id)
        continue
      label_list = [label.text for label in entry.label]
      comments = ''
      if 'comments' in self._options.email_entries:
        comments = ''.join(
            [''.join(comment) if not comment else ''
             for comment
             in self.GetComments(issue_id, self._options.max_comments)])
      content = BugHunterUtils.StripHTML(entry.content.text)
      url_list = list(
          set(re.findall(r'(https?://\S+)', content + comments)))
      url_list = [url for url in url_list
                  if not url.rstrip('/') in _URL_EXCLUSION_LIST]
      mstone = ''
      r = re.compile(r'Mstone-(\d*)')
      for label in label_list:
        m = r.search(label)
        if m:
          mstone = m.group(1)
      issues.append(
          {'issue_id': issue_id, 'title': entry.title.text,
           'author': entry.author[0].name.text,
           'status': entry.status.text if entry.status is not None else '',
           'state': entry.state.text if entry.state is not None else '',
           'content': content, 'mstone': mstone, 'comments': comments,
           'labels': label_list, 'urls': url_list})
    return sorted(issues, key=itemgetter('issue_id'), reverse=True)

  def _SetUpEmailSubjectMsg(self, issues):
    """Set up email subject and its content.

    Args:
      issues: Please refer to the return value in GetIssues().

    Returns:
      A tuple of two strings (email subject and email content).
    """
    time_msg = ''
    if self._options.interval_value:
      time_msg = ' in the past %s %s%s' % (
          self._options.interval_value, self._options.interval_unit[:-1],
          's' if self._options.interval_value > 1 else '')
    subject = BugHunter._EMAIL_SUBJECT_TEMPLATE % {
        'n_issues': len(issues),
        'query_title': self._options.query_title,
        'plural': 's' if len(issues) > 1 else '',
        'time_msg': time_msg}
    content = BugHunter._EMAIL_MSG_TEMPLATE % {
        'link_base': BugHunter._ISSUE_SEARCH_LINK_BASE,
        'unquote_query_text': urllib.quote(self._options.query),
        'query_text': self._options.query,
        'n_issues': len(issues),
        'issues': ''.join(
            [self._issue_template % issue for issue in issues])}
    return (subject, content)

  def SendResultEmail(self, issues):
    """Send result email.

    Args:
      issues: Please refer to the return value in GetIssues().
    """
    subject, content = self._SetUpEmailSubjectMsg(issues)
    BugHunterUtils.SendEmail(
        content, self._options.sender_email_address,
        self._options.receiver_email_address, subject)

  def WriteIssuesToFileInCSV(self, issues, filename):
    """Write issues to a file in CSV format.

    Args:
      issues: Please refer to the return value in GetIssues().
      filename: File name for CSV file.
    """
    with open(filename, 'w') as f:
      writer = csv.writer(f)
      # Write header first.
      writer.writerow(issues[0].keys())
      for issue in issues:
        writer.writerow(
            [unicode(value).encode('utf-8') for value in issue.values()])


class BugHunterUtils(object):
  """Utility class for Bug Hunter."""

  @staticmethod
  def StripHTML(string_with_html):
    """Strip HTML tags from string.

    Args:
      string_with_html: A string with HTML tags.

    Returns:
      A string without HTML tags.
    """
    return re.sub('<[^<]+?>', '', string_with_html)

  @staticmethod
  def SendEmail(message, sender_email_address, receivers_email_address,
                subject):
    """Send email using localhost's mail server.

    Args:
      message: Email message to be sent.
      sender_email_address: Sender's email address.
      receivers_email_address: Receiver's email address.
      subject: Email subject.

    Returns:
      True if successful; False, otherwise.
    """
    try:
      html = '<html><head></head><body>%s</body></html>' % message
      msg = MIMEMultipart('alternative')
      msg['Subject'] = subject
      msg['From'] = sender_email_address
      msg['To'] = receivers_email_address
      msg.attach(MIMEText(html.encode('utf-8'), 'html', _charset='utf-8'))
      smtp_obj = smtplib.SMTP('localhost')
      smtp_obj.sendmail(sender_email_address, receivers_email_address,
                        msg.as_string())
      logging.info('Successfully sent email.')
      smtp_obj.quit()
      return True
    except smtplib.SMTPException:
      logging.exception('Authentication failed, unable to send email.')
    except (socket.gaierror, socket.error, socket.herror):
      logging.exception('Unable to send email.')
    return False


def Main():
  ops = ParseArgs()
  bh = BugHunter(ops)
  issues = bh.GetIssues()
  if issues and ops.sender_email_address and ops.receiver_email_address:
    bh.SendResultEmail(issues)
  if issues:
    bh.WriteIssuesToFileInCSV(issues, ops.output_filename)


if __name__ == '__main__':
  Main()
