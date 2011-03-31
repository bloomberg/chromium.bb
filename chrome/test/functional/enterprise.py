#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess


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
        (pyauto.kDnsPrefetchingEnabled, True, False),
        (pyauto.kSafeBrowsingEnabled, True, False),
        (pyauto.kSearchSuggestEnabled, True, False),
        ]
    # Check if the policies got applied by trying to modify
    for key, defaultval, newval in prefs_list:
      logging.info('Checking pref %s', key)
      self._CheckIfPrefCanBeModified(key, defaultval, newval)

  # Tests for general options
  def testDisableDevTools(self):
    """Verify that we cannot launch dev tools."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    # DevTools process can be seen by PyAuto only when it's undocked.
    self.SetPrefs(pyauto.kDevToolsOpenDocked, False)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEquals(1, len(self.GetBrowserInfo()['windows']),
                      msg='Devtools window launched.')

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
