#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import optparse
import os
import shutil
import sys
import tempfile
import zipfile


import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils


class ImportsTest(pyauto.PyUITest):
  """Import settings from other browsers.

  Import settings tables below show which items get imported on first run and
  via preferences for different browsers and operating systems.

            Bookmarks   History   SearchEngines   Passwords   Homepage
  Firefox:
  Win/FRUI      N          Y            N             N          Y
  Win/Prefs     Y          Y            Y             Y          N
  Mac&Lin/FRUI  Y          Y            Y             Y          Y
  Mac&Lin/Prefs Y          Y            Y             Y          N

  Safari:
  Mac/FRUI      Y          Y            Y             Y          N
  Mac/Prefs     Y          Y            Y             Y          N
  """
  def setUp(self):
    self._to_import = ['ALL']

    if pyauto.PyUITest.IsMac():
      self._firefox_profiles_path = os.path.join(
          os.environ['HOME'], 'Library','Application Support','Firefox')
      self._firefox_test_profile = os.path.abspath(os.path.join(
          pyauto.PyUITest.DataDir(), 'import', 'firefox', 'macwin.zip'))
      self._safari_profiles_path = os.path.join(
          os.environ['HOME'], 'Library', 'Safari')
      # Don't import passwords to avoid Keychain popups. See crbug.com/49378.
      self._to_import = ['HISTORY', 'FAVORITES', 'SEARCH_ENGINES', 'HOME_PAGE']
    elif pyauto.PyUITest.IsWin():
      self._firefox_profiles_path = os.path.join(
          os.getenv('APPDATA'), 'Mozilla', 'Firefox')
      self._firefox_test_profile = os.path.abspath(os.path.join(
          pyauto.PyUITest.DataDir(), 'import', 'firefox', 'macwin.zip'))
    else:  # Linux
      self._firefox_profiles_path = os.path.join(
          os.environ['HOME'], '.mozilla', 'firefox')
      self._firefox_test_profile = os.path.abspath(os.path.join(
          pyauto.PyUITest.DataDir(), 'import', 'firefox', 'linux.zip'))

    # Expected items for tests.
    self._history_items = ['Google', 'Google News', u'Google \ub3c4\uc11c']
    self._bookmark_bar_items = ['Google News', 'Google', u'Google \ub3c4\uc11c']
    self._bookmark_folder_items = []
    self._password_items = ['etouchqa@gmail.com', 'macqa05']
    self._home_page = 'http://news.google.com/'

    self._safari_replacer = None
    self._firefox_replacer = None

    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    # Delete any replacers to restore the original profiles.
    if self._safari_replacer:
      del self._safari_replacer
    if self._firefox_replacer:
      del self._firefox_replacer

  def _UnzipProfileToDir(self, profile_zip, dir):
    """Unzip |profile_zip| into directory |dir|.

    Creates |dir| if it doesn't exist.
    """
    if not os.path.isdir(dir):
      os.makedirs(dir)
    zf = zipfile.ZipFile(profile_zip)
    for name in zf.namelist():
      full_path = os.path.join(dir, name)
      if name.endswith('/'):
        if not os.path.isdir(full_path):
          os.makedirs(full_path)
      else:
        zf.extract(name, dir)
      os.chmod(full_path, 0777)

  def _SwapFirefoxProfile(self):
    """Swaps the test Firefox profile with the original one."""
    self._firefox_replacer = pyauto_utils.ExistingPathReplacer(
        self._firefox_profiles_path)
    self._UnzipProfileToDir(self._firefox_test_profile,
                            self._firefox_profiles_path)

  def _SwapSafariProfile(self):
    """Swaps the test Safari profile with the original one."""
    self._safari_replacer = pyauto_utils.ExistingPathReplacer(
        self._safari_profiles_path)
    self._UnzipProfileToDir(
        os.path.join(self.DataDir(), 'import', 'safari', 'mac.zip'),
        self._safari_profiles_path)

  def _CheckForBookmarks(self, bookmark_titles, bookmark_bar):
    """Checks that the given bookmarks exist.

    Args:
      bookmark_titles: A set of bookmark title strings.
      bookmark_bar: True if the bookmarks are part of the bookmark bar.
                    False otherwise.
    """
    bookmarks = self.GetBookmarkModel()
    if bookmark_bar:
      node = bookmarks.BookmarkBar()
    else:
      node = bookmarks.Other()
    for title in bookmark_titles:
      self.assertTrue([x for x in bookmark_titles \
                       if bookmarks.FindByTitle(title, [node])])

  def _BookmarkDuplicatesExist(self, bookmark_titles):
    """Returns true if any of the bookmark titles are duplicated.

    Args:
      bookmark_titles: A list of bookmark title strings.
    """
    bookmarks = self.GetBookmarkModel()
    for title in bookmark_titles:
      if len(bookmarks.FindByTitle(title)) > 1:
        return True
    return False

  def _CheckForHistory(self, history_titles):
    """Verifies that the given list of history items are in the history.

    Args:
      history_titles: A list of history title strings.
    """
    history = self.GetHistoryInfo().History()

    # History import automation is broken - crbug.com/63001
    return

    for title in history_titles:
      self.assertTrue([x for x in history if x['title'] == title])

  def _CheckForPasswords(self, usernames):
    """Check that password items exist for the given usernames."""
    # Password import automation does not work on Mac. See crbug.com/52124.
    if self.IsMac():
      return
    passwords = self.GetSavedPasswords()
    for username in usernames:
      self.assertTrue([x for x in passwords if x['username_value'] == username])

  def _CheckDefaults(self, bookmarks, history, passwords, home_page,
                     search_engines):
    """Checks the defaults for each of the possible import items.

    All arguments are True if they should be checked, False otherwise."""
    if bookmarks:
      self._CheckForBookmarks(self._bookmark_bar_items, True)
      self._CheckForBookmarks(self._bookmark_folder_items, False)
    if history:
      self._CheckForHistory(self._history_items)
    if passwords:
      self._CheckForPasswords(self._password_items)
    if home_page:
      self.assertEqual(self._home_page, self.GetPrefsInfo().Prefs()['homepage'])
    # TODO(alyssad): Test for search engines after a hook is added.
    # See crbug.com/52009.

  def _CanRunFirefoxTests(self):
    """Determine whether we can run firefox imports.

    On windows, checks if firefox is installed. Always True on other platforms.
    """
    if self.IsWin():
      ff_installed = os.path.exists(os.path.join(
          os.getenv('ProgramFiles'), 'Mozilla Firefox', 'firefox.exe'))
      if not ff_installed:
        logging.warn('Firefox not installed.')
      return ff_installed
    # TODO(nirnimesh): Anything else to be done on other platforms?
    return True

  def _ImportFromFirefox(self, bookmarks, history, passwords, home_page,
                         search_engines):
    """Verify importing individual Firefox data through preferences"""
    if not self._CanRunFirefoxTests():
      logging.warn('Not running firefox import tests.')
      return
    self._SwapFirefoxProfile()
    self.ImportSettings('Mozilla Firefox', False, self._to_import)
    self._CheckDefaults(bookmarks, history, passwords, home_page,
                        search_engines)

  # Tests.
  def testFirefoxImportFromPrefs(self):
    """Verify importing Firefox data through preferences."""
    if not self._CanRunFirefoxTests():
      logging.warn('Not running firefox import tests.')
      return
    if self.IsWinVista():  # Broken on vista. crbug.com/89768
      return
    self._SwapFirefoxProfile()
    self.ImportSettings('Mozilla Firefox', False, self._to_import)
    self._CheckDefaults(bookmarks=True, history=True, passwords=True,
                        home_page=False, search_engines=True)

  def testFirefoxFirstRun(self):
    """Verify importing from Firefox on the first run.

    For Win, only history and homepage will only be imported.
    Mac and Linux can import history, homepage, and bookmarks.
    """
    if not self._CanRunFirefoxTests():
      logging.warn('Not running firefox import tests.')
      return
    self._SwapFirefoxProfile()
    self.ImportSettings('Mozilla Firefox', True, self._to_import)
    non_win = not self.IsWin()
    self._CheckDefaults(bookmarks=non_win, history=True, passwords=True,
                        home_page=non_win, search_engines=True)

  def testImportFirefoxDataTwice(self):
    """Verify importing Firefox data twice.

    Bookmarks should be duplicated, but history and passwords should not.
    """
    if not self._CanRunFirefoxTests():
      logging.warn('Not running firefox import tests.')
      return
    if self.IsWinVista():  # Broken on vista. crbug.com/89768
      return
    self._SwapFirefoxProfile()
    self.ImportSettings('Mozilla Firefox', False, self._to_import)
    num_history_orig = len(self.GetHistoryInfo().History())
    num_passwords_orig = len(self.GetSavedPasswords())

    # Re-import and check for duplicates.
    self.ImportSettings('Mozilla Firefox', False, self._to_import)
    self.assertTrue(self._BookmarkDuplicatesExist(
        self._bookmark_bar_items + self._bookmark_folder_items))
    self.assertEqual(num_history_orig, len(self.GetHistoryInfo().History()))
    self.assertEqual(num_passwords_orig, len(self.GetSavedPasswords()))

  def testImportFirefoxBookmarksFromPrefs(self):
    """Verify importing Firefox bookmarks through preferences."""
    self._ImportFromFirefox(bookmarks=True, history=False, passwords=False,
                            home_page=False, search_engines=False)

  def testImportFirefoxHistoryFromPrefs(self):
    """Verify importing Firefox history through preferences."""
    self._ImportFromFirefox(bookmarks=False, history=True, passwords=False,
                            home_page=False, search_engines=False)

  def testImportFirefoxPasswordsFromPrefs(self):
    """Verify importing Firefox passwords through preferences."""
    if self.IsWinVista():  # Broken on vista. crbug.com/89768
      return
    self._ImportFromFirefox(bookmarks=False, history=False, passwords=True,
                            home_page=False, search_engines=False)

  def testImportFirefoxSearchEnginesFromPrefs(self):
    """Verify importing Firefox search engines through preferences."""
    self._ImportFromFirefox(bookmarks=False, history=False, passwords=False,
                            home_page=False, search_engines=True)

  def testImportFromFirefoxAndSafari(self):
    """Verify importing from Firefox and then Safari."""
    # This test is for Mac only.
    if not self.IsMac():
      return

    self._SwapSafariProfile()
    self._SwapFirefoxProfile()
    self.ImportSettings('Mozilla Firefox', False, self._to_import)
    self.ImportSettings('Safari', False, self._to_import)

    self._CheckDefaults(bookmarks=True, history=True, passwords=True,
                        home_page=False, search_engines=True)
    self.assertTrue(self._BookmarkDuplicatesExist(
        self._bookmark_bar_items + self._bookmark_folder_items))

  def testSafariImportFromPrefs(self):
    """Verify importing Safari data through preferences."""
    # This test is Mac only.
    if not self.IsMac():
      return
    self._SwapSafariProfile()
    self.ImportSettings('Safari', False, self._to_import)
    self._CheckDefaults(bookmarks=True, history=True, passwords=False,
                        home_page=False, search_engines=True)

  def testSafariFirstRun(self):
    """Verify importing Safari data on the first run."""
    # This test is Mac only.
    if not self.IsMac():
      return
    self._SwapSafariProfile()
    self.ImportSettings('Safari', False, self._to_import)
    self._CheckDefaults(bookmarks=True, history=True, passwords=False,
                        home_page=False, search_engines=False)

  def testImportSafariDataTwice(self):
    """Verify importing Safari data twice.

    Bookmarks should be duplicated, but history and passwords should not."""
    # This test is Mac only.
    if not self.IsMac():
      return
    self._SwapSafariProfile()
    self.ImportSettings('Safari', False, self._to_import)
    num_history_orig = len(self.GetHistoryInfo().History())
    num_passwords_orig = len(self.GetSavedPasswords())

    # Re-import and check for duplicates.
    self.ImportSettings('Safari', False, self._to_import)
    self.assertTrue(self._BookmarkDuplicatesExist(
        self._bookmark_bar_items + self._bookmark_folder_items))
    self.assertEqual(num_history_orig, len(self.GetHistoryInfo().History()))
    self.assertEqual(num_passwords_orig, len(self.GetSavedPasswords()))


if __name__ == '__main__':
  pyauto_functional.Main()
