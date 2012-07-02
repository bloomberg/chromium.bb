#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import pyauto_functional
import pyauto


class MultiprofileTest(pyauto.PyUITest):
  """Tests for Multi-Profile / Multi-users"""

  _RESTORE_STARTUP_URL_VALUE = 4
  _RESTORE_LASTOPEN_URL_VALUE = 1
  _RESTORE_DEFAULT_URL_VALUE = 0

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Hit <enter> to dump info.. ')
      self.pprint(self.GetMultiProfileInfo())

  def _GetSearchEngineWithKeyword(self, keyword, windex=0):
    """Get search engine info and return an element that matches keyword.

    Args:
      keyword: Search engine keyword field.
      windex: The window index, default is 0.

    Returns:
      A search engine info dict or None.
    """
    match_list = ([x for x in self.GetSearchEngineInfo(windex=windex)
                   if x['keyword'] == keyword])
    if match_list:
      return match_list[0]
    return None

  def _SetPreferences(self, dict, windex=0):
    """Sets preferences settings.

    Args:
      _dict: Dictionary of key preferences and its value to be set.
      windex: The window index, defaults to 0 (the first window).
    """
    for key in dict.iterkeys():
      self.SetPrefs(key, dict[key], windex=windex)

  def _SetStartUpPage(self, url, windex=0):
    """Set start up page.

    Args:
      url: URL of the page to be set as start up page.
      windex: The window index, default is 0.
    """
    _dict = {pyauto.kURLsToRestoreOnStartup: [url],
             pyauto.kRestoreOnStartup: self._RESTORE_STARTUP_URL_VALUE}
    self._SetPreferences(_dict, windex=windex)
    prefs_info = self.GetPrefsInfo(windex=windex).Prefs(
        pyauto.kURLsToRestoreOnStartup)
    self.assertTrue(url in prefs_info)

  def _SetHomePage(self, url, windex=0):
    """Create new profile and set home page.

    Args:
      url: URL of the page to be set as home page
      windex: The window index, default is 0.
    """
    _dict = {pyauto.kHomePage: url,
             pyauto.kHomePageIsNewTabPage: False, pyauto.kShowHomeButton: True,
             pyauto.kRestoreOnStartup: self._RESTORE_DEFAULT_URL_VALUE}
    self._SetPreferences(_dict, windex=windex)
    self.assertTrue(url in
                    self.GetPrefsInfo(windex=windex).Prefs(pyauto.kHomePage))

  def _SetSessionRestoreURLs(self, set_restore, windex=0):
    """Create new profile and set home page.

    Args:
      set_restore: Value of action of start up.
      windex: The window index, default is 0.
    """
    self.NavigateToURL('http://www.google.com/', windex)
    self.AppendTab(pyauto.GURL('http://news.google.com/'), windex)
    num_tabs = self.GetTabCount(windex)
    dict = {pyauto.kRestoreOnStartup: set_restore}
    self._SetPreferences(dict, windex=windex)

  def _AddSearchEngine(self, title, keyword, url, windex=0):
    """Add search engine.

    Args:
      title: Name for search engine.
      keyword: Keyword, used to initiate a custom search from omnibox.
      url: URL template for this search engine's query.
      windex: The window index, default is 0.
    """
    self.AddSearchEngine(title, keyword, url, windex=windex)
    name = self._GetSearchEngineWithKeyword(keyword, windex=windex)
    self.assertTrue(name)

  def _AssertStartUpPage(self, url, profile='Default'):
    """Asserts start up page for given profile.

    Args:
      url: URL of the page to be set as start up page
      profile: The profile name, defaults to 'Default'.
    """
    self.AppendBrowserLaunchSwitch('--profile-directory=' + profile)
    self.RestartBrowser(clear_profile=False)
    info = self.GetBrowserInfo()
    self.assertEqual(url, info['windows'][0]['tabs'][0]['url'].rstrip('/'))
    self.assertTrue(url in
                    self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup))

  def _AssertHomePage(self, url, profile='Default'):
    """Asserts home page for given profile.

    Args:
      url: URL of the page to be set as home page
      profile: The profile name, defaults to 'Dafault'.
    """
    self.AppendBrowserLaunchSwitch('--profile-directory=' + profile)
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(url in self.GetPrefsInfo().Prefs(pyauto.kHomePage))

  def _AssertDefaultSearchEngine(self, search_engine, profile='Default'):
    """Asserts default search engine for given profile.

    Args:
      search_engine: Name of default search engine.
      profile: The profile name, defaults to 'Default'.
    """
    self.AppendBrowserLaunchSwitch('--profile-directory=' + profile)
    self.RestartBrowser(clear_profile=False)
    name = self._GetSearchEngineWithKeyword(search_engine)
    self.assertTrue(name['is_default'])
    self.SetOmniboxText('test search')
    self.OmniboxAcceptInput()
    self.assertTrue(re.search(search_engine, self.GetActiveTabURL().spec()))

  def _AssertSessionRestore(self, url_list, set_restore, num_tabs=1,
                            profile='Default'):
    """Asserts urls when session is set to restored or set default.

    Args:
      url_list: List of URL to be restored.
      set_restore: Value of action of start up.
      num_tabs: Number of tabs to be restored, default is 1.
      profile: The profile name, defaults to 'Default'.
    """
    self.AppendBrowserLaunchSwitch('--profile-directory=' + profile)
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(num_tabs, self.GetTabCount())
    self.assertEqual(self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup),
                     set_restore)
    tab_index = 0
    while (tab_index < num_tabs):
      self.ActivateTab(tab_index)
      self.assertEqual(url_list[tab_index], self.GetActiveTabURL().spec())
      tab_index += 1

  def testBasic(self):
    """Multi-profile windows can open."""
    self.assertEqual(1, self.GetBrowserWindowCount())
    self.assertTrue(self.GetMultiProfileInfo()['enabled'],
        msg='Multi-profile is not enabled')
    self.OpenNewBrowserWindowWithNewProfile()
    # Verify multi-profile info.
    multi_profile = self.GetMultiProfileInfo()
    self.assertEqual(2, len(multi_profile['profiles']))
    new_profile = multi_profile['profiles'][1]
    self.assertTrue(new_profile['name'])

    # Verify browser windows.
    self.assertEqual(2, self.GetBrowserWindowCount(),
        msg='New browser window did not open')
    info = self.GetBrowserInfo()
    new_profile_window = info['windows'][1]
    self.assertEqual('Profile 1', new_profile_window['profile_path'])
    self.assertEqual(1, len(new_profile_window['tabs']))
    self.assertEqual('chrome://newtab/', new_profile_window['tabs'][0]['url'])

  def test20NewProfiles(self):
    """Verify we can create 20 new profiles."""
    for index in range(1, 21):
      self.OpenNewBrowserWindowWithNewProfile()
      multi_profile = self.GetMultiProfileInfo()
      self.assertEqual(index + 1, len(multi_profile['profiles']),
          msg='Expected %d profiles after adding %d new users. Got %d' % (
              index + 1, index, len(multi_profile['profiles'])))

  def testStartUpPageOptionInMultiProfile(self):
    """Test startup page for Multi-profile windows."""
    self.assertTrue(self.GetMultiProfileInfo()['enabled'],
                    msg='Multi-profile is not enabled')
    # Launch browser with new Profile 1, set startup page to 'www.google.com'.
    self.OpenNewBrowserWindowWithNewProfile()
    self._SetStartUpPage('http://www.google.com', windex=1)
    # Launch browser with new Profile 2, set startup page to 'www.yahoo.com'.
    self.OpenNewBrowserWindowWithNewProfile()
    # Verify start up page for Profile 2 is still newtab page.
    info = self.GetBrowserInfo()
    self.assertEqual('chrome://newtab/', info['windows'][2]['tabs'][0]['url'])
    self._SetStartUpPage('http://www.yahoo.com', windex=2)
    # Exit Profile 1 / Profile 2
    self.CloseBrowserWindow(1)
    self.CloseBrowserWindow(2)
    # Relaunch Browser with Profile 2, verify startup page.
    self._AssertStartUpPage('http://www.yahoo.com', profile='Profile 2')
    # Relaunch Browser with Profile 1, verify startup page.
    self._AssertStartUpPage('http://www.google.com', profile='Profile 1')

  def testHomePageOptionMultiProfile(self):
    """Test Home page for Multi-profile windows."""
    self.assertTrue(self.GetMultiProfileInfo()['enabled'],
                    msg='Multi-profile is not enabled')
    # Launch browser with new Profile 1, set homepage to 'www.google.com'.
    self.OpenNewBrowserWindowWithNewProfile()
    self._SetHomePage('http://www.google.com', windex=1)
    # Launch browser with new Profile 2, set homepage to 'www.yahoo.com'.
    self.OpenNewBrowserWindowWithNewProfile()
    self._SetHomePage('http://www.yahoo.com', windex=2)
    # Exit Profile 1 / Profile 2
    self.CloseBrowserWindow(1)
    self.CloseBrowserWindow(2)
    # Relaunch Browser with Profile 2, verify startup page.
    self._AssertHomePage('http://www.yahoo.com', profile='Profile 2')
    # Relaunch Browser with Profile 1, verify startup page.
    self._AssertHomePage('http://www.google.com', profile='Profile 1')

  def testSessionRestoreInMultiProfile(self):
    """Test session restore preference for Multi-profile windows."""
    self.assertTrue(self.GetMultiProfileInfo()['enabled'],
                    msg='Multi-profile is not enabled')
    # Launch browser with new Profile 1, set pref to restore session on
    # startup.
    self.OpenNewBrowserWindowWithNewProfile()
    self._SetSessionRestoreURLs(self._RESTORE_LASTOPEN_URL_VALUE, windex=1)
    # Launch browser with new Profile 2, do not set session restore pref.
    self.OpenNewBrowserWindowWithNewProfile()
    self._SetSessionRestoreURLs(self._RESTORE_DEFAULT_URL_VALUE, windex=2)
    # Exit Profile 1 / Profile 2
    self.CloseBrowserWindow(1)
    self.CloseBrowserWindow(2)
    # Relaunch Browser with Profile 1, verify session restores on startup.
    url_list = ['http://www.google.com/', 'http://news.google.com/']
    self._AssertSessionRestore(url_list, self._RESTORE_LASTOPEN_URL_VALUE,
                               num_tabs=2, profile='Profile 1')
    # Relaunch Browser with Profile 2, verify session does not get restored.
    url_list = ['chrome://newtab/']
    self._AssertSessionRestore(url_list, self._RESTORE_DEFAULT_URL_VALUE,
                               num_tabs=1, profile='Profile 2')

  def testInstantSearchInMultiProfile(self):
    """Test instant search for Multi-profile windows."""
    self.assertTrue(self.GetMultiProfileInfo()['enabled'],
                    msg='Multi-profile is not enabled')
    # Launch browser with new Profile 1, enable instant search
    self.OpenNewBrowserWindowWithNewProfile()
    self.SetPrefs(pyauto.kInstantEnabled, True, windex=1)
    self.assertTrue(self.GetPrefsInfo(windex=1).Prefs(pyauto.kInstantEnabled))
    # Launch browser with new Profile 2.
    self.OpenNewBrowserWindowWithNewProfile()
    # Exit Profile 1 / Profile 2
    self.CloseBrowserWindow(1)
    self.CloseBrowserWindow(2)
    # Relaunch Browser with Profile 1, verify instant search is enabled.
    self.AppendBrowserLaunchSwitch('--profile-directory=Profile 1')
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kInstantEnabled))
    # Relaunch Browser with Profile 2, verify instant search is disabled.
    self.AppendBrowserLaunchSwitch('--profile-directory=Profile 2')
    self.RestartBrowser(clear_profile=False)
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kInstantEnabled))

  def testMakeSearchEngineDefaultInMultiprofile(self):
    """Test adding and making a search engine default for Multi-profiles."""
    self.assertTrue(self.GetMultiProfileInfo()['enabled'],
                    msg='Multi-profile is not enabled')
    # Launch browser with new Profile 1, add search engine to 'Hulu'.
    self.OpenNewBrowserWindowWithNewProfile()
    self._AddSearchEngine('Hulu', 'hulu.com',
        'http://www.hulu.com/search?query=%s&ref=os&src={referrer:source?}', 1)
    self.MakeSearchEngineDefault('hulu.com', windex=1)
    # Launch browser with new Profile 2, add search engine to 'Youtube'.
    self.OpenNewBrowserWindowWithNewProfile()
    self._AddSearchEngine('YouTube Video Search', 'youtube.com',
        'http://www.youtube.com/results?search_query=%s&page={startPage?}'+
        '&utm_source=opensearch', 2)
    self.MakeSearchEngineDefault('youtube.com', windex=2)
    # Exit Profile 1 / Profile 2
    self.CloseBrowserWindow(1)
    self.CloseBrowserWindow(2)
    # Relaunch Browser with Profile 1, verify default search engine as 'Hulu'.
    self._AssertDefaultSearchEngine('hulu.com', profile='Profile 1')
    # Relaunch Browser with Profile 2, verify default search engine as
    # 'Youtube'.
    self._AssertDefaultSearchEngine('youtube.com', profile='Profile 2')

  def testDeleteSearchEngineInMultiprofile(self):
    """Test adding then deleting a search engine for Multi-profiles."""
    self.assertTrue(self.GetMultiProfileInfo()['enabled'],
                    msg='Multi-profile is not enabled')
    # Launch browser with new Profile 1, add 'foo.com' as new search engine.
    self.OpenNewBrowserWindowWithNewProfile()
    self._AddSearchEngine('foo', 'foo.com', 'http://foo/?q=%s', windex=1)
    # Launch browser with new Profile 2, add 'foo.com' as new search engine.
    self.OpenNewBrowserWindowWithNewProfile()
    self._AddSearchEngine('foo', 'foo.com', 'http://foo/?q=%s', windex=2)
    # Delete search engine 'foo.com' from Profile 1 and exit.
    self.DeleteSearchEngine('foo.com', windex=1)
    self.CloseBrowserWindow(1)
    # Exit Profile 2
    self.CloseBrowserWindow(2)
    # Relaunch Browser with Profile 1, verify search engine 'foo.com'
    # is deleted.
    self.AppendBrowserLaunchSwitch('--profile-directory=Profile 1')
    self.RestartBrowser(clear_profile=False)
    foo = self._GetSearchEngineWithKeyword('foo.com')
    self.assertFalse(foo)
    # Relaunch Browser with Profile 2, verify search engine 'foo.com'
    # is not deleted.
    self.AppendBrowserLaunchSwitch('--profile-directory=Profile 2')
    self.RestartBrowser(clear_profile=False)
    foo = self._GetSearchEngineWithKeyword('foo.com')
    self.assertTrue(foo)


if __name__ == '__main__':
  pyauto_functional.Main()
