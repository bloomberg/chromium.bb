#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Get rietveld stats about the review you done, or forgot to do.

Example:
  - my_reviews.py -r me@chromium.org -Q  for stats for last quarter.
"""
import datetime
import optparse
import os
import sys

import rietveld


def username(email):
  return email.split('@', 1)[0]


def print_reviews(reviewer, created_after, created_before, instance_url):
  """Prints issues the dude reviewed."""
  remote = rietveld.Rietveld(instance_url, None, None)
  total = 0
  actually_reviewed = 0

  # See def search() in rietveld.py to see all the filters you can use.
  for issue in remote.search(
      reviewer=reviewer,
      created_after=created_after,
      created_before=created_before,
      with_messages=True,
      ):
    total += 1
    # By default, hide commit-bot and the domain.
    reviewers = set(username(r) for r in issue['reviewers'])
    reviewers -= set(['commit-bot'])
    # Strip time.
    timestamp = issue['created'][:10]
    if any(
      username(msg['sender']) == username(reviewer)
      for msg in issue['messages']):
      reviewed = ' x '
      actually_reviewed += 1
    else:
      reviewed = '   '

    # More information is available, print issue.keys() to see them.
    print '%7d %s %s O:%-15s  R:%s' % (
        issue['issue'],
        timestamp,
        reviewed,
        username(issue['owner_email']),
        ', '.join(reviewers))
  percent = 0.
  if total:
    percent = (actually_reviewed * 100. / total)
  print 'You actually reviewed %d issues out of %d (%1.1f%%)' % (
      actually_reviewed, total, percent)


def print_count(reviewer, created_after, created_before, instance_url):
  remote = rietveld.Rietveld(instance_url, None, None)
  print len(list(remote.search(
      reviewer=reviewer,
      created_after=created_after,
      created_before=created_before,
      keys_only=True)))


def get_previous_quarter(today):
  """There are four quarters, 01-03, 04-06, 07-09, 10-12.

  If today is in the last month of a quarter, assume it's the current quarter
  that is requested.
  """
  end_year = today.year
  end_month = today.month - (today.month % 3) + 1
  if end_month <= 0:
    end_year -= 1
    end_month += 12
  if end_month > 12:
    end_year += 1
    end_month -= 12
  end = '%d-%02d-01' % (end_year, end_month)
  begin_year = end_year
  begin_month = end_month - 3
  if begin_month <= 0:
    begin_year -= 1
    begin_month += 12
  begin = '%d-%02d-01' % (begin_year, begin_month)
  return begin, end


def main():
  # Silence upload.py.
  rietveld.upload.verbosity = 0
  today = datetime.date.today()
  begin, end = get_previous_quarter(today)
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '--count', action='store_true',
      help='Just count instead of printing individual issues')
  parser.add_option(
      '-r', '--reviewer', metavar='<email>',
      default=os.environ.get('EMAIL_ADDRESS'),
      help='Filter on issue reviewer, default=%default')
  parser.add_option(
      '-b', '--begin', metavar='<date>',
      help='Filter issues created after the date')
  parser.add_option(
      '-e', '--end', metavar='<date>',
      help='Filter issues created before the date')
  parser.add_option(
      '-Q', '--last_quarter', action='store_true',
      help='Use last quarter\'s dates, e.g. %s to %s' % (
        begin, end))
  parser.add_option(
      '-i', '--instance_url', metavar='<host>',
      default='http://codereview.chromium.org',
      help='Host to use, default is %default')
  # Remove description formatting
  parser.format_description = lambda x: parser.description
  options, args = parser.parse_args()
  if args:
    parser.error('Args unsupported')
  if not options.reviewer:
    parser.error('$EMAIL_ADDRESS is not set, please use -r')
  print >> sys.stderr, 'Searching for reviews by %s' % options.reviewer
  if options.last_quarter:
    options.begin = begin
    options.end = end
    print >> sys.stderr, 'Using range %s to %s' % (
        options.begin, options.end)
  if options.count:
    print_count(
        options.reviewer,
        options.begin,
        options.end,
        options.instance_url)
  else:
    print_reviews(
        options.reviewer,
        options.begin,
        options.end,
        options.instance_url)
  return 0


if __name__ == '__main__':
  sys.exit(main())
