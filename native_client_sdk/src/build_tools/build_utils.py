# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Small utility library of python functions used during SDK building.
"""

import os
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
  info = lastchange.FetchVersionInfo(None)
  if info.url.startswith('/trunk/'):
    return 'trunk.%s' % info.revision
  else:
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

  Returns:
    The Chrome revision as a string. e.g. "12345"
  '''
  return lastchange.FetchVersionInfo(None).revision
