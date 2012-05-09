#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the gdata_lib module."""

import getpass
import re
import unittest

import atom.service
import gdata.projecthosting.client as gd_ph_client
import gdata.spreadsheet.service
import mox

import cros_test_lib as test_lib
import gdata_lib
import osutils

# pylint: disable=W0201,W0212,E1101,R0904


class GdataLibTest(test_lib.TestCase):

  def testPrepColNameForSS(self):
    tests = {
      'foo': 'foo',
      'Foo': 'foo',
      'FOO': 'foo',
      'foo bar': 'foobar',
      'Foo Bar': 'foobar',
      'F O O B A R': 'foobar',
      'Foo/Bar': 'foobar',
      'Foo Bar/Dude': 'foobardude',
      'foo/bar': 'foobar',
      }

    for col in tests:
      expected = tests[col]
      self.assertEquals(expected, gdata_lib.PrepColNameForSS(col))
      self.assertEquals(expected, gdata_lib.PrepColNameForSS(expected))

  def testPrepValForSS(self):
    tests = {
      None: None,
      '': '',
      'foo': 'foo',
      'foo123': 'foo123',
      '123': "'123",
      '1.2': "'1.2",
      }

    for val in tests:
      expected = tests[val]
      self.assertEquals(expected, gdata_lib.PrepValForSS(val))

  def testPrepRowForSS(self):
    vals = {
      None: None,
      '': '',
      'foo': 'foo',
      'foo123': 'foo123',
      '123': "'123",
      '1.2': "'1.2",
      }

    # Create before and after rows (rowIn, rowOut).
    rowIn = {}
    rowOut = {}
    col = 'a' # Column names not important.
    for valIn in vals:
      valOut = vals[valIn]
      rowIn[col] = valIn
      rowOut[col] = valOut

      col = chr(ord(col) + 1) # Change column name.

    self.assertEquals(rowOut, gdata_lib.PrepRowForSS(rowIn))

  def testScrubValFromSS(self):
    tests = {
      None: None,
      'foo': 'foo',
      '123': '123',
      "'123": '123',
      }

    for val in tests:
      expected = tests[val]
      self.assertEquals(expected, gdata_lib.ScrubValFromSS(val))


class CredsTest(test_lib.MoxTestCase):

  USER = 'somedude@chromium.org'
  PASSWORD = 'worldsbestpassword'
  DOCS_TOKEN = 'SomeDocsAuthToken'
  TRACKER_TOKEN = 'SomeTrackerAuthToken'

  @osutils.TempFileDecorator
  def testStoreLoadCreds(self):
    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.ReplayAll()

    # This is the test verification.
    with self.OutputCapturer():
      gdata_lib.Creds.SetCreds(mocked_creds, self.USER, self.PASSWORD)
      self.assertEquals(self.USER, mocked_creds.user)
      self.assertEquals(self.PASSWORD, mocked_creds.password)
      self.assertTrue(mocked_creds.creds_dirty)

      gdata_lib.Creds.StoreCreds(mocked_creds, self.tempfile)
      self.assertEquals(self.USER, mocked_creds.user)
      self.assertEquals(self.PASSWORD, mocked_creds.password)
      self.assertFalse(mocked_creds.creds_dirty)

      # Clear user/password before loading from just-created file.
      mocked_creds.user = None
      mocked_creds.password = None
      self.assertEquals(None, mocked_creds.user)
      self.assertEquals(None, mocked_creds.password)

      gdata_lib.Creds.LoadCreds(mocked_creds, self.tempfile)
      self.assertEquals(self.USER, mocked_creds.user)
      self.assertEquals(self.PASSWORD, mocked_creds.password)
      self.assertFalse(mocked_creds.creds_dirty)

    self.mox.VerifyAll()

  @osutils.TempFileDecorator
  def testStoreLoadToken(self):
    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    mocked_creds.user = self.USER
    self.mox.ReplayAll()

    # This is the test verification.
    with self.OutputCapturer():
      gdata_lib.Creds.SetDocsAuthToken(mocked_creds, self.DOCS_TOKEN)
      self.assertEquals(self.DOCS_TOKEN, mocked_creds.docs_auth_token)
      self.assertTrue(mocked_creds.token_dirty)
      gdata_lib.Creds.SetTrackerAuthToken(mocked_creds, self.TRACKER_TOKEN)
      self.assertEquals(self.TRACKER_TOKEN, mocked_creds.tracker_auth_token)
      self.assertTrue(mocked_creds.token_dirty)

      gdata_lib.Creds.StoreAuthToken(mocked_creds, self.tempfile)
      self.assertEquals(self.DOCS_TOKEN, mocked_creds.docs_auth_token)
      self.assertEquals(self.TRACKER_TOKEN, mocked_creds.tracker_auth_token)
      self.assertFalse(mocked_creds.token_dirty)

      # Clear auth_tokens before loading from just-created file.
      mocked_creds.docs_auth_token = None
      mocked_creds.tracker_auth_token = None
      mocked_creds.user = None

      gdata_lib.Creds.LoadAuthToken(mocked_creds, self.tempfile)
      self.assertEquals(self.DOCS_TOKEN, mocked_creds.docs_auth_token)
      self.assertEquals(self.TRACKER_TOKEN, mocked_creds.tracker_auth_token)
      self.assertFalse(mocked_creds.token_dirty)
      self.assertEquals(self.USER, mocked_creds.user)

    self.mox.VerifyAll()

  def testSetCreds(self):
    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.Creds.SetCreds(mocked_creds, self.USER,
                             password=self.PASSWORD)
    self.mox.VerifyAll()
    self.assertEquals(self.USER, mocked_creds.user)
    self.assertEquals(self.PASSWORD, mocked_creds.password)
    self.assertTrue(mocked_creds.creds_dirty)

  def testSetCredsNoPassword(self):
    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(getpass, 'getpass')

    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    getpass.getpass(mox.IgnoreArg()).AndReturn(self.PASSWORD)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.Creds.SetCreds(mocked_creds, self.USER)
    self.mox.VerifyAll()
    self.assertEquals(self.USER, mocked_creds.user)
    self.assertEquals(self.PASSWORD, mocked_creds.password)
    self.assertTrue(mocked_creds.creds_dirty)

  def testSetDocsToken(self):
    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.Creds.SetDocsAuthToken(mocked_creds, self.DOCS_TOKEN)
    self.mox.VerifyAll()
    self.assertEquals(self.DOCS_TOKEN, mocked_creds.docs_auth_token)
    self.assertTrue(mocked_creds.token_dirty)

  def testSetTrackerToken(self):
    # This is the replay script for the test.
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.Creds.SetTrackerAuthToken(mocked_creds, self.TRACKER_TOKEN)
    self.mox.VerifyAll()
    self.assertEquals(self.TRACKER_TOKEN, mocked_creds.tracker_auth_token)
    self.assertTrue(mocked_creds.token_dirty)

class SpreadsheetRowTest(test_lib.TestCase):

  SS_ROW_OBJ = 'SSRowObj'
  SS_ROW_NUM = 5

  def testEmpty(self):
    row = gdata_lib.SpreadsheetRow(self.SS_ROW_OBJ, self.SS_ROW_NUM)

    self.assertEquals(0, len(row))
    self.assertEquals(self.SS_ROW_OBJ, row.ss_row_obj)
    self.assertEquals(self.SS_ROW_NUM, row.ss_row_num)

    self.assertRaises(TypeError, row, '__setitem__', 'abc', 'xyz')
    self.assertEquals(0, len(row))
    self.assertFalse('abc' in row)

  def testInit(self):
    starting_vals = {'abc': 'xyz', 'foo': 'bar'}
    row = gdata_lib.SpreadsheetRow(self.SS_ROW_OBJ, self.SS_ROW_NUM,
                                   starting_vals)

    self.assertEquals(len(starting_vals), len(row))
    self.assertEquals(starting_vals, row)
    self.assertEquals(row['abc'], 'xyz')
    self.assertTrue('abc' in row)
    self.assertEquals(row['foo'], 'bar')
    self.assertTrue('foo' in row)

    self.assertEquals(self.SS_ROW_OBJ, row.ss_row_obj)
    self.assertEquals(self.SS_ROW_NUM, row.ss_row_num)

    self.assertRaises(TypeError, row, '__delitem__', 'abc')
    self.assertEquals(len(starting_vals), len(row))
    self.assertTrue('abc' in row)


class SpreadsheetCommTest(test_lib.MoxTestCase):

  SS_KEY = 'TheSSKey'
  WS_NAME = 'TheWSName'
  WS_KEY = 'TheWSKey'

  USER = 'dude'
  PASSWORD = 'shhh'
  TOKEN = 'authtoken'

  COLUMNS = ('greeting', 'name', 'title')
  ROWS = (
      { 'greeting': 'Hi', 'name': 'George', 'title': 'Mr.' },
      { 'greeting': 'Howdy', 'name': 'Billy Bob', 'title': 'Mr.' },
      { 'greeting': 'Yo', 'name': 'Adriane', 'title': 'Ms.' },
      )

  def MockScomm(self, connect=True):
    """Return a mocked SpreadsheetComm"""
    mocked_scomm = self.mox.CreateMock(gdata_lib.SpreadsheetComm)

    mocked_scomm._columns = None
    mocked_scomm._rows = None

    if connect:
      mocked_gdclient = self.mox.CreateMock(gdata_lib.RetrySpreadsheetsService)
      mocked_scomm.gd_client = mocked_gdclient
      mocked_scomm.ss_key = self.SS_KEY
      mocked_scomm.ws_name = self.WS_NAME
      mocked_scomm.ws_key = self.WS_KEY
    else:
      mocked_scomm.gd_client = None
      mocked_scomm.ss_key = None
      mocked_scomm.ws_name = None
      mocked_scomm.ws_key = None

    return mocked_scomm

  def NewScomm(self, gd_client=None, connect=True):
    """Return a non-mocked SpreadsheetComm."""
    scomm = gdata_lib.SpreadsheetComm()
    scomm.gd_client = gd_client

    if connect:
      scomm.ss_key = self.SS_KEY
      scomm.ws_name = self.WS_NAME
      scomm.ws_key = self.WS_KEY
    else:
      scomm.ss_key = None
      scomm.ws_name = None
      scomm.ws_key = None

    return scomm

  def GenerateCreds(self, skip_user=False, skip_token=False):
    creds = gdata_lib.Creds()
    if not skip_user:
      creds.user = self.USER
      creds.password = self.PASSWORD

    if not skip_token:
      creds.docs_auth_token = self.TOKEN

    return creds

  def testConnect(self):
    mocked_scomm = self.MockScomm(connect=False)
    creds = self.GenerateCreds()

    # This is the replay script for the test.
    mocked_scomm._Login(creds, 'chromiumos')
    mocked_scomm.SetCurrentWorksheet(self.WS_NAME, ss_key=self.SS_KEY)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.SpreadsheetComm.Connect(mocked_scomm, creds,
                                      self.SS_KEY, self.WS_NAME)
    self.mox.VerifyAll()

  def testColumns(self):
    """Test the .columns property.  Testing a property gets ugly."""
    self.mox.StubOutWithMock(gdata.spreadsheet.service, 'CellQuery')
    mocked_gdclient = self.mox.CreateMock(gdata_lib.RetrySpreadsheetsService)
    scomm = self.NewScomm(gd_client=mocked_gdclient, connect=True)

    query = {'max-row': '1'}

    # Simulate a Cells feed from spreadsheet for the column row.
    cols = [c[0].upper() + c[1:] for c in self.COLUMNS]
    entry = [test_lib.EasyAttr(content=test_lib.EasyAttr(text=c)) for c in cols]
    feed = test_lib.EasyAttr(entry=entry)

    # This is the replay script for the test.
    gdata.spreadsheet.service.CellQuery().AndReturn(query)
    mocked_gdclient.GetCellsFeed(self.SS_KEY, self.WS_KEY, query=query
                                 ).AndReturn(feed)
    self.mox.ReplayAll()

    # This is the test verification.
    result = scomm.columns
    del scomm # Force deletion now before VerifyAll.
    self.mox.VerifyAll()

    expected_result = self.COLUMNS
    self.assertEquals(expected_result, result)

  def testRows(self):
    """Test the .rows property.  Testing a property gets ugly."""
    mocked_gdclient = self.mox.CreateMock(gdata_lib.RetrySpreadsheetsService)
    scomm = self.NewScomm(gd_client=mocked_gdclient, connect=True)

    # Simulate a List feed from spreadsheet for all rows.
    rows = [{'col_name': 'Joe', 'col_age': '12', 'col_zip': '12345'},
            {'col_name': 'Bob', 'col_age': '15', 'col_zip': '54321'},
            ]
    entry = []
    for row in rows:
      custom = dict((k, test_lib.EasyAttr(text=v)) for (k, v) in row.items())
      entry.append(test_lib.EasyAttr(custom=custom))
    feed = test_lib.EasyAttr(entry=entry)

    # This is the replay script for the test.
    mocked_gdclient.GetListFeed(self.SS_KEY, self.WS_KEY).AndReturn(feed)
    self.mox.ReplayAll()

    # This is the test verification.
    result = scomm.rows
    del scomm # Force deletion now before VerifyAll.
    self.mox.VerifyAll()
    self.assertEquals(tuple(rows), result)

    # Result tuple should have spreadsheet row num as attribute on each row.
    self.assertEquals(2, result[0].ss_row_num)
    self.assertEquals(3, result[1].ss_row_num)

    # Result tuple should have spreadsheet row obj as attribute on each row.
    self.assertEquals(entry[0], result[0].ss_row_obj)
    self.assertEquals(entry[1], result[1].ss_row_obj)

  def testSetCurrentWorksheetStart(self):
    mocked_scomm = self.MockScomm(connect=True)

    # Undo worksheet settings.
    mocked_scomm.ss_key = None
    mocked_scomm.ws_name = None
    mocked_scomm.ws_key = None

    # This is the replay script for the test.
    mocked_scomm._ClearCache()
    mocked_scomm._GetWorksheetKey(self.SS_KEY, self.WS_NAME
                                  ).AndReturn(self.WS_KEY)
    mocked_scomm._ClearCache()
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.SpreadsheetComm.SetCurrentWorksheet(mocked_scomm, self.WS_NAME,
                                                  ss_key=self.SS_KEY)
    self.mox.VerifyAll()

    self.assertEquals(self.SS_KEY, mocked_scomm.ss_key)
    self.assertEquals(self.WS_KEY, mocked_scomm.ws_key)
    self.assertEquals(self.WS_NAME, mocked_scomm.ws_name)

  def testSetCurrentWorksheetRestart(self):
    mocked_scomm = self.MockScomm(connect=True)

    other_ws_name = 'OtherWSName'
    other_ws_key = 'OtherWSKey'

    # This is the replay script for the test.
    mocked_scomm._GetWorksheetKey(self.SS_KEY, other_ws_name
                                  ).AndReturn(other_ws_key)
    mocked_scomm._ClearCache()
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.SpreadsheetComm.SetCurrentWorksheet(mocked_scomm, other_ws_name)
    self.mox.VerifyAll()

    self.assertEquals(self.SS_KEY, mocked_scomm.ss_key)
    self.assertEquals(other_ws_key, mocked_scomm.ws_key)
    self.assertEquals(other_ws_name, mocked_scomm.ws_name)

  def testClearCache(self):
    rows = 'SomeRows'
    cols = 'SomeColumns'

    scomm = self.NewScomm()
    scomm._rows = rows
    scomm._columns = cols

    scomm._ClearCache(keep_columns=True)
    self.assertTrue(scomm._rows is None)
    self.assertEquals(cols, scomm._columns)

    scomm._rows = rows
    scomm._columns = cols

    scomm._ClearCache(keep_columns=False)
    self.assertTrue(scomm._rows is None)
    self.assertTrue(scomm._columns is None)

    scomm._rows = rows
    scomm._columns = cols

    scomm._ClearCache()
    self.assertTrue(scomm._rows is None)
    self.assertTrue(scomm._columns is None)

  def testLoginWithUserPassword(self):
    mocked_scomm = self.MockScomm(connect=False)
    creds = self.GenerateCreds(skip_token=True)

    self.mox.StubOutClassWithMocks(gdata_lib, 'RetrySpreadsheetsService')

    source = 'SomeSource'

    # This is the replay script for the test.
    mocked_gdclient = gdata_lib.RetrySpreadsheetsService()
    mocked_gdclient.ProgrammaticLogin()
    mocked_gdclient.GetClientLoginToken().AndReturn(self.TOKEN)
    self.mox.ReplayAll()

    # This is the test verification.
    with self.OutputCapturer():
      gdata_lib.SpreadsheetComm._Login(mocked_scomm, creds, source)
    self.mox.VerifyAll()
    self.assertEquals(self.USER, mocked_gdclient.email)
    self.assertEquals(self.PASSWORD, mocked_gdclient.password)
    self.assertEquals(self.TOKEN, creds.docs_auth_token)
    self.assertEquals(source, mocked_gdclient.source)
    self.assertEquals(mocked_gdclient, mocked_scomm.gd_client)

  def testLoginWithToken(self):
    mocked_scomm = self.MockScomm(connect=False)
    creds = self.GenerateCreds(skip_user=True)

    self.mox.StubOutClassWithMocks(gdata_lib, 'RetrySpreadsheetsService')

    source = 'SomeSource'

    # This is the replay script for the test.
    mocked_gdclient = gdata_lib.RetrySpreadsheetsService()
    mocked_gdclient.SetClientLoginToken(creds.docs_auth_token)
    self.mox.ReplayAll()

    # This is the test verification.
    with self.OutputCapturer():
      gdata_lib.SpreadsheetComm._Login(mocked_scomm, creds, source)
    self.mox.VerifyAll()
    self.assertFalse(hasattr(mocked_gdclient, 'email'))
    self.assertFalse(hasattr(mocked_gdclient, 'password'))
    self.assertEquals(source, mocked_gdclient.source)
    self.assertEquals(mocked_gdclient, mocked_scomm.gd_client)

  def testGetWorksheetKey(self):
    mocked_scomm = self.MockScomm()

    entrylist = [
      test_lib.EasyAttr(title=test_lib.EasyAttr(text='Foo'), id='NotImportant'),
      test_lib.EasyAttr(title=test_lib.EasyAttr(text=self.WS_NAME),
               id=test_lib.EasyAttr(text='/some/path/%s' % self.WS_KEY)),
      test_lib.EasyAttr(title=test_lib.EasyAttr(text='Bar'), id='NotImportant'),
      ]
    feed = test_lib.EasyAttr(entry=entrylist)

    # This is the replay script for the test.
    mocked_scomm.gd_client.GetWorksheetsFeed(self.SS_KEY).AndReturn(feed)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.SpreadsheetComm._GetWorksheetKey(mocked_scomm,
                                               self.SS_KEY, self.WS_NAME)
    self.mox.VerifyAll()

  def testGetColumns(self):
    mocked_scomm = self.MockScomm()
    mocked_scomm.columns = 'SomeColumns'

    # Replay script
    self.mox.ReplayAll()

    # This is the test verification.
    result = gdata_lib.SpreadsheetComm.GetColumns(mocked_scomm)
    self.mox.VerifyAll()
    self.assertEquals('SomeColumns', result)

  def testGetColumnIndex(self):
    # Note that spreadsheet column indices start at 1.
    mocked_scomm = self.MockScomm()
    mocked_scomm.columns = ['these', 'are', 'column', 'names']

    # This is the replay script for the test.
    self.mox.ReplayAll()

    # This is the test verification.
    result = gdata_lib.SpreadsheetComm.GetColumnIndex(mocked_scomm, 'are')
    self.mox.VerifyAll()
    self.assertEquals(2, result)

  def testGetRows(self):
    mocked_scomm = self.MockScomm()
    rows = []
    for row_ix, row_dict in enumerate(self.ROWS):
      rows.append(gdata_lib.SpreadsheetRow('SSRowObj%d' % (row_ix + 2),
                                           (row_ix + 2), row_dict))
    mocked_scomm.rows = tuple(rows)

    # This is the replay script for the test.
    self.mox.ReplayAll()

    # This is the test verification.
    result = gdata_lib.SpreadsheetComm.GetRows(mocked_scomm)
    self.mox.VerifyAll()
    self.assertEquals(self.ROWS, result)
    for row_ix in xrange(len(self.ROWS)):
      self.assertEquals(row_ix + 2, result[row_ix].ss_row_num)
      self.assertEquals('SSRowObj%d' % (row_ix + 2), result[row_ix].ss_row_obj)

  def testGetRowCacheByCol(self):
    mocked_scomm = self.MockScomm()

    # This is the replay script for the test.
    mocked_scomm.GetRows().AndReturn(self.ROWS)
    self.mox.ReplayAll()

    # This is the test verification.
    result = gdata_lib.SpreadsheetComm.GetRowCacheByCol(mocked_scomm, 'name')
    self.mox.VerifyAll()

    # Result is a dict of rows by the 'name' column.
    for row in self.ROWS:
      name = row['name']
      self.assertEquals(row, result[name])

  def testGetRowCacheByColDuplicates(self):
    mocked_scomm = self.MockScomm()

    # Create new row list with duplicates by name column.
    rows = []
    for row in self.ROWS:
      new_row = dict(row)
      new_row['greeting'] = row['greeting'] + ' there'
      rows.append(new_row)

    rows.extend(self.ROWS)

    # This is the replay script for the test.
    mocked_scomm.GetRows().AndReturn(tuple(rows))
    self.mox.ReplayAll()

    # This is the test verification.
    result = gdata_lib.SpreadsheetComm.GetRowCacheByCol(mocked_scomm, 'name')
    self.mox.VerifyAll()

    # Result is a dict of rows by the 'name' column.  In this
    # test each result should be a list of the rows with the same
    # value in the 'name' column.
    num_rows = len(rows)
    for ix in xrange(num_rows / 2):
      row1 = rows[ix]
      row2 = rows[ix + (num_rows / 2)]

      name = row1['name']
      self.assertEquals(name, row2['name'])

      expected_rows = [row1, row2]
      self.assertEquals(expected_rows, result[name])

  def testInsertRow(self):
    mocked_scomm = self.MockScomm()

    row = 'TheRow'

    # Replay script
    mocked_scomm.gd_client.InsertRow(row, mocked_scomm.ss_key,
                                     mocked_scomm.ws_key)
    mocked_scomm._ClearCache(keep_columns=True)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.SpreadsheetComm.InsertRow(mocked_scomm, row)
    self.mox.VerifyAll()

  def testUpdateRowCellByCell(self):
    mocked_scomm = self.MockScomm()

    rowIx = 5
    row = {'a': 123, 'b': 234, 'c': 345}
    colIndices = {'a': 1, 'b': None, 'c': 4}

    # Replay script
    for colName in row:
      colIx = colIndices[colName]
      mocked_scomm.GetColumnIndex(colName).AndReturn(colIx)
      if colIx is not None:
        mocked_scomm.ReplaceCellValue(rowIx, colIx, row[colName])
    mocked_scomm._ClearCache(keep_columns=True)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.SpreadsheetComm.UpdateRowCellByCell(mocked_scomm, rowIx, row)
    self.mox.VerifyAll()

  def testDeleteRow(self):
    mocked_scomm = self.MockScomm()

    ss_row = 'TheRow'

    # Replay script
    mocked_scomm.gd_client.DeleteRow(ss_row)
    mocked_scomm._ClearCache(keep_columns=True)
    self.mox.ReplayAll()

    # This is the test verification.
    gdata_lib.SpreadsheetComm.DeleteRow(mocked_scomm, ss_row)
    self.mox.VerifyAll()

  def testReplaceCellValue(self):
    mocked_scomm = self.MockScomm()

    rowIx = 14
    colIx = 4
    val = 'TheValue'

    # Replay script
    mocked_scomm.gd_client.UpdateCell(rowIx, colIx, val,
                                      mocked_scomm.ss_key, mocked_scomm.ws_key)
    mocked_scomm._ClearCache(keep_columns=True)
    self.mox.ReplayAll()

    # Verify
    gdata_lib.SpreadsheetComm.ReplaceCellValue(mocked_scomm, rowIx, colIx, val)
    self.mox.VerifyAll()

  def testClearCellValue(self):
    mocked_scomm = self.MockScomm()

    rowIx = 14
    colIx = 4

    # Replay script
    mocked_scomm.ReplaceCellValue(rowIx, colIx, None)
    self.mox.ReplayAll()

    # Verify
    gdata_lib.SpreadsheetComm.ClearCellValue(mocked_scomm, rowIx, colIx)
    self.mox.VerifyAll()


class IssueCommentTest(unittest.TestCase):

  def testInit(self):
    title = 'Greetings, Earthlings'
    text = 'I come in peace.'
    ic = gdata_lib.IssueComment(title, text)

    self.assertEquals(title, ic.title)
    self.assertEquals(text, ic.text)


class IssueTest(mox.MoxTestBase):

  def testInitOverride(self):
    owner = 'somedude@chromium.org'
    status = 'Assigned'
    issue = gdata_lib.Issue(owner=owner, status=status)

    self.assertEquals(owner, issue.owner)
    self.assertEquals(status, issue.status)

  def testInitInvalidOverride(self):
    self.assertRaises(ValueError, gdata_lib.Issue,
                      foobar='NotARealAttr')

  def testInitFromTracker(self):
    # Need to create a dummy Tracker Issue object.
    tissue_id = 123
    tissue_labels = ['Iteration-10', 'Effort-2']
    tissue_owner = 'thedude@chromium.org'
    tissue_status = 'Available'
    tissue_content = 'The summary message'
    tissue_title = 'The Big Title'

    tissue = test_lib.EasyAttr()
    tissue.id = test_lib.EasyAttr(text='http://www/some/path/%d' % tissue_id)
    tissue.label = [test_lib.EasyAttr(text=l) for l in tissue_labels]
    tissue.owner = test_lib.EasyAttr(
      username=test_lib.EasyAttr(text=tissue_owner))
    tissue.status = test_lib.EasyAttr(text=tissue_status)
    tissue.content = test_lib.EasyAttr(text=tissue_content)
    tissue.title = test_lib.EasyAttr(text=tissue_title)

    mocked_issue = self.mox.CreateMock(gdata_lib.Issue)

    # Replay script
    mocked_issue.GetTrackerIssueComments(tissue_id, 'TheProject').AndReturn([])
    self.mox.ReplayAll()

    # Verify
    gdata_lib.Issue.InitFromTracker(mocked_issue, tissue, 'TheProject')
    self.mox.VerifyAll()
    self.assertEquals(tissue_id, mocked_issue.id)
    self.assertEquals(tissue_labels, mocked_issue.labels)
    self.assertEquals(tissue_owner, mocked_issue.owner)
    self.assertEquals(tissue_status, mocked_issue.status)
    self.assertEquals(tissue_content, mocked_issue.summary)
    self.assertEquals(tissue_title, mocked_issue.title)
    self.assertEquals([], mocked_issue.comments)


class TrackerCommTest(test_lib.MoxTestCase):

  def testConnectEmail(self):
    source = 'TheSource'
    token = 'TheToken'
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    mocked_creds.user = 'dude'
    mocked_creds.password = 'shhh'
    mocked_creds.tracker_auth_token = None
    self.mox.StubOutClassWithMocks(gd_ph_client, 'ProjectHostingClient')
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)

    def set_token(*_args, **_kwargs):
      mocked_itclient.auth_token = test_lib.EasyAttr(token_string=token)

    # Replay script
    mocked_itclient = gd_ph_client.ProjectHostingClient()
    mocked_itclient.ClientLogin(mocked_creds.user, mocked_creds.password,
                                source=source, service='code',
                                account_type='GOOGLE'
                                ).WithSideEffects(set_token)
    mocked_creds.SetTrackerAuthToken(token)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      gdata_lib.TrackerComm.Connect(mocked_tcomm, mocked_creds, 'TheProject',
                                    source=source)
    self.mox.VerifyAll()
    self.assertEquals(mocked_tcomm.it_client, mocked_itclient)

  def testConnectToken(self):
    source = 'TheSource'
    token = 'TheToken'
    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    mocked_creds.user = 'dude'
    mocked_creds.password = 'shhh'
    mocked_creds.tracker_auth_token = token
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)

    self.mox.StubOutClassWithMocks(gd_ph_client, 'ProjectHostingClient')
    self.mox.StubOutClassWithMocks(gdata.gauth, 'ClientLoginToken')

    # Replay script
    mocked_itclient = gd_ph_client.ProjectHostingClient()
    mocked_token = gdata.gauth.ClientLoginToken(token)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      gdata_lib.TrackerComm.Connect(mocked_tcomm, mocked_creds, 'TheProject',
                                    source=source)
    self.mox.VerifyAll()
    self.assertEquals(mocked_tcomm.it_client, mocked_itclient)
    self.assertEquals(mocked_itclient.auth_token, mocked_token)

  def testGetTrackerIssueById(self):
    mocked_itclient = self.mox.CreateMock(gd_ph_client.ProjectHostingClient)
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)
    mocked_tcomm.it_client = mocked_itclient
    mocked_tcomm.project_name = 'TheProject'

    self.mox.StubOutClassWithMocks(gd_ph_client, 'Query')
    self.mox.StubOutClassWithMocks(gdata_lib, 'Issue')
    self.mox.StubOutWithMock(gdata_lib.Issue, 'InitFromTracker')

    issue_id = 12345
    feed = test_lib.EasyAttr(entry=['hi', 'there'])

    # Replay script
    mocked_query = gd_ph_client.Query(issue_id=str(issue_id))
    mocked_itclient.get_issues('TheProject', query=mocked_query
                               ).AndReturn(feed)
    mocked_issue = gdata_lib.Issue()
    mocked_issue.InitFromTracker(feed.entry[0], 'TheProject')
    self.mox.ReplayAll()

    # Verify
    gdata_lib.TrackerComm.GetTrackerIssueById(mocked_tcomm, issue_id)
    self.mox.VerifyAll()

  def testCreateTrackerIssue(self):
    author = 'TheAuthor'
    mocked_itclient = self.mox.CreateMock(gd_ph_client.ProjectHostingClient)
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)
    mocked_tcomm.author = author
    mocked_tcomm.it_client = mocked_itclient
    mocked_tcomm.project_name = 'TheProject'

    issue = test_lib.EasyAttr(title='TheTitle',
                     summary='TheSummary',
                     status='TheStatus',
                     owner='TheOwner',
                     labels='TheLabels')

    # Replay script
    issue_id = test_lib.EasyAttr(id=test_lib.EasyAttr(text='foo/bar/123'))
    mocked_itclient.add_issue(project_name='TheProject',
                              title=issue.title,
                              content=issue.summary,
                              author=author,
                              status=issue.status,
                              owner=issue.owner,
                              labels=issue.labels,
                              ).AndReturn(issue_id)
    self.mox.ReplayAll()

    # Verify
    result = gdata_lib.TrackerComm.CreateTrackerIssue(mocked_tcomm, issue)
    self.mox.VerifyAll()
    self.assertEquals(123, result)

  def testAppendTrackerIssueById(self):
    author = 'TheAuthor'
    project_name = 'TheProject'
    mocked_itclient = self.mox.CreateMock(gd_ph_client.ProjectHostingClient)
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)
    mocked_tcomm.author = author
    mocked_tcomm.it_client = mocked_itclient
    mocked_tcomm.project_name = project_name

    issue_id = 54321
    comment = 'TheComment'

    # Replay script
    mocked_itclient.update_issue(project_name=project_name,
                                 issue_id=issue_id,
                                 author=author,
                                 comment=comment)
    self.mox.ReplayAll()

    # Verify
    result = gdata_lib.TrackerComm.AppendTrackerIssueById(mocked_tcomm,
                                                          issue_id, comment)
    self.mox.VerifyAll()
    self.assertEquals(issue_id, result)


class RetrySpreadsheetsServiceTest(test_lib.MoxTestCase):

  def testRequest(self):
    """Test that calling request method invokes _RetryRequest wrapper."""
    # pylint: disable=W0212

    self.mox.StubOutWithMock(gdata_lib.RetrySpreadsheetsService,
                             '_RetryRequest')

    # Use a real RetrySpreadsheetsService object rather than a mocked
    # one, because the .request method only exists if __init__ is run.
    # Also split up __new__ and __init__ in order to grab the original
    # rss.request method (inherited from base class at that point).
    rss = gdata_lib.RetrySpreadsheetsService.__new__(
        gdata_lib.RetrySpreadsheetsService)
    orig_request = rss.request
    rss.__init__()

    args = ('GET', 'http://foo.bar')

    # This is the replay script for the test.
    gdata_lib.RetrySpreadsheetsService._RetryRequest(orig_request, *args
                                                     ).AndReturn('wrapped')
    self.mox.ReplayAll()

    # This is the test verification.
    retval = rss.request(*args)
    self.mox.VerifyAll()
    self.assertEquals('wrapped', retval)

  def _TestHttpClientRetryRequest(self, statuses):
    """Test retry logic in http_client request during ProgrammaticLogin.

    |statuses| is list of http codes to simulate, where 200 means success.
    """
    expect_success = statuses[-1] == 200

    self.mox.StubOutWithMock(atom.http.ProxiedHttpClient, 'request')
    rss = gdata_lib.RetrySpreadsheetsService()

    args = ('POST', 'https://www.google.com/accounts/ClientLogin')
    def _read():
      return 'Some response text'

    # This is the replay script for the test.
    # Simulate the return codes in statuses.
    for status in statuses:
      retstatus = test_lib.EasyAttr(status=status, read=_read)
      atom.http.ProxiedHttpClient.request(*args,
                                          data=mox.IgnoreArg(),
                                          headers=mox.IgnoreArg()
                                          ).AndReturn(retstatus)
    self.mox.ReplayAll()

    # This is the test verification.
    with self.OutputCapturer():
      if expect_success:
        rss.ProgrammaticLogin()
      else:
        self.assertRaises(gdata.service.Error, rss.ProgrammaticLogin)
      self.mox.VerifyAll()

    if not expect_success:
      # Retries did not help, request still failed.
      regexp = re.compile(r'^Giving up on HTTP request')
      self.AssertOutputContainsWarning(regexp=regexp)
    elif len(statuses) > 1:
      # Warning expected if retries were needed.
      self.AssertOutputContainsWarning()
    else:
      # First try worked, expect no warnings.
      self.AssertOutputContainsWarning(invert=True)

  def testHttpClientRetryRequest(self):
    self._TestHttpClientRetryRequest([200])

  def testHttpClientRetryRequest403(self):
    self._TestHttpClientRetryRequest([403, 200])

  def testHttpClientRetryRequest403x2(self):
    self._TestHttpClientRetryRequest([403, 403, 200])

  def testHttpClientRetryRequest403x3(self):
    self._TestHttpClientRetryRequest([403, 403, 403, 200])

  def testHttpClientRetryRequest403x4(self):
    self._TestHttpClientRetryRequest([403, 403, 403, 403, 200])

  def testHttpClientRetryRequest403x5(self):
    # This one should exhaust the retries.
    self._TestHttpClientRetryRequest([403, 403, 403, 403, 403])

  def _TestRetryRequest(self, statuses):
    """Test retry logic for request method.

    |statuses| is list of http codes to simulate, where 200 means success.
    """
    expect_success = statuses[-1] == 200
    expected_status_index = len(statuses) - 1 if expect_success else 0

    mocked_ss = self.mox.CreateMock(gdata_lib.RetrySpreadsheetsService)
    args = ('GET', 'http://foo.bar')

    # This is the replay script for the test.
    for ix, status in enumerate(statuses):
      # Add index of status to track which status the request function is
      # returning.  It is expected to return the last return status if
      # successful (retries or not), but first return status if failed.
      retval = test_lib.EasyAttr(status=status, index=ix)
      mocked_ss.request(*args).AndReturn(retval)

    self.mox.ReplayAll()

    # This is the test verification.
    with self.OutputCapturer():
      # pylint: disable=W0212
      rval = gdata_lib.RetrySpreadsheetsService._RetryRequest(mocked_ss,
                                                              mocked_ss.request,
                                                              *args)
      self.mox.VerifyAll()
      self.assertEquals(statuses[expected_status_index], rval.status)
      self.assertEquals(expected_status_index, rval.index)

    if not expect_success:
      # Retries did not help, request still failed.
      regexp = re.compile(r'^Giving up on HTTP request')
      self.AssertOutputContainsWarning(regexp=regexp)
    elif expected_status_index > 0:
      # Warning expected if retries were needed.
      self.AssertOutputContainsWarning()
    else:
      # First try worked, expect no warnings.
      self.AssertOutputContainsWarning(invert=True)

  def testRetryRequest(self):
    self._TestRetryRequest([200])

  def testRetryRequest403(self):
    self._TestRetryRequest([403, 200])

  def testRetryRequest403x2(self):
    self._TestRetryRequest([403, 403, 200])

  def testRetryRequest403x3(self):
    self._TestRetryRequest([403, 403, 403, 200])

  def testRetryRequest403x4(self):
    self._TestRetryRequest([403, 403, 403, 403, 200])

  def testRetryRequest403x5(self):
    # This one should exhaust the retries.
    self._TestRetryRequest([403, 403, 403, 403, 403])


if __name__ == '__main__':
  unittest.main()
