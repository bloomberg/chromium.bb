#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Validate or replace the standard gdata authorization token."""

import filecmp
import optparse
import os
import shutil
import sys

ROOT = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, ROOT)

import chromite.buildbot.constants as constants
import chromite.lib.cros_build_lib as build_lib
import chromite.lib.operation as operation

MODULE = os.path.splitext(os.path.basename(__file__))[0]
oper = operation.Operation(MODULE)

TOKEN_FILE = os.path.join(os.environ['HOME'], '.gdata_token')
CRED_FILE = os.path.join(os.environ['HOME'], '.gdata_cred.txt')


def _ChrootPathToExternalPath(path):
  """Translate |path| inside chroot to external path to same location."""
  if path:
    return os.path.join(constants.SOURCE_ROOT,
                        constants.DEFAULT_CHROOT_DIR,
                        path.lstrip('/'))

  return None


class OutsideChroot(object):

  def __init__(self, args):
    self.args = args

  def Run(self):
    """Re-start |args| inside chroot and copy out auth file."""

    # Note that enter_chroot (cros_sdk) will automatically copy both
    # the token file and the cred file into the chroot, so no need
    # to do that here.

    # Rerun the same command that launched this run inside the chroot.
    cmd = [MODULE] + self.args
    result = build_lib.RunCommand(cmd, enter_chroot=True,
                                  print_cmd=False, error_code_ok=True)
    if result.returncode != 0:
      oper.Die('Token validation failed, exit code was %r.' %
               result.returncode)

    # Copy the token file back from chroot if different.
    chroot_token_file = _ChrootPathToExternalPath(TOKEN_FILE)
    if not os.path.exists(chroot_token_file):
      oper.Die('No token file generated inside chroot.')
    elif (not os.path.exists(TOKEN_FILE) or not
          filecmp.cmp(TOKEN_FILE, chroot_token_file)):
      oper.Notice('Copying new token file from chroot to %r' % TOKEN_FILE)
      shutil.copy2(chroot_token_file, TOKEN_FILE)
    else:
      oper.Notice('No change in token file.')


class InsideChroot(object):

  def __init__(self):
    self.creds = None
    self.gd_client = None

  def _ValidateToken(self):
    """Validate the existing token file."""
    # pylint: disable=W0404
    import gdata.service

    if not os.path.exists(TOKEN_FILE):
      oper.Warning('No current token file at %r.' % TOKEN_FILE)
      return False

    # Load token file, if it exists.
    self.creds.LoadAuthToken(TOKEN_FILE)

    oper.Notice('Attempting to log into Docs using auth token.')
    self.gd_client.source = 'Package Status'
    self.gd_client.SetClientLoginToken(self.creds.auth_token)

    try:
      # Try to access generic spreadsheets feed, which will check access.
      self.gd_client.GetSpreadsheetsFeed()

      # Token accepted.  We're done here.
      oper.Notice('Token validated.')
      return True
    except gdata.service.RequestError as ex:
      reason = ex[0]['reason']
      if reason == 'Token expired':
        return False

      raise

  def _GenerateToken(self):
    """Generate a new token from credentials."""
    # pylint: disable=W0404
    import gdata.service

    oper.Warning('Token has expired.  Will try to generate a new one.')
    self.creds.LoadCreds(CRED_FILE)
    self.gd_client.email = self.creds.user
    self.gd_client.password = self.creds.password

    try:
      self.gd_client.ProgrammaticLogin()
      self.creds.SetAuthToken(self.gd_client.GetClientLoginToken())

      oper.Notice('New token generated.  Saving now.')
      self.creds.StoreAuthToken(TOKEN_FILE)
      return True
    except gdata.service.BadAuthentication:
      oper.Error('Credentials from %r not accepted.'
                 '  Unable to generate new token.' % CRED_FILE)
      return False

  def Run(self):
    """Validate existing auth token or generate new one from credentials."""
    # pylint: disable=W0404
    import chromite.lib.gdata_lib as gdata_lib
    import gdata.spreadsheet.service

    self.creds = gdata_lib.Creds()
    self.gd_client = gdata.spreadsheet.service.SpreadsheetsService()

    if not self._ValidateToken():
      if not self._GenerateToken():
        oper.Die('Failed to validate or generate token.')


def _CreateParser():
  usage = 'Usage: %prog'
  epilog = ('\n'
            'Run outside of chroot to validate the gdata '
            'token file at %r or update it if it has expired.\n'
            'To update the token file there must be a valid '
            'credentials file at %r.\n'
            'If run inside chroot the updated token file is '
            'still valid but will not be preserved if chroot\n'
            'is deleted.\n' %
            (TOKEN_FILE, CRED_FILE))

  return optparse.OptionParser(usage=usage, epilog=epilog)


def main(args):
  """Main function."""
  # Create a copy of args just to be safe.
  args = list(args)

  # No actual options used, but --help is still supported.
  parser = _CreateParser()
  (_options, args) = parser.parse_args(args)

  if args:
    parser.print_help()
    oper.Die('No arguments allowed.')

  if build_lib.IsInsideChroot():
    InsideChroot().Run()
  else:
    OutsideChroot(args).Run()


if __name__ == '__main__':
  main(sys.argv[1:])
