# -*- coding: utf-8 -
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Determine the time between ebuild uprevs over some time range.

TODO(evanhernandez): Move this to scripts/ once it has been hardened.
"""

from __future__ import division
from __future__ import print_function

import collections
import datetime
import os

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import portage_util
from chromite.scripts import cros_mark_as_stable


DATE_FORMAT = '%Y-%m-%d'
SECONDS_PER_DAY = 60 * 60 * 24

# A minimal representation of a git commit.
#
# Fields:
#   - id (str): The commit hash.
#   - timestamp (str): The unix timestamp of the commit.
#   - subject (str): The commit subject (i.e. first line of commit message).
Commit = collections.namedtuple('Commit', ['id', 'timestamp', 'subject'])


def get_directory_commits(directory, start_date=None, end_date=None):
  """Get all commits in the given directory.

  Args:
    directory (str): The directory in question. Must be in a git project.
    start_date (datetime.datetime): The earliest datetime to consider, if any.
    end_date (datetime.datetime): The latest datetime to consider, if any.

  Returns:
    list[Commits]: The commits relating to that directory.
  """
  # TODO(evanhernandez): I am not sure how --after/--until consider timezones.
  # For a script like this, the differences are probably negligible, but I
  # would be happier if it guaranteed correctness.
  start_date = start_date and start_date.strftime(DATE_FORMAT)
  end_date = end_date and end_date.strftime(DATE_FORMAT)

  output = git.Log(directory, format='format:"%h|%cd|%s"', after=start_date,
                   until=end_date, reverse=True, date='unix', paths=[directory])
  if not output:
    return []
  logging.debug(output)

  commit_lines = [l.strip() for l in output.splitlines() if l.strip()]
  return [Commit(*cl.split('|', 2)) for cl in commit_lines]


def get_uprev_commits(commits):
  """Find all uprev commits amongs the given git commits.

  Preserves order.

  Args:
    commits (list[Commit]): The git commits in question.

  Returns:
    list[Commit]: Only commits that were uprevs of some ebuild.
  """
  uprev_commits = []
  for commit in commits:
    # Only return the ones with the cros_mark_as_stable commit message.
    # TODO(evanhernandez): Yuck. Need a more robust method here...
    if cros_mark_as_stable.GIT_COMMIT_SUBJECT in commit.subject:
      logging.debug('Found uprev commit: %s|%s', commit.id, commit.subject)
      uprev_commits.append(commit)
  return uprev_commits


def get_commit_timestamps(commits):
  """Get all commit timestamps for the given ebuild.

  Args:
    commits (list[Commit]): The commits in question.

  Returns:
    list[int]: The uprev commit unix timestamps, in order.
  """
  return [int(commit.timestamp) for commit in commits]


def get_average_timestamp_delta_days(timestamps):
  """Return the delta in seconds between each consecutive timestamp.

  Args:
    timestamps (list[int]): The unix timestamps.

  Returns:
    list[int]: The deltas (in seconds) between consecutive timestamps.
  """
  if not len(timestamps) > 2:
    raise ValueError(
        'Need >= 2 timestamps to compute deltas, found: %r' % timestamps)

  deltas = []
  for first, second in zip(timestamps, timestamps[1:]):
    delta = second - first
    assert delta > 0, 'Unexpected negative delta between uprevs.'
    deltas.append(delta)
  average_delta_seconds = sum(deltas) / len(deltas)
  return average_delta_seconds // SECONDS_PER_DAY


def get_parser():
  """Returns the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('board', help='Board of interest.')
  parser.add_argument('package', help='The package of interest.')

  parse_datetime = lambda s: datetime.datetime.strptime(s, DATE_FORMAT)
  parser.add_argument(
      '-s', '--start-date',
      type=parse_datetime,
      help='Earliest date to consider uprevs. Defaults to beginning of time.')
  parser.add_argument(
      '-e', '--end-date', type=parse_datetime,
      help='Latest date to consider uprevs. Defaults to present day.')

  return parser


def main(argv):
  parser = get_parser()
  options = parser.parse_args(argv)
  options.Freeze()

  board = options.board
  package = options.package
  ebuild_path = portage_util.FindEbuildForBoardPackage(package, board)
  if not ebuild_path:
    cros_build_lib.Die('Could not find package %s for board %s.',
                       package, board)
  logging.info('Found corresponding ebuild at: %s', ebuild_path)
  ebuild = portage_util.EBuild(ebuild_path)

  start_date = options.start_date
  end_date = options.end_date
  if start_date and end_date and start_date > end_date:
    cros_build_lib.Die('Start date must be before end date.')

  ebuild_commits = get_directory_commits(
      os.path.dirname(ebuild.ebuild_path), start_date=start_date,
      end_date=end_date)
  logging.info('Found %d commits for ebuild.', len(ebuild_commits))

  ebuild_uprev_commits = get_uprev_commits(ebuild_commits)
  ebuild_uprev_commit_count = len(ebuild_uprev_commits)
  logging.info('%d of those commits were uprevs.', ebuild_uprev_commit_count)
  if ebuild_uprev_commit_count < 2:
    cros_build_lib.Die(
        'Alas, you need at least 2 uprevs to compute uprev frequency. '
        'Try setting a larger time range?',
        ebuild_uprev_commit_count)

  ebuild_uprev_timestamps = get_commit_timestamps(ebuild_uprev_commits)
  average_delta_days = get_average_timestamp_delta_days(ebuild_uprev_timestamps)

  logging.info(
      'Package %s for %s was upreved every %.2f days on average.',
      ebuild.package, board, average_delta_days)
