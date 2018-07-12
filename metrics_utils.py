#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import scm
import subprocess2
import sys

from third_party import colorama


NOTICE_COUNTDOWN_HEADER = (
  '*****************************************************\n'
  '*  METRICS COLLECTION WILL START IN %2d EXECUTIONS   *'
)
NOTICE_COLLECTION_HEADER = (
  '*****************************************************\n'
  '*      METRICS COLLECTION IS TAKING PLACE           *'
)
NOTICE_FOOTER = (
  '*                                                   *\n'
  '* For more information, and for how to disable this *\n'
  '* message, please see metrics.README.md in your     *\n'
  '* depot_tools checkout.                             *\n'
  '*****************************************************\n'
)


def get_python_version():
  """Return the python version in the major.minor.micro format."""
  return '{v.major}.{v.minor}.{v.micro}'.format(v=sys.version_info)


def return_code_from_exception(exception):
  """Returns the exit code that would result of raising the exception."""
  if exception is None:
    return 0
  if isinstance(exception[1], SystemExit):
    return exception[1].code
  return 1


def seconds_to_weeks(duration):
  """Transform a |duration| from seconds to weeks approximately.

  Drops the lowest 19 bits of the integer representation, which ammounts to
  about 6 days.
  """
  return int(duration) >> 19


def get_repo_timestamp(path_to_repo):
  """Get an approximate timestamp for the upstream of |path_to_repo|.

  Returns the top two bits of the timestamp of the HEAD for the upstream of the
  branch path_to_repo is checked out at.
  """
  # Get the upstream for the current branch. If we're not in a branch, fallback
  # to HEAD.
  try:
    upstream = scm.GIT.GetUpstreamBranch(path_to_repo)
  except subprocess2.CalledProcessError:
    upstream = 'HEAD'

  # Get the timestamp of the HEAD for the upstream of the current branch.
  p = subprocess2.Popen(
      ['git', '-C', path_to_repo, 'log', '-n1', upstream, '--format=%at'],
      stdout=subprocess2.PIPE, stderr=subprocess2.PIPE)
  stdout, _ = p.communicate()

  # If there was an error, give up.
  if p.returncode != 0:
    return None

  # Get the age of the checkout in weeks.
  return seconds_to_weeks(stdout.strip())


def print_notice(countdown):
  """Print a notice to let the user know the status of metrics collection."""
  colorama.init()
  print colorama.Fore.RED + '\033[1m'
  if countdown:
    print NOTICE_COUNTDOWN_HEADER % countdown
  else:
    print NOTICE_COLLECTION_HEADER
  print NOTICE_FOOTER + colorama.Style.RESET_ALL
