#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import simplejson
import os

import pyauto_functional
import pyauto


class SQLExecutionError(RuntimeError):
  """Represents an error that occurs while executing an SQL statement."""
  pass


class DatabasesTest(pyauto.PyUITest):
  """Test of Web SQL Databases."""

  def __init__(self, methodName='runTest'):
    super(DatabasesTest, self).__init__(methodName)
    # HTML page used for database testing.
    self.TEST_PAGE_URL = self.GetFileURLForDataPath(
        'database', 'database_tester.html')

  def _ParseAndCheckResultFromTestPage(self, json):
    """Helper function that parses the message sent from |TEST_PAGE_URL| and
    checks that it succeeded.

    Args:
      json: the message, encoded in JSON, that the test page sent to us

    Returns:
      dictionary representing the result from the test page, with format:
      {
        'succeeded': boolean
        'errorMsg': optional string
        'returnValue': optional any type
      }

    Raises:
      SQLExecutionError if the message contains an error message
    """
    result_dict = simplejson.loads(json)
    if result_dict['succeeded'] == False:
      raise SQLExecutionError(result_dict['errorMsg'])
    return result_dict

  def _CreateTable(self, tab_index=0, windex=0):
    """Creates a table in the database.

    This should only be called once per database. This should be called before
    attempting to insert, update, delete, or get the records in the database.

    Defaults to first tab in first window.

    Args:
      tab_index: index of the tab that will create the database
      windex: index of the window containing the tab that will create the
              database
    """
    json = self.CallJavascriptFunc('createTable', [], tab_index, windex)
    self._ParseAndCheckResultFromTestPage(json)

  def _InsertRecord(self, record, tab_index=0, windex=0):
    """Inserts a record, i.e., a row, into the database.

    Defaults to first tab in first window.

    Args:
      record: string that will be added as a new row in the database
      tab_index: index of the tab that will insert the record
      windex: index of the window containing the tab that will insert the record
    """
    json = self.CallJavascriptFunc('insertRecord', [record], tab_index, windex)
    self._ParseAndCheckResultFromTestPage(json)

  def _UpdateRecord(self, index, updated_record, tab_index=0, windex=0):
    """Updates a record, i.e., a row, in the database.

    Defaults to first tab in first window.

    Args:
      index: index of the record to update. Index 0 refers to the oldest item in
             the database
      updated_record: string that will be used to update the row in the database
      tab_index: index of the tab that will update the record
      windex: index of the window containing the tab that will update the record
    """
    json = self.CallJavascriptFunc(
        'updateRecord', [index, updated_record], tab_index, windex)
    self._ParseAndCheckResultFromTestPage(json)

  def _DeleteRecord(self, index, tab_index=0, windex=0):
    """Deletes a record, i.e., a row, from the database.

    Defaults to first tab in first window.

    Args:
      index: index of the record to be deleted. Index 0 refers to the oldest
             item in the database
      tab_index: index of the tab that will delete the record
      windex: index of the window containing the tab that will delete the record
    """
    json = self.CallJavascriptFunc('deleteRecord', [index], tab_index, windex)
    self._ParseAndCheckResultFromTestPage(json)

  def _GetRecords(self, tab_index=0, windex=0):
    """Returns all the records, i.e., rows, in the database.

    The records are ordererd from oldest to newest.

    Defaults to first tab in first window.

    Returns:
      array of all the records in the database

    Args:
      tab_index: index of the tab that will query the database
      windex: index of the window containing the tab that will query the
              database
    """
    json = self.CallJavascriptFunc('getRecords', [], tab_index, windex)
    return self._ParseAndCheckResultFromTestPage(json)['returnValue']

  def _HasTable(self, tab_index=0, windex=0):
    """Returns whether the page has a table in its database."""
    try:
      self._GetRecords(tab_index, windex)
    except SQLExecutionError:
      return False
    return True

  def testInsertRecord(self):
    """Insert records to the database."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('text')
    self.assertEquals(['text'], self._GetRecords())
    self._InsertRecord('text2')
    self.assertEquals(['text', 'text2'], self._GetRecords())

  def testUpdateRecord(self):
    """Update records in the database."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()

    # Update the first record.
    self._InsertRecord('text')
    self._UpdateRecord(0, '0')
    records = self._GetRecords()
    self.assertEquals(1, len(records))
    self.assertEquals('0', records[0])

    # Update the middle record.
    self._InsertRecord('1')
    self._InsertRecord('2')
    self._UpdateRecord(1, '1000')
    self.assertEquals(['0', '1000', '2'], self._GetRecords())

  def testDeleteRecord(self):
    """Delete records in the database."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()

    # Delete the first and only record.
    self._InsertRecord('text')
    self._DeleteRecord(0)
    self.assertFalse(self._GetRecords())

    # Delete the middle record.
    self._InsertRecord('0')
    self._InsertRecord('1')
    self._InsertRecord('2')
    self._DeleteRecord(1)
    self.assertEquals(['0', '2'], self._GetRecords())

  def testDeleteNonexistentRow(self):
    """Attempts to delete a nonexistent row in the table."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('text')
    did_throw_exception = False
    try:
      self._DeleteRecord(1)
    except SQLExecutionError:
      did_throw_exception = True
    self.assertTrue(did_throw_exception)
    self.assertEquals(['text'], self._GetRecords())

  def testDatabaseOperations(self):
    """Insert, update, and delete records in the database."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()

    for i in range(10):
      self._InsertRecord(str(i))
    records = self._GetRecords()
    self.assertEqual([str(i) for i in range(10)], records)

    for i in range(10):
      self._UpdateRecord(i, str(i * i))
    records = self._GetRecords()
    self.assertEqual([str(i * i) for i in range(10)], records)

    for i in range(10):
      self._DeleteRecord(0)
    self.assertEqual(0, len(self._GetRecords()))

  def testReloadActiveTab(self):
    """Create records in the database and verify they persist after reload."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('text')
    self.ReloadActiveTab()
    self.assertEquals(['text'], self._GetRecords())

  def testIncognitoCannotReadRegularDatabase(self):
    """Attempt to read a database created in a regular browser from an incognito
    browser.
    """
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('text')
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self.assertFalse(self._HasTable(windex=1))
    self._CreateTable(windex=1)
    self.assertFalse(self._GetRecords(windex=1))

  def testRegularCannotReadIncognitoDatabase(self):
    """Attempt to read a database created in an incognito browser from a regular
    browser.
    """
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self._CreateTable(windex=1)
    self._InsertRecord('text', windex=1)

    # Verify a regular browser cannot read the incognito database.
    self.NavigateToURL(self.TEST_PAGE_URL)
    self.assertFalse(self._HasTable())
    self._CreateTable()
    self.assertFalse(self._GetRecords())

  def testDbModificationPersistInSecondTab(self):
    """Verify DB changes within first tab are present in the second tab."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('text')
    self.AppendTab(pyauto.GURL(self.TEST_PAGE_URL))
    self._UpdateRecord(0, '0', tab_index=0)
    tab1_records = self._GetRecords(tab_index=0)
    tab2_records = self._GetRecords(tab_index=1)
    self.assertEquals(1, len(tab1_records))
    self.assertEquals('0', tab1_records[0])
    self.assertEquals(1, len(tab2_records))
    self.assertEquals(tab1_records[0], tab2_records[0])

  def testIncognitoDatabaseNotPersistent(self):
    """Verify incognito database is removed after incognito window closes."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self._CreateTable(windex=1)
    self._InsertRecord('text', windex=1)
    self.CloseBrowserWindow(1)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self.assertFalse(self._HasTable(windex=1))

  def testModificationsPersistAfterRendererCrash(self):
    """Verify database modifications persist after crashing tab."""
    self.AppendTab(pyauto.GURL('about:blank'))
    self.ActivateTab(0)
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('1')
    self.KillRendererProcess(
        self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid'])
    self.ReloadActiveTab()
    self.assertEqual(['1'], self._GetRecords())

  def testIncognitoDBPersistentAcrossTabs(self):
    """Test to check if database modifications are persistent across tabs
    in incognito window.
    """
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self._CreateTable(windex=1)
    self._InsertRecord('text', windex=1)
    self.AppendTab(pyauto.GURL(self.TEST_PAGE_URL), 1)
    self.assertEquals(['text'], self._GetRecords(1, 1))

  def testDatabasePersistsAfterRelaunch(self):
    """Verify database modifications persist after restarting browser."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('text')
    self.RestartBrowser(clear_profile=False)
    self.NavigateToURL(self.TEST_PAGE_URL)
    self.assertEquals(['text'], self._GetRecords())

  def testDeleteAndUpdateDatabase(self):
    """Verify can modify database after deleting it."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('text')
    # ClearBrowsingData doesn't return and times out
    self.ClearBrowsingData(['COOKIES'], 'EVERYTHING')
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateTable()
    self._InsertRecord('text2')
    self.assertEquals(['text2'], self._GetRecords())


if __name__ == '__main__':
  pyauto_functional.Main()
