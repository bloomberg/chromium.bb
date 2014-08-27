# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Small utility library of python functions used during SDK building.
"""

import os
import re
import sys

# pylint: disable=E0602

# Reuse last change utility code.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(SCRIPT_DIR)))
sys.path.append(os.path.join(SRC_DIR, 'build/util'))
import lastchange


# Location of chrome's version file.
VERSION_PATH = os.path.join(SRC_DIR, 'chrome', 'VERSION')


def ChromeVersion():
  '''Extract chrome version from src/chrome/VERSION + svn.

  Returns:
    Chrome version string or trunk + svn rev.
  '''
  info = FetchVersionInfo()
  if info.url.startswith('/trunk/'):
    return 'trunk.%s' % info.revision
  else:
    return ChromeVersionNoTrunk()


def ChromeVersionNoTrunk():
  '''Extract the chrome version from src/chrome/VERSION.
  Ignore whether this is a trunk build.

  Returns:
    Chrome version string.
  '''
  exec(open(VERSION_PATH).read())
  return '%s.%s.%s.%s' % (MAJOR, MINOR, BUILD, PATCH)


def ChromeMajorVersion():
  '''Extract chrome major version from src/chrome/VERSION.

  Returns:
    Chrome major version.
  '''
  exec(open(VERSION_PATH, 'r').read())
  return str(MAJOR)


def ChromeRevision():
  '''Extract chrome revision from svn.

     Now that the Chrome source-of-truth is git, this will return the
     Cr-Commit-Position instead. fortunately, this value is equal to the SVN
     revision if one exists.

  Returns:
    The Chrome revision as a string. e.g. "12345"
  '''
  return FetchVersionInfo().revision


def NaClRevision():
  '''Extract NaCl revision from svn.

  Returns:
    The NaCl revision as a string. e.g. "12345"
  '''
  nacl_dir = os.path.join(SRC_DIR, 'native_client')
  return FetchVersionInfo(nacl_dir, 'native_client').revision


def FetchVersionInfo(directory=None,
                     directory_regex_prior_to_src_url='chrome|blink|svn'):
  """
  Returns the last change (in the form of a branch, revision tuple),
  from some appropriate revision control system.

  TODO(binji): This is copied from lastchange.py. Remove this function and use
  lastchange.py directly when the dust settles. (see crbug.com/406783)
  """
  svn_url_regex = re.compile(
      r'.*/(' + directory_regex_prior_to_src_url + r')(/.*)')

  version_info = (lastchange.FetchSVNRevision(directory, svn_url_regex) or
                  lastchange.FetchGitSVNRevision(directory, svn_url_regex) or
                  FetchGitCommitPosition(directory))
  if not version_info:
    version_info = lastchange.VersionInfo(None, None)
  return version_info


def FetchGitCommitPosition(directory=None):
  """
  Return the "commit-position" of the Chromium git repo. This should be
  equivalent to the SVN revision if one eixsts.

  This is a copy of the (recently reverted) change in lastchange.py.
  TODO(binji): Move this logic to lastchange.py when the dust settles.
  (see crbug.com/406783)
  """
  proc = lastchange.RunGitCommand(directory,
                                  ['show', '-s', '--format=%B', 'HEAD'])
  pos = ''
  if proc:
    output = proc.communicate()[0]
    if proc.returncode == 0 and output:
      for line in reversed(output.splitlines()):
        match = re.search('Cr-Commit-Position: .*@{#(\d+)}', line)
        if match:
          pos = match.group(1)
  if not pos:
    return lastchange.VersionInfo(None, None)
  return lastchange.VersionInfo('git', pos)
