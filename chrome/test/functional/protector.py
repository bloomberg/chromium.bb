#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported first
import pyauto
import test_utils

import logging
import os


class ProtectorTest(pyauto.PyUITest):
  """TestCase for Protector."""

  _default_search_id_key = 'Default Search Provider ID'

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    # Get the profile path of the first profile.
    profiles = self.GetMultiProfileInfo()
    self.assertTrue(profiles['profiles'])
    self._profile_path = profiles['profiles'][0]['path']
    self.assertTrue(self._profile_path)
    # Set to the keyword of the new default search engine after a successful
    # _GetDefaultSearchEngine call.
    self._new_default_search_keyword = None

  def _GetDefaultSearchEngine(self):
    """Returns the default search engine, if any; None otherwise.

    Returns:
       Dictionary describing the default search engine. See GetSearchEngineInfo
       for an example.
    """
    for search_engine in self.GetSearchEngineInfo():
      if search_engine['is_default']:
        return search_engine
    return None

  def _OpenDatabase(self, db_name):
    """Opens a given SQLite database in the default profile.

    Args:
      db_name: name of database file (relative to the profile directory).

    Returns:
      An sqlite3.Connection instance.

    Raises:
      ImportError if sqlite3 module is not found.
    """
    db_path = os.path.join(self._profile_path, db_name)
    logging.info('Opening DB: %s' % db_path)
    import sqlite3
    db_conn = sqlite3.connect(db_path)
    db_conn.isolation_level = None
    self.assertTrue(db_conn)
    return db_conn

  def _FetchSingleValue(self, conn, query, parameters=None):
    """Executes an SQL query that should select a single row with a single
    column and returns its value.

    Args:
      conn: sqlite3.Connection instance.
      query: SQL query (may contain placeholders).
      parameters: parameters to substitute for query.

    Returns:
      Value of the column fetched.
    """
    cursor = conn.cursor()
    cursor.execute(query, parameters)
    row = cursor.fetchone()
    self.assertTrue(row)
    self.assertEqual(1, len(row))
    return row[0]

  def _UpdateSingleRow(self, conn, query, parameters=None):
    """Executes an SQL query that should update a single row.

    Args:
      conn: sqlite3.Connection instance.
      query: SQL query (may contain placeholders).
      parameters: parameters to substitute for query.
    """
    cursor = conn.cursor()
    cursor.execute(query, parameters)
    self.assertEqual(1, cursor.rowcount)

  def _ChangeDefaultSearchEngine(self):
    """Replaces the default search engine in Web Data database with another one.

    Keywords of the new default search engine is saved to
    self._new_default_search_keyword.
    """
    web_database = self._OpenDatabase('Web Data')
    default_id = int(self._FetchSingleValue(
        web_database,
        'SELECT value FROM meta WHERE key = ?',
        (self._default_search_id_key,)))
    self.assertTrue(default_id)
    new_default_id = int(self._FetchSingleValue(
        web_database,
        'SELECT id FROM keywords WHERE id != ? LIMIT 1',
        (default_id,)))
    self.assertTrue(new_default_id)
    self.assertNotEqual(default_id, new_default_id)
    self._UpdateSingleRow(web_database,
                          'UPDATE meta SET value = ? WHERE key = ?',
                          (new_default_id, self._default_search_id_key))
    self._new_default_search_keyword = self._FetchSingleValue(
        web_database,
        'SELECT keyword FROM keywords WHERE id = ?',
        (new_default_id,))
    logging.info('Update default search ID: %d -> %d (%s)' %
                 (default_id, new_default_id, self._new_default_search_keyword))
    web_database.close()

  def testNoChangeOnCleanProfile(self):
    """Test that no change is reported on a clean profile."""
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testDetectSearchEngineChangeAndApply(self):
    """Test for detecting and applying a default search engine change."""
    # Get current search engine.
    old_default_search = self._GetDefaultSearchEngine()
    self.assertTrue(old_default_search)
    # Close browser, change the search engine and start it again.
    self.RestartBrowser(clear_profile=False,
                        pre_launch_hook=self._ChangeDefaultSearchEngine)
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    default_search = self._GetDefaultSearchEngine()
    # Protector must restore the old search engine.
    self.assertEqual(old_default_search, default_search)
    self.ApplyProtectorChange()
    # Now the search engine must have changed to the new one.
    default_search = self._GetDefaultSearchEngine()
    self.assertNotEqual(old_default_search, default_search)
    self.assertEqual(self._new_default_search_keyword,
                     default_search['keyword'])
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testDetectSearchEngineChangeAndDiscard(self):
    """Test for detecting and discarding a default search engine change."""
    # Get current search engine.
    old_default_search = self._GetDefaultSearchEngine()
    self.assertTrue(old_default_search)
    # Close browser, change the search engine and start it again.
    self.RestartBrowser(clear_profile=False,
                        pre_launch_hook=self._ChangeDefaultSearchEngine)
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    default_search = self._GetDefaultSearchEngine()
    # Protector must restore the old search engine.
    self.assertEqual(old_default_search, default_search)
    self.DiscardProtectorChange()
    # Old search engine remains active.
    default_search = self._GetDefaultSearchEngine()
    self.assertEqual(old_default_search, default_search)
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testSearchEngineChangeDismissedOnEdit(self):
    """Test that default search engine change is dismissed when default search
    engine is changed by user.
    """
    # Get current search engine.
    old_default_search = self._GetDefaultSearchEngine()
    self.assertTrue(old_default_search)
    # Close browser, change the search engine and start it again.
    self.RestartBrowser(clear_profile=False,
                        pre_launch_hook=self._ChangeDefaultSearchEngine)
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Change default search engine.
    self.MakeSearchEngineDefault(self._new_default_search_keyword)
    # Change is successful.
    default_search = self._GetDefaultSearchEngine()
    self.assertEqual(self._new_default_search_keyword,
                     default_search['keyword'])
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])


# TODO(ivankr): more hijacking cases (remove the current default search engine,
# add new search engines to the list, invalidate backup, etc).


if __name__ == '__main__':
  pyauto_functional.Main()
