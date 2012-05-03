#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported first
import pyauto
import test_utils

import json
import logging
import os


class BaseProtectorTest(pyauto.PyUITest):
  """Base class for Protector test cases."""

  _DEFAULT_SEARCH_ID_KEY = 'Default Search Provider ID'

  # Possible values for session.restore_on_startup pref:
  _SESSION_STARTUP_HOMEPAGE = 0  # For migration testing only.
  _SESSION_STARTUP_LAST = 1
  _SESSION_STARTUP_URLS = 4
  _SESSION_STARTUP_NTP = 5

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

  def _IsEnabled(self):
    """Whether protector should be enabled for the test suite."""
    return True

  def ExtraChromeFlags(self):
    """Adds required Protector-related flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    return super(BaseProtectorTest, self).ExtraChromeFlags() + [
        '--protector' if self._IsEnabled() else '--no-protector'
        ]

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
        (self._DEFAULT_SEARCH_ID_KEY,)))
    self.assertTrue(default_id)
    new_default_id = int(self._FetchSingleValue(
        web_database,
        'SELECT id FROM keywords WHERE id != ? LIMIT 1',
        (default_id,)))
    self.assertTrue(new_default_id)
    self.assertNotEqual(default_id, new_default_id)
    self._UpdateSingleRow(web_database,
                          'UPDATE meta SET value = ? WHERE key = ?',
                          (new_default_id, self._DEFAULT_SEARCH_ID_KEY))
    self._new_default_search_keyword = self._FetchSingleValue(
        web_database,
        'SELECT keyword FROM keywords WHERE id = ?',
        (new_default_id,))
    logging.info('Update default search ID: %d -> %d (%s)' %
                 (default_id, new_default_id, self._new_default_search_keyword))
    web_database.close()

  def _LoadPreferences(self):
    """Reads the contents of Preferences file.

    Returns: dict() with user preferences as returned by PrefsInfo.Prefs().
    """
    prefs_path = os.path.join(self._profile_path, 'Preferences')
    logging.info('Opening prefs: %s' % prefs_path)
    with open(prefs_path) as f:
      return json.load(f)

  def _WritePreferences(self, prefs):
    """Writes new contents to the Preferences file.

    Args:
      prefs: dict() with new user preferences as returned by PrefsInfo.Prefs().
    """
    with open(os.path.join(self._profile_path, 'Preferences'), 'w') as f:
      json.dump(prefs, f)

  def _InvalidatePreferencesBackup(self):
    """Makes the Preferences backup invalid by clearing the signature."""
    prefs = self._LoadPreferences()
    prefs['backup']['_signature'] = 'INVALID'
    self._WritePreferences(prefs)

  def _ChangeSessionStartupPrefs(self, startup_type=None, startup_urls=None,
                                 homepage=None, delete_migrated_pref=False):
    """Changes the session startup type and the list of URLs to load on startup.

    Args:
      startup_type: int with one of _SESSION_STARTUP_* values. If startup_type
          is None, then it deletes the preference.
      startup_urls: list(str) with a list of URLs; if None, is left unchanged.
      homepage: unless None, the new value for homepage.
      delete_migrated_pref: Whether we should delete the preference which says
          we've already migrated the startup_type preference.
    """
    prefs = self._LoadPreferences()
    if startup_type is None:
      del prefs['session']['restore_on_startup']
    else:
      prefs['session']['restore_on_startup'] = startup_type

    if startup_urls is not None:
      prefs['session']['urls_to_restore_on_startup'] = startup_urls
    if homepage is not None:
      prefs['homepage'] = homepage
      prefs['homepage_is_newtabpage'] = False
    if delete_migrated_pref:
      del prefs['session']['restore_on_startup_migrated']
    self._WritePreferences(prefs)

  def _ChangePinnedTabsPrefs(self, pinned_tabs):
    """Changes the list of pinned tabs.

    Args:
      pinned_tabs: list(str) with a list of pinned tabs URLs.
    """
    prefs = self._LoadPreferences()
    prefs['pinned_tabs'] = []
    for tab in pinned_tabs:
      prefs['pinned_tabs'].append({'url': tab})
    self._WritePreferences(prefs)

  def _ChangeHomepage(self, homepage, homepage_is_ntp, show_homepage_button):
    """Changes homepage settings.

    Args:
      homepage: new homepage URL (str),
      homepage_is_ntp: whether homepage is NTP.
      show_homepage_button: whether homepage button is visible.
    """
    prefs = self._LoadPreferences()
    prefs['homepage'] = homepage
    prefs['homepage_is_newtabpage'] = homepage_is_ntp
    prefs['browser']['show_home_button'] = show_homepage_button
    self._WritePreferences(prefs)

  def _AssertTabsOpen(self, urls, pinned=None):
    """Asserts that exactly one window with the specified URLs is open.

    Args:
      urls: list of URLs of expected open tabs.
      pinned: if given, list of boolean values whether the corresponding tab is
          expected to be pinned or not.
    """
    info = self.GetBrowserInfo()
    self.assertEqual(1, len(info['windows']))  # one window
    self.assertEqual(urls, [tab['url'] for tab in info['windows'][0]['tabs']])
    if pinned:
      self.assertEqual(pinned,
                       [tab['pinned'] for tab in info['windows'][0]['tabs']])

  def testNoChangeOnCleanProfile(self):
    """Test that no change is reported on a clean profile."""
    self.assertFalse(self.GetProtectorState()['showing_change'])


class ProtectorSearchEngineTest(BaseProtectorTest):
  """Test suite for search engine change detection with Protector enabled."""

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
    search_urls = [engine['url'] for engine in self.GetSearchEngineInfo()]
    # Verify there are no duplicate search engines:
    self.assertEqual(len(search_urls), len(set(search_urls)))

  def testSearchEngineChangeWithMultipleWindows(self):
    """Test that default search engine change is detected in multiple
    browser windows.
    """
    # Get current search engine.
    old_default_search = self._GetDefaultSearchEngine()
    self.assertTrue(old_default_search)
    # Close browser, change the search engine and start it again.
    self.RestartBrowser(clear_profile=False,
                        pre_launch_hook=self._ChangeDefaultSearchEngine)
    # The change must be detected by Protector in first window
    self.OpenNewBrowserWindow(True)
    self.assertTrue(self.GetProtectorState(window_index=0)['showing_change'])
    # Open another Browser Window
    self.OpenNewBrowserWindow(True)
    # The change must be detected by Protector in second window
    self.assertTrue(self.GetProtectorState(window_index=1)['showing_change'])

  def testSearchEngineChangeDiscardedOnRelaunchingBrowser(self):
    """Verify that relaunching the browser while Protector is showing a change
    discards it.
    """
    # Get current search engine.
    old_default_search = self._GetDefaultSearchEngine()
    self.assertTrue(old_default_search)
    # Close browser, change the search engine and start it again.
    self.RestartBrowser(clear_profile=False,
                        pre_launch_hook=self._ChangeDefaultSearchEngine)
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    default_search = self._GetDefaultSearchEngine()
    self.assertEqual(old_default_search, default_search)
    # After relaunching the browser, old search engine still must be active.
    self.RestartBrowser(clear_profile=False)
    default_search = self._GetDefaultSearchEngine()
    self.assertEqual(old_default_search, default_search)
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

# TODO(ivankr): more hijacking cases (remove the current default search engine,
# add new search engines to the list, invalidate backup, etc).


class ProtectorPreferencesTest(BaseProtectorTest):
  """Generic test suite for Preferences protection."""

  def testPreferencesBackupInvalid(self):
    """Test for detecting invalid Preferences backup."""
    # Set startup prefs to open specific URLs.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_URLS)
    self.SetPrefs(pyauto.kURLsToRestoreOnStartup, ['http://www.google.com/'])
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=self._InvalidatePreferencesBackup)
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Startup settings are reset to default (NTP).
    self.assertEqual(self._SESSION_STARTUP_NTP,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # Verify that previous startup URL has not been opened.
    self._AssertTabsOpen(['chrome://newtab/'])
    # Click "Edit Settings...".
    self.DiscardProtectorChange()
    # Verify that a new tab with settings is opened.
    info = self.GetBrowserInfo()
    self._AssertTabsOpen(['chrome://newtab/', 'chrome://chrome/settings/'])
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])
    self.RestartBrowser(clear_profile=False)
    # Not showing the change after a restart
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testPreferencesBackupInvalidRestoreLastSession(self):
    """Test that session restore setting is not reset if backup is invalid."""
    # Set startup prefs to restore the last session.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_LAST)
    previous_urls = ['chrome://version/', 'http://news.google.com/']
    self.NavigateToURL(previous_urls[0])
    for url in previous_urls[1:]:
      self.AppendTab(pyauto.GURL(url))
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=self._InvalidatePreferencesBackup)
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Startup settings are left unchanged.
    self.assertEqual(self._SESSION_STARTUP_LAST,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # Session has been restored.
    self._AssertTabsOpen(previous_urls)

  def testPreferencesBackupInvalidChangeDismissedOnEdit(self):
    """Test that editing protected prefs dismisses the invalid backup bubble."""
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=self._InvalidatePreferencesBackup)
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Change some protected setting manually.
    self.SetPrefs(pyauto.kHomePage, 'http://example.com/')
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])


class ProtectorSessionStartupTest(BaseProtectorTest):
  """Test suite for session startup changes detection with Protector enabled.
  """
  def testDetectSessionStartupChangeAndApply(self):
    """Test for detecting and applying a session startup pref change."""
    # Set startup prefs to restoring last open tabs.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_LAST)
    previous_urls = ['chrome://version/', 'http://news.google.com/']
    self.NavigateToURL(previous_urls[0])
    for url in previous_urls[1:]:
      self.AppendTab(pyauto.GURL(url))
    # Restart browser with startup prefs set to open google.com.
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeSessionStartupPrefs(
            self._SESSION_STARTUP_URLS,
            startup_urls=['http://www.google.com']))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Protector must restore old preference values.
    self.assertEqual(self._SESSION_STARTUP_LAST,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # Verify that open tabs are consistent with restored prefs.
    self._AssertTabsOpen(previous_urls)
    self.ApplyProtectorChange()
    # Now the new preference values are active.
    self.assertEqual(self._SESSION_STARTUP_URLS,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testDetectSessionStartupChangeAndDiscard(self):
    """Test for detecting and discarding a session startup pref change."""
    # Set startup prefs to restoring last open tabs.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_LAST)
    # Restart browser with startup prefs set to open google.com.
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeSessionStartupPrefs(
            self._SESSION_STARTUP_URLS,
            startup_urls=['http://www.google.com']))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Old preference values restored.
    self.assertEqual(self._SESSION_STARTUP_LAST,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    self.DiscardProtectorChange()
    # Old preference values are still active.
    self.assertEqual(self._SESSION_STARTUP_LAST,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testSessionStartupChangeDismissedOnEdit(self):
    """Test for that editing startup prefs manually dismissed the change."""
    # Set startup prefs to restoring last open tabs.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_LAST)
    # Restart browser with startup prefs set to open google.com.
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeSessionStartupPrefs(
            self._SESSION_STARTUP_URLS,
            startup_urls=['http://www.google.com']))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Change the setting manually.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_NTP)
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testSessionStartupPrefMigrationFromHomepage(self):
    """Test migration from old session.restore_on_startup values (homepage)."""
    # Set startup prefs to restoring last open tabs.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_LAST)
    self.SetPrefs(pyauto.kURLsToRestoreOnStartup, [])
    new_homepage = 'http://www.google.com/'
    # Restart browser with startup prefs set to open homepage (google.com).
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeSessionStartupPrefs(
            self._SESSION_STARTUP_HOMEPAGE,
            homepage=new_homepage,
            delete_migrated_pref=True))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Protector must restore old preference values.
    self.assertEqual(self._SESSION_STARTUP_LAST,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    self.assertEqual([],
                     self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup))
    self.ApplyProtectorChange()
    # Now the new preference values are active.
    self.assertEqual(self._SESSION_STARTUP_URLS,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # Homepage migrated to the list of startup URLs.
    self.assertEqual([new_homepage],
                     self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup))
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testSessionStartupPrefMigrationFromBlank(self):
    """Test migration from session.restore_on_startup being blank, as it would
    be for a user who had m18 or lower, and never changed that preference.
    """
    # Set startup prefs to restoring last open tabs.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_LAST)
    self.SetPrefs(pyauto.kURLsToRestoreOnStartup, [])
    # Set the homepage.
    new_homepage = 'http://www.google.com/'
    self.SetPrefs(pyauto.kHomePageIsNewTabPage, False)
    self.SetPrefs(pyauto.kHomePage, new_homepage)
    # Restart browser, clearing the 'restore on startup' pref, to simulate a
    # user coming from m18 and having left it on the default value.
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeSessionStartupPrefs(
            startup_type=None,
            delete_migrated_pref=True))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Protector must restore old preference values.
    self.assertEqual(self._SESSION_STARTUP_LAST,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    self.assertEqual([],
                     self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup))
    self.ApplyProtectorChange()
    # Now the new preference values are active.
    self.assertEqual(self._SESSION_STARTUP_URLS,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # Homepage migrated to the list of startup URLs.
    self.assertEqual([new_homepage],
                     self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup))
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testSessionStartupPrefNoMigrationOnHomepageChange(self):
    """Test that when the user modifies their homepage in m19+, we don't do the
    preference migration.
    """
    # Initially, the default value is selected for kRestoreOnStartup.
    self.assertEqual(self._SESSION_STARTUP_NTP,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # Set the homepage, but leave kRestoreOnStartup unchanged.
    new_homepage = 'http://www.google.com/'
    self.SetPrefs(pyauto.kHomePageIsNewTabPage, False)
    self.SetPrefs(pyauto.kHomePage, new_homepage)
    # Restart browser.
    self.RestartBrowser(clear_profile=False)
    # Now the new preference values are active.
    self.assertEqual(self._SESSION_STARTUP_NTP,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # kURLsToRestoreOnStartup pref is unchanged.
    self.assertEqual([],
                     self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup))
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testDetectPinnedTabsChangeAndApply(self):
    """Test for detecting and applying a change to pinned tabs."""
    pinned_urls = ['chrome://version/', 'chrome://credits/']
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangePinnedTabsPrefs(pinned_urls))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Protector must restore old preference values.
    self.assertEqual([],
                     self.GetPrefsInfo().Prefs(pyauto.kPinnedTabs))
    # No pinned tabs are open, only NTP.
    info = self.GetBrowserInfo()
    self._AssertTabsOpen(['chrome://newtab/'])
    self.ApplyProtectorChange()
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])
    # Pinned tabs should have been opened now in the correct order.
    self._AssertTabsOpen(pinned_urls + ['chrome://newtab/'],
                         pinned=[True, True, False])
    self.RestartBrowser(clear_profile=False)
    # Not showing the change after a restart
    self.assertFalse(self.GetProtectorState()['showing_change'])
    # Same pinned tabs are open.
    self._AssertTabsOpen(pinned_urls + ['chrome://newtab/'],
                         pinned=[True, True, False])

  def testDetectPinnedTabsChangeAndDiscard(self):
    """Test for detecting and discarding a change to pinned tabs."""
    pinned_url = 'chrome://version/'
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangePinnedTabsPrefs([pinned_url]))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Protector must restore old preference values.
    self.assertEqual([],
                     self.GetPrefsInfo().Prefs(pyauto.kPinnedTabs))
    # No pinned tabs are open, only NTP.
    info = self.GetBrowserInfo()
    self._AssertTabsOpen(['chrome://newtab/'])
    self.DiscardProtectorChange()
    # No longer showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])
    # Pinned tabs are not opened after another restart.
    self.RestartBrowser(clear_profile=False)
    self._AssertTabsOpen(['chrome://newtab/'])
    # Not showing the change after a restart.
    self.assertFalse(self.GetProtectorState()['showing_change'])


class ProtectorHomepageTest(BaseProtectorTest):
  """Test suite for homepage changes with Protector enabled."""

  def testDetectHomepageChangeAndApply(self):
    """Test that homepage change is detected and can be applied."""
    previous_homepage = 'http://example.com/'
    new_homepage = 'http://example.info/'
    self.SetPrefs(pyauto.kHomePage, previous_homepage)
    self.SetPrefs(pyauto.kHomePageIsNewTabPage, False)
    self.SetPrefs(pyauto.kShowHomeButton, False)
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeHomepage(new_homepage, False, True))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Protector must restore old preference values.
    self.assertEquals(previous_homepage,
                      self.GetPrefsInfo().Prefs(pyauto.kHomePage))
    self.assertEquals(False, self.GetPrefsInfo().Prefs(pyauto.kShowHomeButton))
    self.ApplyProtectorChange()
    # Now new values are active.
    self.assertEquals(new_homepage, self.GetPrefsInfo().Prefs(pyauto.kHomePage))
    self.assertEquals(True, self.GetPrefsInfo().Prefs(pyauto.kShowHomeButton))
    # No longer showing the change
    self.assertFalse(self.GetProtectorState()['showing_change'])

  def testDetectHomepageChangeAndDiscard(self):
    """Test that homepage change is detected and can be discarded."""
    previous_homepage = 'http://example.com/'
    new_homepage = 'http://example.info/'
    self.SetPrefs(pyauto.kHomePage, previous_homepage)
    self.SetPrefs(pyauto.kHomePageIsNewTabPage, False)
    self.SetPrefs(pyauto.kShowHomeButton, False)
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeHomepage(new_homepage, False, True))
    # The change must be detected by Protector.
    self.assertTrue(self.GetProtectorState()['showing_change'])
    # Protector must restore old preference values.
    self.assertEquals(previous_homepage,
                      self.GetPrefsInfo().Prefs(pyauto.kHomePage))
    self.assertEquals(False, self.GetPrefsInfo().Prefs(pyauto.kShowHomeButton))
    self.DiscardProtectorChange()
    # Nothing changed
    self.assertEquals(previous_homepage,
                      self.GetPrefsInfo().Prefs(pyauto.kHomePage))
    self.assertEquals(False, self.GetPrefsInfo().Prefs(pyauto.kShowHomeButton))
    # No longer showing the change
    self.assertFalse(self.GetProtectorState()['showing_change'])


class ProtectorDisabledTest(BaseProtectorTest):
  """Test suite for Protector in disabled state."""

  def _IsEnabled(self):
    """Overriden from BaseProtectorTest to disable Protector."""
    return False

  def testNoSearchEngineChangeReported(self):
    """Test that the default search engine change is neither reported to user
    nor reverted.
    """
    # Get current search engine.
    old_default_search = self._GetDefaultSearchEngine()
    self.assertTrue(old_default_search)
    # Close browser, change the search engine and start it again.
    self.RestartBrowser(clear_profile=False,
                        pre_launch_hook=self._ChangeDefaultSearchEngine)
    # The change must not be reported by Protector.
    self.assertFalse(self.GetProtectorState()['showing_change'])
    default_search = self._GetDefaultSearchEngine()
    # The new search engine must be active.
    self.assertEqual(self._new_default_search_keyword,
                     default_search['keyword'])

  def testNoPreferencesBackupInvalidReported(self):
    """Test that invalid Preferences backup is not reported."""
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_URLS)
    new_url = 'chrome://version/'
    self.SetPrefs(pyauto.kURLsToRestoreOnStartup, [new_url])
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=self._InvalidatePreferencesBackup)
    # The change must not be reported by Protector.
    self.assertFalse(self.GetProtectorState()['showing_change'])
    # New preference values must be active.
    self.assertEqual(self._SESSION_STARTUP_URLS,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # Verify that open tabs are consistent with new prefs.
    self._AssertTabsOpen([new_url])

  def testNoSessionStartupChangeReported(self):
    """Test that the session startup change is neither reported nor reverted."""
    # Set startup prefs to restoring last open tabs.
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_LAST)
    new_url = 'chrome://version/'
    self.NavigateToURL('http://www.google.com/')
    # Restart browser with startup prefs set to open google.com.
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeSessionStartupPrefs(
            self._SESSION_STARTUP_URLS,
            startup_urls=[new_url]))
    # The change must not be reported by Protector.
    self.assertFalse(self.GetProtectorState()['showing_change'])
    # New preference values must be active.
    self.assertEqual(self._SESSION_STARTUP_URLS,
                     self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    # Verify that open tabs are consistent with new prefs.
    self._AssertTabsOpen([new_url])

  def testNoHomepageChangeReported(self):
    """Test that homepage change is neither reported nor reverted."""
    new_homepage = 'http://example.info/'
    self.SetPrefs(pyauto.kHomePage, 'http://example.com/')
    self.SetPrefs(pyauto.kHomePageIsNewTabPage, False)
    self.SetPrefs(pyauto.kShowHomeButton, False)
    self.RestartBrowser(
        clear_profile=False,
        pre_launch_hook=lambda: self._ChangeHomepage(new_homepage, False, True))
    # Not showing the change.
    self.assertFalse(self.GetProtectorState()['showing_change'])
    # New values must be active.
    self.assertEquals(new_homepage, self.GetPrefsInfo().Prefs(pyauto.kHomePage))
    self.assertEquals(True, self.GetPrefsInfo().Prefs(pyauto.kShowHomeButton))


if __name__ == '__main__':
  pyauto_functional.Main()
