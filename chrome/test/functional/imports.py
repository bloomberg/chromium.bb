#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
      self._firefox_test_profile = os.path.join(
          pyauto.PyUITest.DataDir(), 'import', 'firefox', 'macwin.zip')
      self._safari_profiles_path = os.path.join(
          os.environ['HOME'], 'Library', 'Safari')
      # Set the path here since it can't get set on the browser side and it is
      # necessary for importing passwords on Mac.
      self._old_path = None
      if 'DYLD_FALLBACK_LIBRARY_PATH' in os.environ:
        self._old_path = os.environ['DYLD_FALLBACK_LIBRARY_PATH']
      os.environ['DYLD_FALLBACK_LIBRARY_PATH'] = os.path.join(
          self.DataDir(), 'firefox3_nss_mac')
      # Don't import passwords to avoid Keychain popups. See crbug.com/49378.
      self._to_import = ['HISTORY', 'FAVORITES', 'SEARCH_ENGINES', 'HOME_PAGE']
    elif pyauto.PyUITest.IsWin():
      self._firefox_profiles_path = os.path.join(
          os.getenv('APPDATA'), 'Mozilla', 'Firefox')
      self._firefox_test_profile = os.path.join(
          pyauto.PyUITest.DataDir(), 'import', 'firefox', 'macwin.zip')
    else:
      self._firefox_profiles_path = os.path.join(
          os.environ['HOME'], '.mozilla', 'firefox')
      self._firefox_test_profile = os.path.join(
          pyauto.PyUITest.DataDir(), 'import', 'firefox', 'linux.zip')

    # Expected items for tests.
    self._history_items = ['Google', 'Google News', u'Google \ub3c4\uc11c']
    self._bookmark_bar_items = ['Google News']
    self._bookmark_folder_items = ['Google', u'Google \ub3c4\uc11c']
    self._password_items = ['etouchqa@gmail.com', 'macqa05']
    self._home_page = 'http://news.google.com/'

    self._safari_replacer = None
    self._firefox_replacer = None

    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    # Re-set the path to its state before the test.
    if self.IsMac():
      if self._old_path:
        os.environ['DYLD_FALLBACK_LIBRARY_PATH'] = self._old_path
      else:
        os.environ.pop('DYLD_FALLBACK_LIBRARY_PATH')
    # Delete any replacers to restore the original profiles.
    if self._safari_replacer:
      del self._safari_replacer
    if self._firefox_replacer:
      del self._firefox_replacer

  def _UnzipProfileToDir(self, profile_zip, dir):
    """Unzip |profile_zip| into directory |dir|.

    Creates |dir| if it doesn't exist.
    """
    zf = zipfile.ZipFile(profile_zip)
    # Make base.
    pushd = os.getcwd()
    try:
      if not os.path.isdir(dir):
        os.mkdir(dir)
      os.chdir(dir)
      # Extract files.
      for info in zf.infolist():
        name = info.filename
        if name.endswith('/'):  # It's a directory.
          if not os.path.isdir(name):
            os.makedirs(name)
        else:  # It's a file.
          dir = os.path.dirname(name)
          if dir and not os.path.isdir(dir):
            os.makedirs(dir)
          out = open(name, 'wb')
          out.write(zf.read(name))
          out.close()
        # Set permissions.
        os.chmod(name, 0777)
    finally:
      os.chdir(pushd)

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
    confirmed_titles = set()
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

  # Tests.
  def testFirefoxImportFromPrefs(self):
    """Verify importing Firefox data through preferences."""
    self._SwapFirefoxProfile()
    self.ImportSettings('Mozilla Firefox', False, self._to_import)
    self._CheckDefaults(bookmarks=True, history=True, passwords=True,
                        home_page=False, search_engines=True)

  def testFirefoxFirstRun(self):
    """Verify importing from Firefox on the first run.

    For Win, only history and homepage will only be imported.
    Mac and Linux can import history, homepage, and bookmarks.
    """
    self._SwapFirefoxProfile()
    self.ImportSettings('Mozilla Firefox', True, self._to_import)
    non_win = not self.IsWin()
    self._CheckDefaults(bookmarks=non_win, history=True, passwords=True,
                        home_page=non_win, search_engines=True)

  def testImportFirefoxDataTwice(self):
    """Verify importing Firefox data twice.

    Bookmarks should be duplicated, but history and passwords should not.
    """
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
