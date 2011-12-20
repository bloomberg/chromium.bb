#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for interacting with gdata (i.e. Google Docs, Tracker, etc)."""

import getpass
import os
import re

import gdata.spreadsheet.service

import chromite.lib.operation as operation

# pylint: disable=E0702,W0613,E1101

CRED_FILE = os.path.join(os.environ['HOME'], '.gdata_cred.txt')

oper = operation.Operation('gdata_lib')

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
    # Prefer user/password if given.
    if user:
      if not user.endswith('@chromium.org'):
        user = '%s@chromium.org' % user

      if not password:
        password = getpass.getpass('Tracker password for %s:' % user)

      self.user = user
      self.password = password

    elif cred_file and os.path.exists(cred_file):
      self.LoadCreds(cred_file)

  def LoadCreds(self, filepath):
    """Load email/password credentials from |filepath|."""
    # Read email from first line and password from second.

    with open(filepath, 'r') as f:
      (self.user, self.password) = (l.strip() for l in f.readlines())
    oper.Notice('Loaded Docs/Tracker login credentials from "%s"' % filepath)

  def StoreCreds(self, filepath):
    """Store email/password credentials to |filepath|."""
    oper.Notice('Storing Docs/Tracker login credentials to "%s"' % filepath)
    # Simply write email on first line and password on second.
    with open(filepath, 'w') as f:
      f.write(self.user + '\n')
      f.write(self.password + '\n')


class RetrySpreadsheetsService(gdata.spreadsheet.service.SpreadsheetsService):
  """The entire purpose of this class is to remove some flakiness from
  interactions with Google Docs spreadsheet service.  It does this by
  overriding the low-level http 'request' method to support retries.
  The original is implemented in base class atom.service.AtomService.

  As documented in http://code.google.com/p/chromium-os/issues/detail?id=23819,
  the two errors we see in the builder are:
  gdata.service.RequestError and gdata.service.CaptchaRequired
  Both of these errors, as seen so far, come from a 403 status response.  For
  now that is the only status response that warrants a retry.
  """

  TRY_MAX = 5
  RETRYABLE_STATUSES = (403,)

  def request(self, *args, **kwargs):
    """Override request method to allow for retries with flaky http requests

    This is retry wrapper around the request method of the base class.

    Retry when the status value on the return object is in RETRYABLE_STATUSES,
    and run up to TRY_MAX times.  If successful (whether or not retries
    were necessary) return the last return value returned from base method.
    If unsuccessful return the first return value returned from base method.
    """
    func = gdata.spreadsheet.service.SpreadsheetsService.request

    first_retval = None
    for try_ix in xrange(1, self.TRY_MAX + 1):
      retval = func(self, *args, **kwargs)
      if retval.status not in self.RETRYABLE_STATUSES:
        return retval
      else:
        oper.Warning('Retry-able HTTP request failure (status=%d), try %d/%d' %
                     (retval.status, try_ix, self.TRY_MAX))
        if not first_retval:
          first_retval = retval

    oper.Warning('Giving up on HTTP request after %d tries' % self.TRY_MAX)
    return first_retval


# Support having this module test itself if run as __main__, by leveraging
# the gdata_lib_unittest module.
# Also, the unittests serve as extra documentation.
if __name__ == '__main__':
  import gdata_lib_unittest
  gdata_lib_unittest.unittest.main(gdata_lib_unittest)
