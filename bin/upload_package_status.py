#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Support uploading a csv file to a Google Docs spreadsheet."""

import optparse
import os
import pickle
import re
import sys

import gdata.spreadsheet.service

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.table as table
import chromite.lib.operation as operation
import chromite.lib.upgrade_table as utable
import merge_package_status as mps

REAL_SS_KEY = 'tJuuSuHmrEMqdL5b8dkgBIA'
TEST_SS_KEY = 't3RE08XLO2f1vTiV4N-w2ng'
PKGS_WS_NAME = 'Packages'
DEPS_WS_NAME = 'Dependencies'

oper = operation.Operation('upload_package_status')
oper.verbose = True # Without verbose Info messages don't show up.

def _PrepColForSS(col):
  """Translate a column name for spreadsheet "list" interface."""
  # Spreadsheet "list" interface requires column names to be
  # all lowercase and with no spaces.
  return col.lower().replace(' ', '')

def _PrepRowForSS(row):
  """Make sure spreadsheet handles all values in row as strings."""
  for key in row:
    val = row[key]
    row[key] = _PrepValForSS(val)
  return row

# Regex to detect values that the spreadsheet will auto-format as numbers.
_NUM_REGEX = re.compile(r'^[\d\.]+$')
def _PrepValForSS(val):
  """Make sure spreadsheet handles this value as a string."""
  if val and _NUM_REGEX.match(val):
    return "'" + val
  return val

def _ScrubValFromSS(val):
  """Remove string indicator prefix if found."""
  if val and val[0] == "'":
    return val[1:]
  return val

class Uploader(object):
  """Uploads portage package status data from csv file to Google spreadsheet."""

  __slots__ = ['_docs_token', # Google Docs login token
               '_email',      # Google Docs account email
               '_gd_client',  # gdata.spreadsheet.service.SpreadsheetsService
               '_password',   # Google Docs account password
               '_ss_key',     # Google Doc spreadsheet key (string)
               '_table',      # table.Table
               '_verbose',    # boolean
               '_ws_key',     # Worksheet key (string)
               '_ws_name',    # Worksheet name (string)
               ]

  ID_COL = utable.UpgradeTable.COL_PACKAGE
  SOURCE = "Uploaded from CSV"

  def __init__(self, table_obj, verbose=False):
    self._table = table_obj
    self._gd_client = None
    self._docs_token = None
    self._email = None
    self._password = None
    self._ss_key = None
    self._verbose = verbose
    self._ws_key = None
    self._ws_name = None

  def _Verbose(self, msg):
    """Print |msg| if _verbose is true."""
    if self._verbose:
      print msg

  def _GetWorksheetKey(self, ss_key, ws_name):
    """Get the worksheet key with name |ws_name| in spreadsheet |ss_key|."""
    feed = self._gd_client.GetWorksheetsFeed(ss_key)
    # The worksheet key is the last component in the URL (after last '/')
    for entry in feed.entry:
      if ws_name == entry.title.text:
        return entry.id.text.split('/')[-1]

    oper.Die("Unable to find worksheet '%s' in spreadsheet '%s'" %
             (ws_name, ss_key))

  def LoginDocsWithEmailPassword(self):
    """Set up and connect the Google Doc client using email/password."""
    gd_client = gdata.spreadsheet.service.SpreadsheetsService()
    gd_client.source = self.SOURCE
    gd_client.email = self._email
    gd_client.password = self._password
    gd_client.ProgrammaticLogin()
    self._gd_client = gd_client
    self._docs_token = gd_client.GetClientLoginToken()

  def LoginDocsWithToken(self):
    """Set up and connect Google Doc client using login token."""
    print "Using existing credentials for docs login."
    gd_client = gdata.spreadsheet.service.SpreadsheetsService()
    gd_client.source = self.SOURCE
    gd_client.SetClientLoginToken(self._docs_token)
    self._gd_client = gd_client
    self._email = None
    self._password = None

  def SetCreds(self, email, password):
    """Set the email/password credentials to be used."""
    self._email = email
    self._password = password

  def LoadCreds(self, filepath):
    """Store email/password credentials to |filepath|."""
    print "Loading Docs login credentials from '%s'" % filepath
    # Read email from first line and password from second.
    f = open(filepath, 'r')
    (self._email, self._password) = (l.strip() for l in f.readlines())
    f.close()

  def StoreCreds(self, filepath):
    """Load email/password credentials from |filepath|."""
    print "Storing Docs login credentials to '%s'" % filepath
    # Simply write email on first line and password on second.
    f = open(filepath, 'w')
    f.write(self._email + '\n')
    f.write(self._password + '\n')
    f.close()

  def LoadDocsToken(self, filepath):
    """Loads docs login token from |filepath|."""
    self._docs_token = None
    try:
      print "Loading Docs login token from '%s'" % filepath
      f = open(filepath, 'r')
      obj = pickle.load(f)
      f.close()
      if obj.has_key('docs_token'):
        self._docs_token = obj['docs_token']
    except IOError:
      print 'Unable to load credentials from file at %s' % filepath

    return bool(self._docs_token)

  def StoreDocsToken(self, filepath):
    """Stores docs login token to disk at |filepath|."""
    obj = {}
    if self._docs_token:
      obj['docs_token'] = self._docs_token
    try:
      print "Storing Docs login token to '%s'" % filepath
      f = open(filepath, 'w')
      pickle.dump(obj, f)
      f.close()
    except IOError:
      print 'Unable to store credentials'

  def _GetSSRowForPackage(self, package):
    """Find the spreadsheet row object corresponding to Package=|package|."""
    query = gdata.spreadsheet.service.ListQuery()
    query._SetSpreadsheetQuery('package = "%s"' % package)
    feed = self._gd_client.GetListFeed(self._ss_key, self._ws_key, query=query)
    if len(feed.entry) == 1:
      return feed.entry[0]

    if len(feed.entry) > 1:
      raise LookupError("More than one row in spreadsheet with Package=%s" %
                        package)

    if len(feed.entry) == 0:
      return None

  def Upload(self, ss_key, ws_name):
    """Upload |_table| to the given Google Spreadsheet.

    The spreadsheet is identified the spreadsheet key |ss_key|.
    The worksheet within that spreadsheet is identified by the
    worksheet name |ws_name|.
    """
    self._ss_key = ss_key
    self._ws_name = ws_name
    self._ws_key = self._GetWorksheetKey(ss_key, ws_name)

    self._Verbose("Details by package: S=Same, C=Changed, A=Added, D=Deleted")
    rows_unchanged, rows_updated, rows_inserted = self._UploadChangedRows()
    rows_deleted, rows_with_owner_deleted = self._DeleteOldRows()

    print("Final row stats: %d changed, %d added, %d deleted, %d same." %
          (rows_updated, rows_inserted, rows_deleted, rows_unchanged))
    if rows_with_owner_deleted:
      print("WARNING: %d rows with owner entry deleted, see above warnings." %
            rows_with_owner_deleted)
    else:
      print("No rows with owner entry were deleted.")

  def _UploadChangedRows(self):
    """Upload all rows in table that need to be changed in spreadsheet."""
    print("Uploading to worksheet '%s' of spreadsheet '%s' now." %
          (self._ws_name, self._ss_key))

    rows_unchanged, rows_updated, rows_inserted = (0, 0, 0)

    # Go over all rows in csv table.  Identify existing row by the 'Package'
    # column.  Either update existing row or create new one.
    for csv_row in self._table:
      csv_package = csv_row[self.ID_COL]
      ss_row = self._GetSSRowForPackage(csv_package)

      # Seed new row values from csv_row values, with column translation.
      new_row = dict((_PrepColForSS(key), csv_row[key]) for key in csv_row)

      if ss_row:
        changed = []

        # For columns that are in spreadsheet but not in csv, grab values
        # from spreadsheet so they are not overwritten by UpdateRow.
        for col in ss_row.custom:
          ss_val = _ScrubValFromSS(ss_row.custom[col].text)
          if col not in new_row:
            new_row[col] = ss_val
          elif (ss_val or new_row[col]) and ss_val != new_row[col]:
            changed.append("%s='%s'->'%s'" % (col, ss_val, new_row[col]))
          elif ss_row.custom[col].text != _PrepValForSS(ss_val):
            changed.append("%s=str'%s'" % (col, new_row[col]))

        if changed:
          self._gd_client.UpdateRow(ss_row, _PrepRowForSS(new_row))
          rows_updated += 1
          self._Verbose("C %-30s: %s" %
                        (csv_package, ', '.join(changed)))
        else:
          rows_unchanged += 1
          self._Verbose("S %-30s:" % csv_package)
      else:
        self._gd_client.InsertRow(_PrepRowForSS(new_row),
                                  self._ss_key, self._ws_key)
        rows_inserted += 1
        row_descr_list = []
        for col in sorted(new_row.keys()):
          if col != self.ID_COL:
            row_descr_list.append("%s='%s'" % (col, new_row[col]))
        self._Verbose("A %-30s: %s" %
                      (csv_package, ', '.join(row_descr_list)))

    return (rows_unchanged, rows_updated, rows_inserted)

  def _DeleteOldRows(self):
    """Delete all rows from spreadsheet that not found in table."""
    print "Checking for rows in spreadsheet that should be deleted now."

    rows_deleted, rows_with_owner_deleted = (0, 0)

    # Also need to delete rows in spreadsheet that are not in csv table.
    list_feed = self._gd_client.GetListFeed(self._ss_key, self._ws_key)
    for ss_row in list_feed.entry:
      ss_id_col = _PrepColForSS(self.ID_COL)
      ss_package = _ScrubValFromSS(ss_row.custom[ss_id_col].text)

      # See whether this row is in csv table.
      csv_rows = self._table.GetRowsByValue({ self.ID_COL: ss_package })
      if not csv_rows:
        # Row needs to be deleted from spreadsheet.
        owner_val = None
        owner_notes_val = None
        row_descr_list = []
        for col in sorted(ss_row.custom.keys()):
          if col == 'owner':
            owner_val = ss_row.custom[col].text
          if col == 'ownernotes':
            owner_notes_val = ss_row.custom[col].text

          # Don't include ID_COL value in description, it is in prefix already.
          if col != _PrepColForSS(self.ID_COL):
            val = ss_row.custom[col].text
            row_descr_list.append("%s='%s'" % (col, val))

        self._Verbose("D %-30s: %s" %
                      (ss_package, ', '.join(row_descr_list)))
        if owner_val or owner_notes_val:
          rows_with_owner_deleted += 1
          print("WARNING: Deleting spreadsheet row with owner entry:\n" +
                "  %-30s: Owner=%s, Owner Notes=%s" %
                (ss_package, owner_val, owner_notes_val))

        self._gd_client.DeleteRow(ss_row)
        rows_deleted += 1

    return (rows_deleted, rows_with_owner_deleted)

def LoadTable(table_file):
  """Load csv |table_file| into a table.  Return table."""
  print "Loading csv table from '%s'." % (table_file)
  csv_table = table.Table.LoadFromCSV(table_file)
  return csv_table

def main():
  """Main function."""
  usage = 'Usage: %prog [options] csv_file'
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('--cred-file', dest='cred_file', type='string',
                    action='store', default=None,
                    help="File for reading/writing Docs login email/password.")
  parser.add_option('--email', dest='email', type='string',
                    action='store', default=None,
                    help="Email for Google Doc user")
  parser.add_option('--password', dest='password', type='string',
                    action='store', default=None,
                    help="Password for Google Doc user")
  parser.add_option('--ss-key', dest='ss_key', type='string',
                    action='store', default=None,
                    help="Key of spreadsheet to upload to")
  parser.add_option('--test-spreadsheet', dest='test_ss',
                    action='store_true', default=False,
                    help="Upload to the testing spreadsheet.")
  parser.add_option('--token-file', dest='token_file', type='string',
                    action='store', default=None,
                    help="File for reading/writing Docs login token.")
  parser.add_option('--verbose', dest='verbose',
                    action='store_true', default=False,
                    help="Show details about packages..")

  (options, args) = parser.parse_args()

  if len(args) < 1:
    parser.print_help()
    oper.Die("One csv_file is required.")

  # If email or password provided, the other is required.  If neither is
  # provided, then either cred_file or token_file must be provided and
  # be a real file.
  if options.email or options.password:
    if not (options.email and options.password):
      parser.print_help()
      oper.Die("The email/password options must be used together.")
  elif not options.token_file and not options.cred_file:
    parser.print_help()
    oper.Die("Either email/password, cred-file, or token-file required.")
  elif not ((options.token_file and os.path.exists(options.token_file)) or
            (options.cred_file and os.path.exists(options.cred_file))):
    parser.print_help()
    oper.Die("Without email/password cred-file or token-file must exist.")
  elif (options.token_file and os.path.exists(options.token_file) and
        options.cred_file and os.path.exists(options.cred_file)):
    parser.print_help()
    oper.Die("Without email/password, both cred-file and token-file " +
                 "is ambiguous.")

  # --ss-key and --test-spreadsheet are mutually exclusive.
  if options.ss_key and options.test_ss:
    parser.print_help()
    oper.Die("Cannot specify --ss-key and --test-spreadsheet together.")

  # Load the given csv file.
  csv_table = LoadTable(args[0])

  # Prepare table for upload.
  mps.FinalizeTable(csv_table)

  # Prepare the Google Doc client for uploading.
  uploader = Uploader(csv_table, verbose=options.verbose)
  if options.email and options.password:
    # Login with supplied email/password
    uploader.SetCreds(email=options.email, password=options.password)
    uploader.LoginDocsWithEmailPassword()

    # If cred or token file given, write them out now.
    if options.token_file:
      uploader.StoreDocsToken(options.token_file)
    if options.cred_file:
      uploader.StoreCreds(options.cred_file)
  elif options.cred_file and os.path.exists(options.cred_file):
    # Login by reading email/password from supplied file.
    uploader.LoadCreds(options.cred_file)
    uploader.LoginDocsWithEmailPassword()

    # If token file given, write it out now.
    if options.token_file:
      uploader.StoreDocsToken(options.token_file)
  elif options.token_file and os.path.exists(options.token_file):
    # Login with token from supplied file.
    uploader.LoadDocsToken(options.token_file)
    uploader.LoginDocsWithToken()

  ss_key = options.ss_key
  ws_names = [PKGS_WS_NAME, DEPS_WS_NAME]
  if not ss_key:
    if options.test_ss:
      ss_key = TEST_SS_KEY # For testing with backup spreadsheet
    else:
      ss_key = REAL_SS_KEY

  for ws_name in ws_names:
    uploader.Upload(ss_key, ws_name=ws_name)

if __name__ == '__main__':
  main()
