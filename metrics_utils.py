#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import scm
import subprocess2
import sys

from third_party import colorama


# Current version of metrics recording.
# When we add new metrics, the version number will be increased, we display the
# user what has changed, and ask the user to agree again.
CURRENT_VERSION = 0

APP_URL = 'https://cit-cli-metrics.appspot.com'

EMPTY_LINE = (
  '*                                                   *'
)
NOTICE_COUNTDOWN_HEADER = (
  '*****************************************************\n'
  '*  METRICS COLLECTION WILL START IN %2d EXECUTIONS   *'
)
NOTICE_COLLECTION_HEADER = (
  '*****************************************************\n'
  '*      METRICS COLLECTION IS TAKING PLACE           *'
)
NOTICE_VERSION_CHANGE_HEADER = (
  '*****************************************************\n'
  '*       WE ARE COLLECTING ADDITIONAL METRICS        *'
)
NOTICE_FOOTER = (
  '* For more information, and for how to disable this *\n'
  '* message, please see metrics.README.md in your     *\n'
  '* depot_tools checkout.                             *\n'
  '*****************************************************\n'
)

CHANGE_NOTICE = {
  # No changes for version 0
  0: '',
}


KNOWN_PROJECT_URLS = {
  'https://chrome-internal.googlesource.com/chrome/ios_internal',
  'https://chrome-internal.googlesource.com/infra/infra_internal',
  'https://chromium.googlesource.com/breakpad/breakpad',
  'https://chromium.googlesource.com/chromium/src',
  'https://chromium.googlesource.com/chromium/tools/depot_tools',
  'https://chromium.googlesource.com/crashpad/crashpad',
  'https://chromium.googlesource.com/external/gyp',
  'https://chromium.googlesource.com/external/naclports',
  'https://chromium.googlesource.com/infra/goma/client',
  'https://chromium.googlesource.com/infra/infra',
  'https://chromium.googlesource.com/native_client/',
  'https://chromium.googlesource.com/syzygy',
  'https://chromium.googlesource.com/v8/v8',
  'https://dart.googlesource.com/sdk',
  'https://pdfium.googlesource.com/pdfium',
  'https://skia.googlesource.com/buildbot',
  'https://skia.googlesource.com/skia',
  'https://webrtc.googlesource.com/src',
}


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
  print(colorama.Fore.RED + '\033[1m', file=sys.stderr, end='')
  if countdown:
    print(NOTICE_COUNTDOWN_HEADER % countdown, file=sys.stderr)
  else:
    print(NOTICE_COLLECTION_HEADER, file=sys.stderr)
  print(EMPTY_LINE, file=sys.stderr)
  print(NOTICE_FOOTER + colorama.Style.RESET_ALL, file=sys.stderr)


def print_version_change(config_version):
  """Print a notice to let the user know we are collecting more metrics."""
  colorama.init()
  print(colorama.Fore.RED + '\033[1m', file=sys.stderr, end='')
  print(NOTICE_VERSION_CHANGE_HEADER, file=sys.stderr)
  print(EMPTY_LINE, file=sys.stderr)
  for version in range(config_version + 1, CURRENT_VERSION + 1):
    print(CHANGE_NOTICE[version], file=sys.stderr)
    print(EMPTY_LINE, file=sys.stderr)
