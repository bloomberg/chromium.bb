#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import re

import pyauto_functional  # must be imported before pyauto
import pyauto


class EnterpriseTest(pyauto.PyUITest):
  """Test for Enterprise features.

  Browser preferences will be managed using policies. These managed preferences
  cannot be modified by user. This works only for Google Chrome, not Chromium.

  On Linux, assume that 'suid-python' (a python binary setuid root) is
  available on the machine under /usr/local/bin directory.
  """
  assert pyauto.PyUITest.IsWin() or pyauto.PyUITest.IsLinux(), \
         'Only runs on Win or Linux'

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump prefs... ')
      self.pprint(self.GetPrefsInfo().Prefs())

  @staticmethod
  def _Cleanup():
    """Removes the registry key and its subkeys(if they exist).

    Win: Registry Key being deleted: HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google
    Linux: Removes the chrome directory from /etc/opt
    """
    if pyauto.PyUITest.IsWin():
      if subprocess.call(
          r'reg query HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google') == 0:
        logging.debug(r'Removing HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google')
        subprocess.call(r'reg delete HKLM\Software\Policies\Google /f')
    elif pyauto.PyUITest.IsLinux():
      sudo_cmd_file = os.path.join(os.path.dirname(__file__),
                                   'enterprise_helper_linux.py')
      if os.path.isdir ('/etc/opt/chrome'):
        logging.debug('Removing directory /etc/opt/chrome/')
        subprocess.call(['suid-python', sudo_cmd_file,
                         'remove_dir', '/etc/opt/chrome'])

  @staticmethod
  def _SetUp():
    """Win: Add the registry keys from the .reg file.

    Removes the registry key and its subkeys if they exist.
    Adding HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google.

    Linux: Copy the chrome.json file to the managed directory.
    Remove /etc/opt/chrome directory if it exists.
    """
    EnterpriseTest._Cleanup()
    if pyauto.PyUITest.IsWin():
      registry_location = os.path.join(EnterpriseTest.DataDir(), 'enterprise',
                                       'chrome-add.reg')
      # Add the registry keys
      subprocess.call('reg import %s' % registry_location)
    elif pyauto.PyUITest.IsLinux():
      chrome_json = os.path.join(EnterpriseTest.DataDir(),
                                       'enterprise', 'chrome.json')
      sudo_cmd_file = os.path.join(os.path.dirname(__file__),
                                   'enterprise_helper_linux.py')
      policies_location = '/etc/opt/chrome/policies/managed'
      subprocess.call(['suid-python',  sudo_cmd_file,
                       'setup_dir', policies_location])
      # Copy chrome.json file to the managed directory
      subprocess.call(['suid-python',  sudo_cmd_file,
                       'copy', chrome_json, policies_location])

  def setUp(self):
    # Add policies through registry or json file.
    self._SetUp()
    # Check if registries are created in Win.
    if pyauto.PyUITest.IsWin():
      registry_query_code = subprocess.call(
        r'reg query HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google')
      assert registry_query_code == 0, 'Could not create registries.'
    # Check if json file is copied under correct location in Linux.
    elif pyauto.PyUITest.IsLinux():
      policy_file_check = os.path.isfile(
                            '/etc/opt/chrome/policies/managed/chrome.json')
      assert policy_file_check, 'Policy file(s) not set up.'
    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    EnterpriseTest._Cleanup()

  def _CheckIfPrefCanBeModified(self, key, defaultval, newval):
    """Check if the managed preferences can be modified.

    Args:
      key: The preference key that you want to modify
      defaultval: Default value of the preference that we are trying to modify
      newval: New value that we are trying to set
    """
    # Check if the default value of the preference is set as expected.
    self.assertEqual(self.GetPrefsInfo().Prefs(key), defaultval,
                     msg='Default value of the preference is wrong.')
    self.assertRaises(pyauto.JSONInterfaceError,
                      lambda: self.SetPrefs(key, newval))

  def _GetPluginPID(self, plugin_name):
    """Fetch the pid of the plugin process with name |plugin_name|."""
    child_processes = self.GetBrowserInfo()['child_processes']
    plugin_type = 'Plug-in'
    for x in child_processes:
      if x['type'] == plugin_type and re.search(plugin_name, x['name']):
        return x['pid']
    return None

  # Tests for options in Basics
  def testStartupPages(self):
    """Verify that user cannot modify the startup page options."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    # Verify startup option
    self.assertEquals(4, self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    self.assertRaises(pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kRestoreOnStartup, 1))

    # Verify URLs to open on startup
    self.assertEquals(['http://chromium.org'],
        self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup))
    self.assertRaises(pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kURLsToRestoreOnStartup,
                              ['http://www.google.com']))

  def testHomePageOptions(self):
    """Verify that we cannot modify Homepage URL."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    # Try to configure home page URL
    self.assertEquals('http://chromium.org',
                      self.GetPrefsInfo().Prefs('homepage'))
    self.assertRaises(pyauto.JSONInterfaceError,
        lambda: self.SetPrefs('homepage', 'http://www.google.com'))

    # Try to remove NTP as home page
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kHomePageIsNewTabPage))
    self.assertRaises(pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kHomePageIsNewTabPage, False))

  def testShowHomeButton(self):
    """Verify that home button option cannot be modified when it's managed."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kShowHomeButton, True, False)

  def testInstant(self):
    """Verify that Instant option cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kInstantEnabled, True, False)

  # Tests for options in Personal Stuff
  def testPasswordManager(self):
    """Verify that password manager preference cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kPasswordManagerEnabled, True, False)
  
  def testPasswordManagerNotAllowShowPasswords(self):
    """Verify that password manager preference not to show passwords 
    cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kPasswordManagerAllowShowPasswords,
                                   False, True)

  # Tests for options in Under the Hood
  def testPrivacyPrefs(self):
    """Verify that the managed preferences cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    prefs_list = [
        # (preference key, default value, new value)
        (pyauto.kAlternateErrorPagesEnabled, True, False),
        (pyauto.kAutofillEnabled, False, True),
        (pyauto.kNetworkPredictionEnabled, True, False),
        (pyauto.kSafeBrowsingEnabled, True, False),
        (pyauto.kSearchSuggestEnabled, True, False),
        ]
    # Check if the policies got applied by trying to modify
    for key, defaultval, newval in prefs_list:
      logging.info('Checking pref %s', key)
      self._CheckIfPrefCanBeModified(key, defaultval, newval)

  def testClearSiteDataOnExit(self):
    """Verify that clear data on exit preference cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kClearSiteDataOnExit, True, False)

  def testBlockThirdPartyCookies(self):
    """Verify that clear data on exit preference cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kBlockThirdPartyCookies, True, False)

  # Tests for general options
  def testApplicationLocale(self):
    """Verify that Chrome can be launched only in a specific locale."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    if self.IsWin():
      self.assertTrue(re.search('hi',
                      self.GetPrefsInfo().Prefs()['intl']['accept_languages']),
                      msg='Chrome locale is not Hindi.')
      # TODO(sunandt): Try changing the application locale to another language.
    elif self.IsLinux():
      logging.info("Locale policy not supported in Linux")
      pass

  def testDisableDevTools(self):
    """Verify that devtools window cannot be launched."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    # DevTools process can be seen by PyAuto only when it's undocked.
    self.SetPrefs(pyauto.kDevToolsOpenDocked, False)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEquals(1, len(self.GetBrowserInfo()['windows']),
                      msg='Devtools window launched.')

  def testDisableSPDY(self):
    """Verify that SPDY is disabled."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self.NavigateToURL('chrome://net-internals/#spdy')
    self.assertEquals(0,
                      self.FindInPage('encrypted.google.com')['match_count'])
    self.AppendTab(pyauto.GURL('https://encrypted.google.com'))
    self.assertEquals('Google', self.GetActiveTabTitle())
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertEquals(0,
        self.FindInPage('encrypted.google.com', tab_index=0)['match_count'],
        msg='SPDY is not disabled.')

  def testDisabledPlugins(self):
    """Verify that disabled plugins cannot be enabled."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search('Flash', plugin['name']):
        self.assertRaises(pyauto.JSONInterfaceError,
                          lambda: self.EnablePlugin(plugin['path']))
        return

  def testDisabledPluginsException(self):
    """Verify that plugins given exceptions can be managed by users.
    Chrome PDF Viewer is disabled using DisabledPlugins policy.
    User can still toggle the plugin setting when an exception is given for a
    plugin. So we are trying to enable Chrome PDF Viewer.
    """
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search('Chrome PDF Viewer', plugin['name']):
        self.EnablePlugin(plugin['path'])
        return

  def testSetDownloadDirectory(self):
    """Verify that the downloads directory and prompt for download preferences
    cannot be modified.
    """
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self.assertEqual('Downloads',
      self.GetPrefsInfo().Prefs()['download']['default_directory'],
      msg='Downloads directory is not set correctly.')
    if self.IsWin():
      # Check for changing the download directory location
      self.assertRaises(pyauto.JSONInterfaceError,
          lambda: self.SetPrefs(pyauto.kDownloadDefaultDirectory,
                                os.getenv('USERPROFILE')))
    elif self.IsLinux():
      self.assertRaises(pyauto.JSONInterfaceError,
          lambda: self.SetPrefs(pyauto.kDownloadDefaultDirectory,
                                os.getenv('HOME')))
    # Check for changing the option 'Ask for each download'
    self.assertRaises(pyauto.JSONInterfaceError,
                      lambda: self.SetPrefs(pyauto.kPromptForDownload, True))

  def testEnabledPlugins(self):
    """Verify that enabled plugins cannot be disabled."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search('Java', plugin['name']):
        self.assertRaises(pyauto.JSONInterfaceError,
                          lambda: self.DisablePlugin(plugin['path']))
        return
    logging.debug('Java is not present.')

  def testIncognitoEnabled(self):
    """Verify that incognito window can be launched."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEquals(2, self.GetBrowserWindowCount())

  def testDisableBrowsingHistory(self):
    """Verify that browsing history is not being saved."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'empty.html'))
    self.NavigateToURL(url)
    self.assertFalse(self.GetHistoryInfo().History(),
                     msg='History is being saved.')

  def testAlwaysAuthorizePlugins(self):
    """Verify plugins are always allowed to run when policy is set."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    url = self.GetFileURLForDataPath('plugin', 'java_new.html')
    self.NavigateToURL(url)
    self.assertFalse(self.WaitForInfobarCount(1))
    pid = self._GetPluginPID('Java')
    self.assertTrue(pid, 'No plugin process for java')

  def testDisablePopups(self):
    """Verify popups are not allowed if policy disables popups."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    url = self.GetFileURLForDataPath('popup_blocker', 'popup-window-open.html')
    self.NavigateToURL(url)
    self.assertEqual(1, len(self.GetBlockedPopupsInfo()),
                     msg='Popup not blocked')
    self.assertRaises(pyauto.JSONInterfaceError,
               lambda: self.SetPrefs(pyauto.kManagedDefaultPopupsSetting, 1))

  def testTranslateEnabled(self):
    """Verify that translate happens if policy enables it."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kEnableTranslate))
    url = self.GetFileURLForDataPath('translate', 'es', 'google.html')
    self.NavigateToURL(url)
    self.assertTrue(self.WaitForInfobarCount(1))
    translate_info = self.GetTranslateInfo()
    self.assertEqual('es', translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self._CheckIfPrefCanBeModified(pyauto.kEnableTranslate, True, False)
  
  def testEditBookmarksEnabled(self):
    """Verify that bookmarks can be edited if policy sets it."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kEditBookmarksEnabled, True, False)

  def testDefaultSearchProviderEnabled(self):
    """Verify a default search is performed when the user types text in the
       omnibox that is not a URL
    """
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return

    self._CheckIfPrefCanBeModified(pyauto.kDefaultSearchProviderEnabled, True,
                                   False)
    intranet_engine = [x for x in self.GetSearchEngineInfo()
                       if x['keyword'] == 'mis']
    self.assertTrue(intranet_engine)
    self.assertTrue(intranet_engine[0]['is_default'])
    self.SetOmniboxText('google chrome')
    self.WaitUntilOmniboxQueryDone()
    self.OmniboxAcceptInput()
    self.assertTrue(re.search('search.my.company',
                              self.GetActiveTabURL().spec()))

class EnterpriseTestReverse(pyauto.PyUITest):
  """Test for the Enterprise features that uses the opposite values of the
  policies used by above test class 'EnterpriseTest'.

  Browser preferences will be managed using policies. These managed preferences
  cannot be modified by user. This works only for Google Chrome, not Chromium.

  On Linux, assume that 'suid-python' (a python binary setuid root) is
  available on the machine under /usr/local/bin directory.
  """
  assert pyauto.PyUITest.IsWin() or pyauto.PyUITest.IsLinux(), \
         'Only runs on Win or Linux'

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump prefs... ')
      self.pprint(self.GetPrefsInfo().Prefs())

  @staticmethod
  def _Cleanup():
    """Removes the registry key and its subkeys(if they exist).

    Win: Registry Key being deleted: HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google
    Linux: Removes the chrome directory from /etc/opt
    """
    if pyauto.PyUITest.IsWin():
      if subprocess.call(
          r'reg query HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google') == 0:
        logging.debug(r'Removing HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google')
        subprocess.call(r'reg delete HKLM\Software\Policies\Google /f')
    elif pyauto.PyUITest.IsLinux():
      sudo_cmd_file = os.path.join(os.path.dirname(__file__),
                                   'enterprise_helper_linux.py')
      if os.path.isdir ('/etc/opt/chrome'):
        logging.debug('Removing directory /etc/opt/chrome/')
        subprocess.call(['suid-python', sudo_cmd_file,
                         'remove_dir', '/etc/opt/chrome'])

  @staticmethod
  def _SetUp():
    """Win: Add the registry keys from the .reg file.

    Removes the registry key and its subkeys if they exist.
    Adding HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google.

    Linux: Copy the chrome.json file to the managed directory.
    Remove /etc/opt/chrome directory if it exists.
    """
    EnterpriseTestReverse._Cleanup()
    if pyauto.PyUITest.IsWin():
      registry_location = os.path.join(EnterpriseTestReverse.DataDir(),
                                       'enterprise', 'chrome-add-reverse.reg')
      # Add the registry keys
      subprocess.call('reg import %s' % registry_location)
    elif pyauto.PyUITest.IsLinux():
      chrome_json = os.path.join(EnterpriseTestReverse.DataDir(),
                                       'enterprise', 'chrome-reverse.json')
      sudo_cmd_file = os.path.join(os.path.dirname(__file__),
                                   'enterprise_helper_linux.py')
      policies_location = '/etc/opt/chrome/policies/managed'
      subprocess.call(['suid-python',  sudo_cmd_file,
                       'setup_dir', policies_location])
      # Copy chrome.json file to the managed directory
      subprocess.call(['suid-python',  sudo_cmd_file,
                       'copy', chrome_json, policies_location])

  def setUp(self):
    # Add policies through registry or json file.
    self._SetUp()
    # Check if registries are created in Win.
    if pyauto.PyUITest.IsWin():
      registry_query_code = subprocess.call(
        r'reg query HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google')
      assert registry_query_code == 0, 'Could not create registries.'
    # Check if json file is copied under correct location in Linux.
    elif pyauto.PyUITest.IsLinux():
      policy_file_check = os.path.isfile(
                        '/etc/opt/chrome/policies/managed/chrome-reverse.json')
      assert policy_file_check, 'Policy file(s) not set up.'
    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    EnterpriseTestReverse._Cleanup()

  def _CheckIfPrefCanBeModified(self, key, defaultval, newval):
    """Check if the managed preferences can be modified.

    Args:
      key: The preference key that you want to modify
      defaultval: Default value of the preference that we are trying to modify
      newval: New value that we are trying to set
    """
    # Check if the default value of the preference is set as expected.
    self.assertEqual(self.GetPrefsInfo().Prefs(key), defaultval,
                     msg='Default value of the preference is wrong.')
    self.assertRaises(pyauto.JSONInterfaceError,
                      lambda: self.SetPrefs(key, newval))

  def _GetPluginPID(self, plugin_name):
    """Fetch the pid of the plugin process with name |plugin_name|."""
    child_processes = self.GetBrowserInfo()['child_processes']
    plugin_type = 'Plug-in'
    for x in child_processes:
      if x['type'] == plugin_type and re.search(plugin_name, x['name']):
        return x['pid']
    return None

  # Tests for options in Basics
  def testStartupPages(self):
    """Verify that user cannot modify the startup page options."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    # Verify startup option
    self.assertEquals(0, self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    self.assertRaises(pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kRestoreOnStartup, 1))

  def testHomePageOptions(self):
    """Verify that we cannot modify Homepage settings."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    # Try to configure home page URL
    self.assertEquals('http://chromium.org',
                      self.GetPrefsInfo().Prefs('homepage'))
    self.assertRaises(pyauto.JSONInterfaceError,
        lambda: self.SetPrefs('homepage', 'http://www.google.com'))

    # Try to reconfigure NTP as home page
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kHomePageIsNewTabPage))
    self.assertRaises(pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kHomePageIsNewTabPage, True))

  def testShowHomeButton(self):
    """Verify that home button option cannot be modified when it's managed."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kShowHomeButton, False, True)

  def testInstant(self):
    """Verify that Instant option cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kInstantEnabled, False, True)

  # Tests for options in Personal Stuff
  def testPasswordManagerEnabled(self):
    """Verify that password manager preference cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kPasswordManagerEnabled, False, True)
  
  def testPasswordManagerAllowShowPasswords(self):
    """Verify that password manager preference to show passwords 
    cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kPasswordManagerAllowShowPasswords,
                                   True, False)

  # Tests for options in Under the Hood
  def testPrivacyPrefs(self):
    """Verify that the managed preferences cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    prefs_list = [
        # (preference key, default value, new value)
        (pyauto.kAlternateErrorPagesEnabled, False, True),
        (pyauto.kNetworkPredictionEnabled, False, True),
        (pyauto.kSafeBrowsingEnabled, False, True),
        (pyauto.kSearchSuggestEnabled, False, True),
        ]
    # Check if the policies got applied by trying to modify
    for key, defaultval, newval in prefs_list:
      logging.info('Checking pref %s', key)
      self._CheckIfPrefCanBeModified(key, defaultval, newval)

  def testNotClearSiteDataOnExit(self):
    """Verify that clear data on exit preference cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kClearSiteDataOnExit, False, True)

  def testUnblockThirdPartyCookies(self):
    """Verify that block third party cookies preference cannot be modified."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kBlockThirdPartyCookies, False, True)

  def testEnableDevTools(self):
    """Verify that devtools window can be launched."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    # DevTools process can be seen by PyAuto only when it's undocked.
    self.SetPrefs(pyauto.kDevToolsOpenDocked, False)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEquals(2, len(self.GetBrowserInfo()['windows']),
                      msg='Devtools window not launched.')

  def testEnableSPDY(self):
    """Verify that SPDY is enabled."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self.NavigateToURL('chrome://net-internals/#spdy')
    self.assertEquals(0,
                      self.FindInPage('encrypted.google.com')['match_count'])
    self.AppendTab(pyauto.GURL('https://encrypted.google.com'))
    self.assertEquals('Google', self.GetActiveTabTitle())
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertEquals(1,
        self.FindInPage('encrypted.google.com', tab_index=0)['match_count'],
        msg='SPDY is not enabled.')

  def testSetDownloadDirectory(self):
    """Verify that the downloads directory and prompt for download preferences
    can be modified.
    """
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    if self.IsWin():
      download_default_dir = self.GetDownloadDirectory().value()
      self.assertEqual(download_default_dir,
        self.GetPrefsInfo().Prefs()['download']['default_directory'],
        msg='Downloads directory is not set correctly.')
      # Check for changing the download directory location
      self.SetPrefs(pyauto.kDownloadDefaultDirectory,
                                os.getenv('USERPROFILE'))
    elif self.IsLinux():
      download_default_dir = os.path.join(os.getenv('HOME'), 'Downloads')
      self.assertEqual(download_default_dir,
        self.GetPrefsInfo().Prefs()['download']['default_directory'],
        msg='Downloads directory is not set correctly.')
      self.SetPrefs(pyauto.kDownloadDefaultDirectory,
                                os.getenv('HOME'))
    # Check for changing the option 'Ask for each download'
    self.SetPrefs(pyauto.kPromptForDownload, False)

  def testIncognitoDisabled(self):
    """Verify that incognito window can be launched."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEquals(1, self.GetBrowserWindowCount())

  def testEnableBrowsingHistory(self):
    """Verify that browsing history is being saved."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'empty.html'))
    self.NavigateToURL(url)
    self.assertTrue(self.GetHistoryInfo().History(),
                     msg='History not is being saved.')

  def testAlwaysAuthorizePluginsDisabled(self):
    """Verify plugins are always not allowed to run when policy is set."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    url = self.GetFileURLForDataPath('plugin', 'java_new.html')
    self.NavigateToURL(url)
    self.assertTrue(self.WaitForInfobarCount(1))
    pid = self._GetPluginPID('Java')
    self.assertFalse(pid, 'There is a plugin process for java')

  def testEnablePopups(self):
    """Verify popups are allowed if policy enables popups."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    url = self.GetFileURLForDataPath('popup_blocker', 'popup-window-open.html')
    self.NavigateToURL(url)
    self.assertEqual(2, self.GetBrowserWindowCount(),
                     msg='Popup could not be launched');
    self.assertRaises(pyauto.JSONInterfaceError,
               lambda: self.SetPrefs(pyauto.kManagedDefaultPopupsSetting, 2))

  def testTranslateDisabled(self):
    """Verify that translate does not happen if policy disables it."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kEnableTranslate))
    url = self.GetFileURLForDataPath('translate', 'es', 'google.html')
    self.NavigateToURL(url)
    self.assertFalse(self.WaitForInfobarCount(1))
    self._CheckIfPrefCanBeModified(pyauto.kEnableTranslate, False, True)

  def testEditBookmarksDisabled(self):
    """Verify that bookmarks cannot be edited if policy sets it."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    self._CheckIfPrefCanBeModified(pyauto.kEditBookmarksEnabled, False, True)

  def testDefaultSearchProviderDisabled(self):
    """Verify that inputting text in omnibox does not trigger search when
    default search provider is disabled.
    """

    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return

    self._CheckIfPrefCanBeModified(pyauto.kDefaultSearchProviderEnabled, False,
                                   True)
    self.SetOmniboxText('deli')
    self.WaitUntilOmniboxQueryDone()
    self.assertRaises(pyauto.JSONInterfaceError,
                      lambda: self.OmniboxAcceptInput())


if __name__ == '__main__':
  pyauto_functional.Main()
