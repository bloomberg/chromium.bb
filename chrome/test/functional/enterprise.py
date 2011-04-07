#!/usr/bin/python
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
  """
  assert pyauto.PyUITest.IsWin(), 'Only runs on Win'

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Interact with the browser and hit <enter> to dump prefs... ')
      pp.pprint(self.GetPrefsInfo().Prefs())

  @staticmethod
  def _CleanupRegistry():
    """Removes the registry key and its subkeys(if they exist).
    Registry Key being deleted: HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google
    """
    if subprocess.call(
        r'reg query HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google') == 0:
      logging.debug(r'Removing HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google')
      subprocess.call(r'reg delete HKLM\Software\Policies\Google /f')

  @staticmethod
  def _SetUpRegistry():
    """Add the registry keys from the .reg file.
    Removes the registry key and its subkeys if they exist.
    Adding HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google."""
    EnterpriseTest._CleanupRegistry()
    registry_location = os.path.join(EnterpriseTest.DataDir(), 'enterprise',
                                     'chrome-add.reg')
    # Add the registry keys
    subprocess.call('reg import %s' % registry_location)

  def setUp(self):
     # Add policies through registry.
     self._SetUpRegistry()
     # Check if registries are created.
     registry_query_code = subprocess.call(
        r'reg query HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Google')
     assert registry_query_code == 0, 'Could not create registries.'
     pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    EnterpriseTest._CleanupRegistry()

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
    self.assertTrue(re.search('hi',
                    self.GetPrefsInfo().Prefs()['intl']['accept_languages']),
                    msg='Chrome locale is not Hindi.')
    # TODO(sunandt): Try changing the application locale to another language.

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
    self.NavigateToURL('chrome://net-internals/#spdy')
    self.assertEquals(0,
                      self.FindInPage('encrypted.google.com')['match_count'])
    self.AppendTab(pyauto.GURL('https://encrypted.google.com'))
    self.assertEquals('Google', self.GetActiveTabTitle())
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertEquals(0,
        self.FindInPage('encrypted.google.com', tab_index=0)['match_count'])

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
    if self.IsWin():
      self.assertEqual('Downloads',
        self.GetPrefsInfo().Prefs()['download']['default_directory'],
        msg='Downloads directory is not set correctly.')
      # Check for changing the download directory location
      self.assertRaises(pyauto.JSONInterfaceError,
          lambda: self.SetPrefs(pyauto.kDownloadDefaultDirectory,
                                os.getenv('USERPROFILE')))
      # Check for changing the option 'Ask for each download'
      self.assertRaises(pyauto.JSONInterfaceError,
                        lambda: self.SetPrefs(pyauto.kPromptForDownload, True))
    elif self.IsLinux():
      # TODO(sunandt)
      pass

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


if __name__ == '__main__':
  pyauto_functional.Main()
