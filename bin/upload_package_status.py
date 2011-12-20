#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Support uploading a csv file to a Google Docs spreadsheet."""

import optparse
import os
import sys

import gdata.spreadsheet.service

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.gdata_lib as gdata_lib
import chromite.lib.table as table
import chromite.lib.operation as operation
import chromite.lib.upgrade_table as utable
import merge_package_status as mps

REAL_SS_KEY = '0AsXDKtaHikmcdEp1dVN1SG1yRU1xZEw1Yjhka2dCSUE'
TEST_SS_KEY = '0AsXDKtaHikmcdDlQMjI3ZDdPVGc4Rkl3Yk5OLWxjR1E'
PKGS_WS_NAME = 'Packages'
DEPS_WS_NAME = 'Dependencies'

oper = operation.Operation('upload_package_status')

class Uploader(object):
  """Uploads portage package status data from csv file to Google spreadsheet."""

  __slots__ = ['_creds',      # gdata_lib.Creds object
               '_gd_client',  # gdata.spreadsheet.service.SpreadsheetsService
               '_ss_key',     # Google Doc spreadsheet key (string)
               '_table',      # table.Table
               '_ws_key',     # Worksheet key (string)
               '_ws_name',    # Worksheet name (string)
               ]

  ID_COL = utable.UpgradeTable.COL_PACKAGE
  SOURCE = "Uploaded from CSV"

  def __init__(self, creds, table_obj):
    self._creds = creds
    self._table = table_obj
    self._gd_client = None
    self._ss_key = None
    self._ws_key = None
    self._ws_name = None

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
    gd_client = gdata_lib.RetrySpreadsheetsService()

    gd_client.source = self.SOURCE
    gd_client.email = self._creds.user
    gd_client.password = self._creds.password
    gd_client.ProgrammaticLogin()
    self._gd_client = gd_client

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

    oper.Info("Details by package: S=Same, C=Changed, A=Added, D=Deleted")
    rows_unchanged, rows_updated, rows_inserted = self._UploadChangedRows()
    rows_deleted, rows_with_owner_deleted = self._DeleteOldRows()

    oper.Notice("Final row stats: %d changed, %d added, %d deleted, %d same." %
                (rows_updated, rows_inserted, rows_deleted, rows_unchanged))
    if rows_with_owner_deleted:
      oper.Warning("%d rows with owner entry deleted, see above warnings." %
                   rows_with_owner_deleted)
    else:
      oper.Notice("No rows with owner entry were deleted.")

  def _UploadChangedRows(self):
    """Upload all rows in table that need to be changed in spreadsheet."""
    oper.Notice("Uploading to worksheet '%s' of spreadsheet '%s' now." %
                (self._ws_name, self._ss_key))

    rows_unchanged, rows_updated, rows_inserted = (0, 0, 0)

    # Go over all rows in csv table.  Identify existing row by the 'Package'
    # column.  Either update existing row or create new one.
    for csv_row in self._table:
      csv_package = csv_row[self.ID_COL]
      ss_row = self._GetSSRowForPackage(csv_package)

      # Seed new row values from csv_row values, with column translation.
      new_row = dict((gdata_lib.PrepColNameForSS(key),
                      csv_row[key]) for key in csv_row)

      if ss_row:
        changed = []

        # For columns that are in spreadsheet but not in csv, grab values
        # from spreadsheet so they are not overwritten by UpdateRow.
        for col in ss_row.custom:
          ss_val = gdata_lib.ScrubValFromSS(ss_row.custom[col].text)
          if col not in new_row:
            new_row[col] = ss_val
          elif (ss_val or new_row[col]) and ss_val != new_row[col]:
            changed.append("%s='%s'->'%s'" % (col, ss_val, new_row[col]))
          elif ss_row.custom[col].text != gdata_lib.PrepValForSS(ss_val):
            changed.append("%s=str'%s'" % (col, new_row[col]))

        if changed:
          self._gd_client.UpdateRow(ss_row, gdata_lib.PrepRowForSS(new_row))
          rows_updated += 1
          oper.Info("C %-30s: %s" % (csv_package, ', '.join(changed)))
        else:
          rows_unchanged += 1
          oper.Info("S %-30s:" % csv_package)
      else:
        self._gd_client.InsertRow(gdata_lib.PrepRowForSS(new_row),
                                  self._ss_key, self._ws_key)
        rows_inserted += 1
        row_descr_list = []
        for col in sorted(new_row.keys()):
          if col != self.ID_COL:
            row_descr_list.append("%s='%s'" % (col, new_row[col]))
        oper.Info("A %-30s: %s" % (csv_package, ', '.join(row_descr_list)))

    return (rows_unchanged, rows_updated, rows_inserted)

  def _DeleteOldRows(self):
    """Delete all rows from spreadsheet that not found in table."""
    oper.Notice("Checking for rows in spreadsheet that should be deleted now.")

    rows_deleted, rows_with_owner_deleted = (0, 0)

    # Also need to delete rows in spreadsheet that are not in csv table.
    list_feed = self._gd_client.GetListFeed(self._ss_key, self._ws_key)
    for ss_row in list_feed.entry:
      ss_id_col = gdata_lib.PrepColNameForSS(self.ID_COL)
      ss_package = gdata_lib.ScrubValFromSS(ss_row.custom[ss_id_col].text)

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
          if col != gdata_lib.PrepColNameForSS(self.ID_COL):
            val = ss_row.custom[col].text
            row_descr_list.append("%s='%s'" % (col, val))

        oper.Info("D %-30s: %s" % (ss_package, ', '.join(row_descr_list)))
        if owner_val or owner_notes_val:
          rows_with_owner_deleted += 1
          oper.Notice("WARNING: Deleting spreadsheet row with owner entry:\n" +
                      "  %-30s: Owner=%s, Owner Notes=%s" %
                      (ss_package, owner_val, owner_notes_val))

        self._gd_client.DeleteRow(ss_row)
        rows_deleted += 1

    return (rows_deleted, rows_with_owner_deleted)

def LoadTable(table_file):
  """Load csv |table_file| into a table.  Return table."""
  oper.Notice("Loading csv table from '%s'." % (table_file))
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
  parser.add_option('--verbose', dest='verbose',
                    action='store_true', default=False,
                    help="Show details about packages..")

  (options, args) = parser.parse_args()

  oper.verbose = options.verbose

  if len(args) < 1:
    parser.print_help()
    oper.Die("One csv_file is required.")

  # If email or password provided, the other is required.  If neither is
  # provided, then cred_file must be provided and be a real file.
  if options.email or options.password:
    if not (options.email and options.password):
      parser.print_help()
      oper.Die("The email/password options must be used together.")
  elif not options.cred_file:
    parser.print_help()
    oper.Die("Either email/password or cred-file required.")
  elif not (options.cred_file and os.path.exists(options.cred_file)):
    parser.print_help()
    oper.Die("Without email/password cred-file must exist.")

  # --ss-key and --test-spreadsheet are mutually exclusive.
  if options.ss_key and options.test_ss:
    parser.print_help()
    oper.Die("Cannot specify --ss-key and --test-spreadsheet together.")

  # Prepare credentials for spreadsheet access.
  creds = gdata_lib.Creds(cred_file=options.cred_file,
                          user=options.email,
                          password=options.password)

  # Load the given csv file.
  csv_table = LoadTable(args[0])

  # Prepare table for upload.
  mps.FinalizeTable(csv_table)

  # Prepare the Google Doc client for uploading.
  uploader = Uploader(creds, csv_table)
  uploader.LoginDocsWithEmailPassword()

  ss_key = options.ss_key
  ws_names = [PKGS_WS_NAME, DEPS_WS_NAME]
  if not ss_key:
    if options.test_ss:
      ss_key = TEST_SS_KEY # For testing with backup spreadsheet
    else:
      ss_key = REAL_SS_KEY

  for ws_name in ws_names:
    uploader.Upload(ss_key, ws_name=ws_name)

  # If email/password given, as well as path to cred_file, write
  # credentials out to that location.
  if options.email and options.password and options.cred_file:
    creds.StoreCreds(options.cred_file)

if __name__ == '__main__':
  main()
