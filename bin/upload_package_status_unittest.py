#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

import exceptions
import re
import shutil
import sys
import tempfile
import unittest

import gdata.spreadsheet.service as gdata_ss
import mox

import chromite.lib.cros_test_lib as test_lib
import chromite.lib.table as tablelib
import merge_package_status as mps
import upload_package_status as ups

# pylint: disable=W0212,R0904,E1120

def _WriteCredsFile(tmpfile_path, email, password):
  with open(tmpfile_path, 'w') as tmpfile:
    tmpfile.write(email + '\n')
    tmpfile.write(password + '\n')

class ModuleTest(test_lib.TestCase):
  """Test misc functionality of the upload_package_status module."""

  ROW_IN1 = {'Package': 'sys-fs/dosfstools',
             'Overlay': 'portage',
             'State': 'needs upgrade',
             'Current Version': '3.0.2',
             'Stable Upstream Version': '3.0.9',
             'Latest Upstream Version': '3.0.11',
             'Chrome OS Root Target': 'chromeos',
             }
  ROW_UP1 = {'package': 'sys-fs/dosfstools',
             'overlay': 'portage',
             'state': 'needs upgrade',
             'currentversion': "'3.0.2",
             'stableupstreamversion': "'3.0.9",
             'latestupstreamversion': "'3.0.11",
             'chromeosroottarget': 'chromeos',
             }

  def testPrepColForSS(self):
    """Column names in spreadsheet must be lower-case with no spaces."""
    cols = {'Package': 'package',
            'Overlay': 'overlay',
            'Current Version': 'currentversion',
            'Stable Upstream Version': 'stableupstreamversion',
            'Latest Upstream Version': 'latestupstreamversion',
            'Chrome OS Root Target': 'chromeosroottarget',
            }
    for col_in in cols:
      self.assertEquals(cols[col_in], ups._PrepColForSS(col_in))

  def testPrepValForSS(self):
    """Number-like vals should get a ' prefix to force string handling."""
    strs = ['chromiumos-overlay',
            'george',
            'chromeos chromeos-dev',
            '1.2.3-r1',
            ]
    nums = ['1',
            '1.2',
            '1.2.3',
            ]
    for val in strs:
      self.assertEquals(val, ups._PrepValForSS(val))

    for val in nums:
      self.assertEquals("'" + val, ups._PrepValForSS(val))

  def testPrepRowForSS(self):
    row = dict(self.ROW_IN1)
    ups._PrepRowForSS(row)
    for col in row:
      self.assertEquals(row[col], self.ROW_UP1[ups._PrepColForSS(col)])

  def testScrubValFromSS(self):
    vals = {"foo": "foo",
            "'foo": "foo",
            "1.2.3": "1.2.3",
            "'1.2.3": "1.2.3",
            }
    for val_in in vals:
      self.assertEquals(vals[val_in], ups._ScrubValFromSS(val_in))

class SSEntry(object):
  """Class to simulate one spreadsheet entry."""
  def __init__(self, text):
    self.text = text

class SSRow(object):
  """Class for simulating spreadsheet row."""
  def __init__(self, row, cols=None):
    self.custom = {}

    if not cols:
      # If columns not specified, then column order doesn't matter.
      cols = row.keys()
    for col in cols:
      ss_col = ups._PrepColForSS(col)
      val = row[col]
      ss_val = ups._PrepValForSS(val)
      self.custom[ss_col] = SSEntry(ss_val)

class SSFeed(object):
  """Class for simulating spreadsheet list feed."""
  def __init__(self, rows, cols=None):
    self.entry = []
    for row in rows:
      self.entry.append(SSRow(row, cols))

class UploaderTest(test_lib.MoxTestCase):
  """Test the functionality of upload_package_status.Uploader class."""

  COL_PKG = 'Package'
  COL_SLOT = 'Slot'
  COL_OVERLAY = 'Overlay'
  COL_STATUS = 'Status'
  COL_VER = 'Current Version'
  COL_STABLE_UP = 'Stable Upstream Version'
  COL_LATEST_UP = 'Latest Upstream Version'
  COL_TARGET = 'Chrome OS Root Target'
  COLS = [COL_PKG,
          COL_SLOT,
          COL_OVERLAY,
          COL_STATUS,
          COL_VER,
          COL_STABLE_UP,
          COL_LATEST_UP,
          COL_TARGET,
          ]

  ROW0 = {COL_PKG: 'lib/foo',
          COL_SLOT: '0',
          COL_OVERLAY: 'portage',
          COL_STATUS: 'needs upgrade',
          COL_VER: '3.0.2',
          COL_STABLE_UP: '3.0.9',
          COL_LATEST_UP: '3.0.11',
          COL_TARGET: 'chromeos',
          }
  ROW1 = {COL_PKG: 'sys-dev/bar',
          COL_SLOT: '0',
          COL_OVERLAY: 'chromiumos-overlay',
          COL_STATUS: 'needs upgrade',
          COL_VER: '1.2.3-r1',
          COL_STABLE_UP: '1.2.3-r2',
          COL_LATEST_UP: '1.2.4',
          COL_TARGET: 'chromeos-dev',
          }
  ROW2 = {COL_PKG: 'sys-dev/raster',
          COL_SLOT: '1',
          COL_OVERLAY: 'chromiumos-overlay',
          COL_STATUS: 'current',
          COL_VER: '1.2.3',
          COL_STABLE_UP: '1.2.3',
          COL_LATEST_UP: '1.2.4',
          COL_TARGET: 'chromeos-test',
          }

  EMAIL = 'knights@ni.com'
  PASSWORD = 'the'

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _MockUploader(self, table=None, gd_client=None):
    """Set up a mocked Uploader object."""
    uploader = self.mox.CreateMock(ups.Uploader)

    if not table:
      # Use default table
      table = self._CreateDefaultTable()

    for slot in ups.Uploader.__slots__:
      uploader.__setattr__(slot, None)

    uploader._verbose = False
    uploader._table = table
    uploader._gd_client = gd_client
    if gd_client:
      uploader._docs_token = gd_client.token

    return uploader

  def _MockGdClient(self, email=None, password=None):
    """Set up mocked gd_client object."""
    gd_client = self.mox.CreateMock(gdata_ss.SpreadsheetsService)

    gd_client.source = "Some Source"
    gd_client.email = email
    gd_client.password = password
    gd_client.token = "Some Token"

    return gd_client

  def _CreateDefaultTable(self):
    return self._CreateTableWithRows(self.COLS,
                                     [self.ROW0, self.ROW1])

  def _CreateTableWithRows(self, cols, rows):
    mytable = tablelib.Table(list(cols))
    if rows:
      for row in rows:
        mytable.AppendRow(dict(row))
    return mytable

  def testLoadTable(self):
    # Note that this test is not actually for method of Uploader class.

    self.mox.StubOutWithMock(tablelib.Table, 'LoadFromCSV')
    csv = 'any.csv'

    # Replay script
    tablelib.Table.LoadFromCSV(csv).AndReturn('loaded_table')
    self.mox.ReplayAll()

    # Verify
    self._StartCapturingOutput()
    loaded_table = ups.LoadTable(csv)
    self.assertEquals(loaded_table, 'loaded_table')
    self._StopCapturingOutput()

  def testSetCreds(self):
    mocked_uploader = self._MockUploader()

    # Replay script
    self.mox.ReplayAll()

    # Verify
    ups.Uploader.SetCreds(mocked_uploader, self.EMAIL, self.PASSWORD)
    self.assertEquals(self.EMAIL, mocked_uploader._email)
    self.assertEquals(self.PASSWORD, mocked_uploader._password)

  @test_lib.tempfile_decorator
  def testStoreLoadCreds(self):
    table = self._CreateDefaultTable()
    uploader = ups.Uploader(table)
    # Use SetCreds to bootstrap (it has a separate unit test)
    uploader.SetCreds(self.EMAIL, self.PASSWORD)

    self._StartCapturingOutput()

    self.assertEquals(uploader._email, self.EMAIL)
    self.assertEquals(uploader._password, self.PASSWORD)

    # Verify that Uploader can store/load creds
    uploader.StoreCreds(self.tempfile)
    uploader._email = None
    uploader._password = None
    self.assertFalse(uploader._email)
    self.assertFalse(uploader._password)

    uploader.LoadCreds(self.tempfile)
    self.assertEquals(uploader._email, self.EMAIL)
    self.assertEquals(uploader._password, self.PASSWORD)

    self._StopCapturingOutput()

  def testLoginDocsWithEmailPassword(self):
    mocked_uploader = self._MockUploader()
    ups.Uploader.SetCreds(mocked_uploader, self.EMAIL, self.PASSWORD)

    self.mox.StubOutWithMock(gdata_ss.SpreadsheetsService,
                             'ProgrammaticLogin')
    self.mox.StubOutWithMock(gdata_ss.SpreadsheetsService,
                             'GetClientLoginToken')

    # Replay script
    gdata_ss.SpreadsheetsService.ProgrammaticLogin()
    gdata_ss.SpreadsheetsService.GetClientLoginToken().AndReturn('Some Token')
    self.mox.ReplayAll()

    # Verify
    ups.Uploader.LoginDocsWithEmailPassword(mocked_uploader)
    self.assertEquals(mocked_uploader._gd_client.email, self.EMAIL)
    self.assertEquals(mocked_uploader._gd_client.password, self.PASSWORD)
    self.assertEquals(mocked_uploader._docs_token, 'Some Token')

  def testLoginDocsWithToken(self):
    mocked_uploader = self._MockUploader()
    mocked_uploader._docs_token = 'Some Token'

    self.mox.StubOutWithMock(gdata_ss.SpreadsheetsService,
                             'SetClientLoginToken')

    # Replay script
    gdata_ss.SpreadsheetsService.SetClientLoginToken('Some Token')
    self.mox.ReplayAll()

    # Verify
    self._StartCapturingOutput()
    self.assertFalse(mocked_uploader._gd_client)
    ups.Uploader.LoginDocsWithToken(mocked_uploader)
    self.assertTrue(isinstance(mocked_uploader._gd_client,
                               gdata_ss.SpreadsheetsService))
    self._StopCapturingOutput()

  def testGetSSRowForPackage(self):
    mocked_gd_client = self._MockGdClient()
    mocked_uploader = self._MockUploader(gd_client=mocked_gd_client)
    mocked_uploader._ss_key = 'Some ss_key'
    mocked_uploader._ws_key = 'Some ws_key'
    package = 'Some Package'

    # The only thing that matters is that only one row is in feed (array size 1)
    feed = SSFeed([{'Some Col': 'Some Val'}])

    self.mox.StubOutWithMock(gdata_ss.SpreadsheetsService, 'GetListFeed')

    def Verifier(query):
      # Check instance type and contents of query object.
      if not isinstance(query, gdata_ss.ListQuery):
        return False
      return query['sq'] == 'package = "%s"' % package
    mocked_gd_client.GetListFeed('Some ss_key', 'Some ws_key',
                                 query=mox.Func(Verifier)).AndReturn(feed)
    self.mox.ReplayAll()

    # Verify
    ups.Uploader._GetSSRowForPackage(mocked_uploader, package)
    self.mox.VerifyAll()

  def testUpload(self):
    mocked_uploader = self._MockUploader()
    ss_key = 'Some ss_key'
    ws_name = 'Some ws_name'
    ws_key = 'Some ws_key'

    # Replay script
    # TODO(mtennant): This approach just seems so brittle.  For example,
    # I would prefer this test allow any number of calls to _Verbose
    # at any time.  But I haven't figured out how.
    mocked_uploader._GetWorksheetKey(ss_key, ws_name).AndReturn(ws_key)
    mocked_uploader._Verbose(mox.IgnoreArg())
    mocked_uploader._UploadChangedRows().AndReturn(tuple([0, 1, 2]))
    mocked_uploader._DeleteOldRows().AndReturn(tuple([3, 4]))
    self.mox.ReplayAll()

    # Verify
    self._StartCapturingOutput()
    ups.Uploader.Upload(mocked_uploader, ss_key, ws_name)
    self.mox.VerifyAll()
    self._StopCapturingOutput()

  def testUploadChangedRows(self):
    mocked_gd_client = self._MockGdClient()
    mocked_uploader = self._MockUploader(gd_client=mocked_gd_client)
    mocked_uploader._ss_key = ss_key = 'Some ss_key'
    mocked_uploader._ws_key = ws_key = 'Some ws_key'

    def RowVerifier(ss_row, golden_row):
      for col in golden_row:
        ss_col = ups._PrepColForSS(col)
        golden_ss_val = ups._PrepValForSS(golden_row[col])
        if golden_ss_val != ss_row[ss_col]:
          return False
      return True
    row1_verifier = lambda row : RowVerifier(row, self.ROW1)

    # Pretend first row does not exist already in online spreadsheet.
    row0_pkg = self.ROW0[self.COL_PKG]
    mocked_uploader._GetSSRowForPackage(row0_pkg).AndReturn(None)
    mocked_gd_client.InsertRow(mox.IgnoreArg(), ss_key, ws_key)
    # Check for output indicating an added line (starts with A).
    mocked_uploader._Verbose(mox.Regex(r'^A '))

    # Pretend second row does already exist in online spreadsheet.
    row1_pkg = self.ROW1[self.COL_PKG]
    orig_row = dict(self.ROW1)
    orig_row[self.COL_VER] = '1.2.3' # Give it something to change.
    ss_row = SSRow(orig_row)
    mocked_uploader._GetSSRowForPackage(row1_pkg).AndReturn(ss_row)
    # We expect ROW1 to be changed.
    mocked_gd_client.UpdateRow(mox.IgnoreArg(), mox.Func(row1_verifier))
    # Check for output indicating a changed line (starts with C).
    mocked_uploader._Verbose(mox.Regex(r'^C '))

    self.mox.ReplayAll()

    # Verify
    self._StartCapturingOutput()
    ups.Uploader._UploadChangedRows(mocked_uploader)
    self.mox.VerifyAll()
    self._StopCapturingOutput()

  def testDeleteOldRows(self):
    mocked_gd_client = self._MockGdClient()
    mocked_uploader = self._MockUploader(gd_client=mocked_gd_client)
    mocked_uploader._ss_key = ss_key = 'Some ss_key'
    mocked_uploader._ws_key = ws_key = 'Some ws_key'

    def SSRowVerifier(ss_row, golden_row):
      for col in golden_row:
        ss_col = ups._PrepColForSS(col)
        golden_ss_val = ups._PrepValForSS(golden_row[col])
        ss_val = ss_row.custom[ss_col].text
        if golden_ss_val != ss_val:
          return False
      return True
    row2_verifier = lambda ss_row : SSRowVerifier(ss_row, self.ROW2)

    # Prepare spreadsheet feed with 2 rows, one in table and one not.
    feed = SSFeed([self.ROW1, self.ROW2], cols=self.COLS)
    mocked_gd_client.GetListFeed(ss_key, ws_key).AndReturn(feed)
    mocked_uploader._Verbose(mox.Regex(r'^D '))
    # We expect ROW2 in spreadsheet to be deleted.
    mocked_gd_client.DeleteRow(mox.Func(row2_verifier))
    self.mox.ReplayAll()

    # Verify
    self._StartCapturingOutput()
    ups.Uploader._DeleteOldRows(mocked_uploader)
    self.mox.VerifyAll()
    self._StopCapturingOutput()

class MainTest(test_lib.MoxTestCase):
  """Test argument handling at the main method level."""

  def setUp(self):
    """Setup for all tests in this class."""
    mox.MoxTestBase.setUp(self)

  def _AssertOutputEndsInError(self, stdout):
    """Return True if |stdout| ends with an error message."""
    lastline = [ln for ln in stdout.split('\n') if ln][-1]
    self.assertTrue(self._IsErrorLine(lastline),
                    msg="expected output to end in error line, but "
                    "_IsErrorLine says this line is not an error:\n%s" %
                    lastline)

  def _PrepareArgv(self, *args):
    """Prepare command line for calling upload_package_status.main"""
    sys.argv = [ re.sub("_unittest", "", sys.argv[0]) ]
    sys.argv.extend(args)

  def testHelp(self):
    """Test that --help is functioning"""
    self._PrepareArgv("--help")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Running with --help should exit with code==0
    try:
      ups.main()
    except exceptions.SystemExit, e:
      self.assertEquals(e.args[0], 0)

    # Verify that a message beginning with "Usage: " was printed
    (stdout, _stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self.assertTrue(stdout.startswith("Usage: "))

  def testMissingCSV(self):
    """Test that running without a csv file argument exits with an error."""
    self._PrepareArgv("")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Running without a package should exit with code!=0
    try:
      ups.main()
    except exceptions.SystemExit, e:
      self.assertNotEquals(e.args[0], 0)

    # Verify that output ends in an error message.
    (stdout, _stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self._AssertOutputEndsInError(stdout)

  def testMainEmailPassword(self):
    """Verify that running main with email/password follows flow."""
    csv = 'any.csv'
    email = 'foo@g.com'
    password = '123'

    self.mox.StubOutWithMock(ups, 'LoadTable')
    self.mox.StubOutWithMock(ups.Uploader, 'SetCreds')
    self.mox.StubOutWithMock(ups.Uploader, 'LoginDocsWithEmailPassword')
    self.mox.StubOutWithMock(ups.Uploader, 'Upload')
    self.mox.StubOutWithMock(mps, 'FinalizeTable')

    ups.LoadTable(csv).AndReturn('csv_table')
    mps.FinalizeTable('csv_table')
    ups.Uploader.SetCreds(email=email, password=password)
    ups.Uploader.LoginDocsWithEmailPassword()
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name='Packages')
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name='Dependencies')
    self.mox.ReplayAll()

    self._PrepareArgv("--email=%s" % email,
                      "--password=%s" % password,
                      csv)

    ups.main()
    self.mox.VerifyAll()

  @test_lib.tempfile_decorator
  def testMainCredsFile(self):
    """Verify that running main with creds file follows flow."""
    csv = 'any.csv'
    email = 'foo@g.com'
    password = '123'
    creds_file = self.tempfile
    _WriteCredsFile(creds_file, email, password)

    self.mox.StubOutWithMock(ups, 'LoadTable')
    self.mox.StubOutWithMock(ups.Uploader, 'LoadCreds')
    self.mox.StubOutWithMock(ups.Uploader, 'LoginDocsWithEmailPassword')
    self.mox.StubOutWithMock(ups.Uploader, 'Upload')
    self.mox.StubOutWithMock(mps, 'FinalizeTable')

    ups.LoadTable(csv).AndReturn('csv_table')
    mps.FinalizeTable('csv_table')
    ups.Uploader.LoadCreds(creds_file)
    ups.Uploader.LoginDocsWithEmailPassword()
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name=ups.PKGS_WS_NAME)
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name=ups.DEPS_WS_NAME)
    self.mox.ReplayAll()

    self._PrepareArgv("--cred-file=%s" % creds_file, csv)

    ups.main()
    self.mox.VerifyAll()

if __name__ == '__main__':
  unittest.main()
