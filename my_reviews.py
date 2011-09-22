#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Get rietveld stats.

Example:
  - my_reviews.py -o me@chromium.org -Q  for stats for last quarter.
"""
import datetime
import optparse
import os
import sys

import rietveld


def print_reviews(owner, reviewer, created_after, created_before, instance_url):
  """Prints issues with the filter.

  Set with_messages=True to search() call bellow if you want each message too.
  If you only want issue numbers, use keys_only=True in the search() call.
  You can then use remote.get_issue_properties(issue, True) to get the data per
  issue.
  """
  remote = rietveld.Rietveld(instance_url, None, None)

  # See def search() in rietveld.py to see all the filters you can use.
  for issue in remote.search(
      owner=owner,
      reviewer=reviewer,
      created_after=created_after,
      created_before=created_before,
      keys_only=False,
      with_messages=False,
      ):
    # By default, hide commit-bot and the domain.
    reviewers = set(r.split('@', 1)[0] for r in issue['reviewers'])
    reviewers -= set(('commit-bot',))
    # Strip time.
    timestamp = issue['created'][:10]

    # More information is available, print issue.keys() to see them.
    print '%d: %s %s' % (issue['issue'], timestamp, ', '.join(reviewers))


def print_count(owner, reviewer, created_after, created_before, instance_url):
  remote = rietveld.Rietveld(instance_url, None, None)
  print len(list(remote.search(
      owner=owner,
      reviewer=reviewer,
      created_after=created_after,
      created_before=created_before,
      keys_only=False)))


def get_previous_quarter(today):
  """There are four quarters, 01-03, 04-06, 07-09, 10-12.

  If today is in the last month of a quarter, assume it's the current quarter
  that is requested.
  """
  year = today.year
  month = today.month - (today.month % 3) + 1
  if month <= 0:
    month += 12
    year -= 1
  if month > 12:
    month -= 12
    year += 1
  previous_month = month - 3
  previous_year = year
  if previous_month <= 0:
    previous_month += 12
    previous_year -= 1
  return (
      '%d-%02d-01' % (previous_year, previous_month),
      '%d-%02d-01' % (year, month))


def main():
  # Silence upload.py.
  rietveld.upload.verbosity = 0
  today = datetime.date.today()
  created_after, created_before = get_previous_quarter(today)
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '--count', action='store_true',
      help='Just count instead of printing individual issues')
  parser.add_option(
      '-o', '--owner', metavar='<email>', help='Filter on issue owner')
  parser.add_option(
      '-r', '--reviewer', metavar='<email>', help='Filter on issue reviewer')
  parser.add_option(
      '-c', '--created_after', metavar='<date>',
      help='Filter issues created after the date')
  parser.add_option(
      '-C', '--created_before', metavar='<date>',
      help='Filter issues create before the date')
  parser.add_option(
      '-Q', '--last_quarter', action='store_true',
      help='Use last quarter\'s dates, e.g. %s to %s' % (
        created_after, created_before))
  parser.add_option(
      '-i', '--instance_url', metavar='<host>',
      default='http://codereview.chromium.org',
      help='Host to use, default is %default')
  # Remove description formatting
  parser.format_description = lambda x: parser.description
  options, args = parser.parse_args()
  if args:
    parser.error('Args unsupported')
  if not options.owner and not options.reviewer:
    options.owner = os.environ['EMAIL_ADDRESS']
    if '@' not in options.owner:
      parser.error('Please specify at least -o or -r')
    print >> sys.stderr, 'Defaulting to owner=%s' % options.owner
  if options.last_quarter:
    options.created_after = created_after
    options.created_before = created_before
    print >> sys.stderr, 'Using range %s to %s' % (
        options.created_after, options.created_before)
  if options.count:
    print_count(
        options.owner, options.reviewer,
        options.created_after, options.created_before,
        options.instance_url)
  else:
    print_reviews(
        options.owner, options.reviewer,
        options.created_after, options.created_before,
        options.instance_url)
  return 0


if __name__ == '__main__':
  sys.exit(main())
