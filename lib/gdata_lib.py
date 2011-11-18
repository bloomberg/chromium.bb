#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for interacting with gdata (i.e. Google Docs, Tracker, etc)."""

import getpass
import os
import re

CRED_FILE = os.path.join(os.environ['HOME'], '.gdata_cred.txt')

_BAD_COL_CHARS_REGEX = re.compile(r'[ /]')
def PrepColNameForSS(col):
  """Translate a column name for spreadsheet interface."""
  # Spreadsheet interface requires column names to be
  # all lowercase and with no spaces or other special characters.
  return _BAD_COL_CHARS_REGEX.sub('', col.lower())

def PrepRowForSS(row):
  """Make sure spreadsheet handles all values in row as strings."""
  return dict((key, PrepValForSS(val)) for key, val in row.items())

# Regex to detect values that the spreadsheet will auto-format as numbers.
_NUM_REGEX = re.compile(r'^[\d\.]+$')
def PrepValForSS(val):
  """Make sure spreadsheet handles this value as a string."""
  if val and _NUM_REGEX.match(val):
    return "'" + val
  return val

def ScrubValFromSS(val):
  """Remove string indicator prefix if found."""
  if val and val[0] == "'":
    return val[1:]
  return val

class Creds(object):
  """Class to manage user/password credentials."""

  __slots__ = [
    'cred_file',   # Path to credentials file
    'password',    # User password
    'user',        # User account (foo@chromium.org)
    ]

  def __init__(self, cred_file=None, user=None, password=None):
    if cred_file and os.path.exists(cred_file):
      self.LoadCreds(cred_file)
    elif user:
      if not user.endswith('@chromium.org'):
        user = '%s@chromium.org' % user

      if not password:
        password = getpass.getpass('Tracker password for %s:' % user)

      self.user = user
      self.password = password

      if cred_file:
        self.StoreCreds(cred_file)

  def LoadCreds(self, filepath):
    """Load email/password credentials from |filepath|."""
    # Read email from first line and password from second.

    with open(filepath, 'r') as f:
      (self.user, self.password) = (l.strip() for l in f.readlines())
    print 'Loaded Docs/Tracker login credentials from "%s"' % filepath

  def StoreCreds(self, filepath):
    """Store email/password credentials to |filepath|."""
    print 'Storing Docs/Tracker login credentials to "%s"' % filepath
    # Simply write email on first line and password on second.
    with open(filepath, 'w') as f:
      f.write(self.user + '\n')
      f.write(self.password + '\n')

# Support having this module test itself if run as __main__, by leveraging
# the gdata_lib_unittest module.
# Also, the unittests serve as extra documentation.
if __name__ == '__main__':
  import gdata_lib_unittest
  gdata_lib_unittest.unittest.main(gdata_lib_unittest)
