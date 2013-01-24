# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for FFmpeg repository.

Does the following:
- Displays a message indicating that all changes must be pushed to SVN.
- Prevents changes inside the git-svn and svn repositories.
- Warns users when a change is made without updating the README file.
"""

import re
import subprocess


# Setup some basic ANSI colors.
BOLD_RED = '\033[1;31m'
BOLD_BLUE = '\033[1;34m'


def Color(color, string):
  return ''.join([color, string, '\033[m'])


def _CheckForMainRepository(input_api, output_api):
  """Ensure no commits are allowed to the Subversion repository."""
  try:
    stdout = subprocess.Popen('git config --get remote.origin.url', shell=True,
                              cwd=input_api.PresubmitLocalPath(),
                              stdout=subprocess.PIPE).communicate()[0]
    if 'chromium/third_party/ffmpeg.git' in stdout:
      return []
  except Exception, e:
    # Not everyone has a working Git configuration, so print the exception text
    # and move on.
    print e

  return [output_api.PresubmitError('\n'.join([
      'Commits to the FFmpeg repository must be made through Git. The easiest',
      'way to do this is to switch to using NewGit:\n',
      '   http://code.google.com/p/chromium/wiki/UsingNewGit\n',
      'Alternatively, you can clone the repository directly using:\n',
      '   git clone http://git.chromium.org/chromium/third_party/ffmpeg.git']))]


def _WarnAboutManualSteps():
  """Warn about the manual steps required for rolling the FFmpeg repository."""
  print Color(BOLD_RED, '[ REMEMBER ]'.center(70, '*'))
  print Color(
      BOLD_BLUE, '\n'.join([
          'Updates to FFmpeg require the following additional manual steps:',
          '  - Push of new sources from Git to Subversion. See sync_svn.py.',
          '  - External DEPS roll.',
          '  - If new source files were added, generate_gyp.py must be run.\n',
          'For help, please consult the following resources:',
          '  - chromium/README.chromium help text.',
          '  - Chromium development list: chromium-dev@chromium.org',
          '  - "Communication" section of http://www.chromium.org/developers']))
  print Color(BOLD_RED, 70 * '*')


def _WarnIfReadmeIsUnchanged(input_api, output_api):
  """Warn if the README file hasn't been updated with change notes."""
  readme_re = re.compile(r'.*[/\\]?chromium[/\\]patches[/\\]README$')
  for f in input_api.AffectedFiles():
    if readme_re.match(f.LocalPath()):
      return []

  return [output_api.PresubmitPromptWarning('\n'.join([
      'FFmpeg changes detected without any update to chromium/patches/README,',
      'it\'s good practice to update this file with a note about your changes.'
  ]))]


def CheckChangeOnUpload(input_api, output_api):
  _WarnAboutManualSteps()
  results = []
  results.extend(_CheckForMainRepository(input_api, output_api))
  results.extend(_WarnIfReadmeIsUnchanged(input_api, output_api))
  return results
