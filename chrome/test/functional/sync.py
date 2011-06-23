#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


class SyncTest(pyauto.PyUITest):
  """Tests for sync."""

  def testSignInToSync(self):
    """Sign in to sync."""
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    new_timeout = pyauto.PyUITest.ActionTimeoutChanger(self,
                                                       60 * 1000)  # 1 min.
    self.assertTrue(self.GetSyncInfo()['summary'] == 'OFFLINE_UNUSABLE')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Never')
    self.assertTrue(self.SignInToSync(username, password))
    self.assertTrue(self.GetSyncInfo()['summary'] == 'READY')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')

  def testDisableAndEnableDatatypes(self):
    """Sign in, disable and then enable sync for multiple sync datatypes."""
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    new_timeout = pyauto.PyUITest.ActionTimeoutChanger(self,
                                                       2 * 60 * 1000)  # 2 min.
    self.assertTrue(self.SignInToSync(username, password))
    self.assertTrue(self.GetSyncInfo()['summary'] == 'READY')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')
    self.assertTrue(self.DisableSyncForDatatypes(['Apps', 'Autofill',
        'Bookmarks', 'Extensions', 'Preferences', 'Themes']))
    self.assertFalse('Apps' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Autofill' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Bookmarks' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Extensions' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Preferences' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Themes' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue(self.EnableSyncForDatatypes(['Apps', 'Autofill',
        'Bookmarks', 'Extensions', 'Preferences','Themes']))
    self.assertTrue(self.DisableSyncForDatatypes(['Passwords']))
    self.assertTrue('Apps' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Autofill' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Bookmarks' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Extensions' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Preferences' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Themes' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Passwords' in self.GetSyncInfo()['synced datatypes'])

  def testRestartBrowser(self):
    """Sign in to sync and restart the browser."""
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    new_timeout = pyauto.PyUITest.ActionTimeoutChanger(self,
                                                       2 * 60 * 1000)  # 2 min.
    self.assertTrue(self.SignInToSync(username, password))
    self.assertTrue(self.GetSyncInfo()['summary'] == 'READY')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(self.AwaitSyncRestart())
    self.assertTrue(self.GetSyncInfo()['summary'] == 'READY')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')
    self.assertTrue(self.GetSyncInfo()['updates received'] == 0)

  def testPersonalStuffSyncSection(self):
    """Verify the Sync section in Preferences before and after sync."""
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    default_text = 'Keep everything synced or choose what data to sync'
    set_up_button = 'Set Up Sync'
    customize_button = 'Customize'
    stop_button = 'Stop Sync'
    signed_in_text = 'Google Dashboard'
    chrome_personal_stuff_url = 'chrome://settings/personal'
    new_timeout = pyauto.PyUITest.ActionTimeoutChanger(self,
                                                       2 * 60 * 1000)  # 2 min.
    self.AppendTab(pyauto.GURL(chrome_personal_stuff_url))
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(default_text, tab_index=1)['match_count'],
                expect_retval=1),
        'No default sync text.')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(set_up_button, tab_index=1)['match_count'],
                expect_retval=1),
        'No set up sync button.')

    self.assertTrue(self.SignInToSync(username, password))
    self.GetBrowserWindow(0).GetTab(1).Reload()
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(username, tab_index=1)['match_count'],
                expect_retval=1),
        'No sync user account information.')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(signed_in_text, tab_index=1)['match_count'],
                expect_retval=1),
        'No Google Dashboard information after signing in.')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(stop_button, tab_index=1)['match_count'],
                expect_retval=1),
        'No stop sync button.')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(customize_button, tab_index=1)['match_count'],
                expect_retval=1),
        'No customize sync button.')


if __name__ == '__main__':
  pyauto_functional.Main()
