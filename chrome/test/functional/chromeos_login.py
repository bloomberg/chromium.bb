#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_errors
import test_utils


sys.path.append('/usr/local')  # To make autotest libs importable.
from autotest.cros import cros_ui
from autotest.cros import cryptohome


class ChromeosLogin(pyauto.PyUITest):
  """TestCases for Logging into ChromeOS."""

  assert os.geteuid() == 0, 'Need to run this test as root'

  def ShouldAutoLogin(self):
    return False

  def setUp(self):
    # We want a clean session_manager instance for every run,
    # so restart ui now.
    cros_ui.stop(allow_fail=True)
    cryptohome.remove_all_vaults()
    cros_ui.start(wait_for_login_prompt=False)
    pyauto.PyUITest.setUp(self)

  def _ValidCredentials(self, account_type='test_google_account'):
    """Obtains a valid username and password from a data file.

    Returns:
      A dictionary with the keys 'username' and 'password'
    """
    return self.GetPrivateInfo()[account_type]

  def testExecuteJavascriptInOOBEWebUI(self):
    """Test that javascript can be executed at the login page."""
    msg = 'test success'
    ret = self.ExecuteJavascriptInOOBEWebUI(
              'window.domAutomationController.send("%s");' % msg)
    self.assertEqual(ret, msg)

  def testGoodLogin(self):
    """Test that login is successful with valid credentials."""
    credentials = self._ValidCredentials()
    self.Login(credentials['username'], credentials['password'])
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Login failed.')

  def testBadUsername(self):
    """Test that login fails when passed an invalid username."""
    self.assertRaises(
        pyauto_errors.JSONInterfaceError,
        lambda: self.Login('doesnotexist@fakedomain.org', 'badpassword'))
    login_info = self.GetLoginInfo()
    self.assertFalse(login_info['is_logged_in'],
                     msg='Login succeeded, with bad credentials.')

  def testBadPassword(self):
    """Test that login fails when passed an invalid password."""
    credentials = self._ValidCredentials()
    self.assertRaises(
        pyauto_errors.JSONInterfaceError,
        lambda: self.Login(credentials['username'], 'badpassword'))
    login_info = self.GetLoginInfo()
    self.assertFalse(login_info['is_logged_in'],
                     msg='Login succeeded, with bad credentials.')

  def testLoginAsGuest(self):
    """Test we can login with guest mode."""
    self.LoginAsGuest()
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Not logged in at all.')
    self.assertTrue(login_info['is_guest'], msg='Not logged in as guest.')

  def testLockScreenAfterLogin(self):
    """Test after logging in that the screen can be locked."""
    self.testGoodLogin()
    self.assertFalse(self.GetLoginInfo()['is_screen_locked'],
                     msg='Screen is locked, but the screen was not locked.')
    self.LockScreen()
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_screen_locked'], msg='The screen is not '
                    'locked after attempting to lock the screen.')

  def testLockAndUnlockScreenAfterLogin(self):
    """Test locking and unlocking the screen after logging in."""
    self.testLockScreenAfterLogin()
    self.UnlockScreen(self._ValidCredentials()['password'])
    login_info = self.GetLoginInfo()
    self.assertFalse(login_info['is_screen_locked'],
                     msg='Screen is locked, but it should have been unlocked.')

  def testLockAndUnlockScreenAfterLoginWithBadPassword(self):
    """Test locking and unlocking the screen with the wrong password."""
    self.testLockScreenAfterLogin()
    self.UnlockScreen('not_the_right_password')
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_screen_locked'],
                     msg='Screen is unlock, but it should have been unlocked '
                         'since we attempted to unlock with a bad password')

  def testLoginToCreateNewAccount(self):
    """Test we can login as a guest and create a new account."""
    self.ShowCreateAccountUI()
    # The login hook does not wait for the first tab to load, so we wait here.
    self.assertTrue(
      self.WaitUntil(self.GetActiveTabTitle, expect_retval='Google Accounts'),
                     msg='Could not verify that the Accounts tab was opened.')
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_guest'], msg='Not logged in as guest.')

  def testGoodLoginForTransitionedDomainAccount(self):
    """Test that login is successful with valid credentials for a domain.

    ChromeOS only allows GA+ accounts to login, there are also known as
    transitioned accounts.

    """
    credentials = self._ValidCredentials(account_type='test_domain_account')
    self.Login(credentials['username'], credentials['password'])
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Login failed.')

  def testCachedCredentials(self):
    """Test that we can login without connectivity if we have so before."""
    self.testGoodLogin()
    self.Logout()
    self.SetProxySettingsOnChromeOS('singlehttp', '10.10.10.10')
    self.testGoodLogin()
    # Reset back to direct proxy
    self.SetProxySettingsOnChromeOS('type', self.PROXY_TYPE_DIRECT)

  def testNavigateAfterLogin(self):
    """Test that page navigation is successful after logging in."""
    self.testGoodLogin()
    self.NavigateToURL("http://www.google.com")
    self.assertEqual(self.GetActiveTabTitle(), 'Google',
                     msg='Unable to navigate to Google and verify tab title.')

  def testSigningOutFromLockedScreen(self):
    """Test logout can be performed from the lock screen."""
    self.testLockScreenAfterLogin()
    self.SignoutInScreenLocker()
    self.assertFalse(self.GetLoginInfo()['is_logged_in'],
                     msg='Still logged in when we should be logged out.')

  def testLoginSequenceSanity(self):
    """Test that the interface can maintain a connection after multiple logins.

    This test is to verify the stability of the automation interface.

    """
    self.testGoodLogin()
    self.Logout()
    self.testBadPassword()
    self.testLoginAsGuest()
    self.Logout()
    self.testLoginToCreateNewAccount()

  def testLogoutWithNoWindows(self):
    """Verify logout when no browser windows are present."""
    self.testGoodLogin()
    for i in range(5):
      self.OpenNewBrowserWindow(True)
    for _ in range(self.GetBrowserWindowCount()):
      self.CloseBrowserWindow(0)
    self.assertEqual(0, self.GetBrowserWindowCount(),
                     msg='Could not close all browser windows')
    self.Logout()
    self.testGoodLogin()

  def testInitialLoginState(self):
    """Verify basic state of browser windows at initial login."""
    self.testGoodLogin()
    # Should have 1 browser window with 1 tab.
    info = self.GetBrowserInfo()
    self.assertEqual(1, len(info['windows']))
    self.assertFalse(info['windows'][0]['incognito'],
        msg='Did not expect incognito window after login')
    self.assertEqual(1, len(info['windows'][0]['tabs']))

    self.OpenNewBrowserWindow(True)
    # Should have 2 regular browser windows.
    info = self.GetBrowserInfo()
    self.assertEqual(2, len(info['windows']))
    self.assertFalse(info['windows'][0]['incognito'])
    self.assertFalse(info['windows'][1]['incognito'],
        msg='Expected a regular new window.')

  def testProfilePreservedBetweenLogins(self):
    """Verify that profile is preserved between two login sessions.

    Also verify Local State.
    """
    self.testGoodLogin()

    # Build up some history and setup state in "Local State".
    url = self.GetHttpURLForDataPath('title2.html')
    self.NavigateToURL(url)
    # chromeos often takes a while to register URLs into history.
    self.assertTrue(self.WaitUntil(lambda: self.GetHistoryInfo().History()),
                    msg='Could not open %s successfully' % url)
    open('/home/chronos/__magic__', 'w').close()
    open('/home/chronos/user/__magic__', 'w').close()

    def _VerifyProfile():
      history = self.GetHistoryInfo().History()
      self.assertEqual(1, len(history))
      self.assertEqual(url, history[0]['url'])
      self.assertTrue(os.path.exists('/home/chronos/__magic__'),
          msg='/home/chronos/__magic__ did not persist across login sessions')
      self.assertTrue(os.path.exists('/home/chronos/user/__magic__'),
          msg='/home/chronos/user/__magic__ did not persist across '
              'login sessions')

    _VerifyProfile()
    self.Logout()
    self.testGoodLogin()  # Re-login with same account.
    _VerifyProfile()

  def testGuestCrosh(self):
    """Verify we can use crosh in guest mode."""
    self.LoginAsGuest()
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Not logged in at all.')
    self.assertTrue(login_info['is_guest'], msg='Not logged in as guest.')
    for _ in range(self.GetBrowserWindowCount()):
      self.CloseBrowserWindow(0)
    test_utils.OpenCroshVerification(self)

    # Verify crosh prompt.
    self.WaitForHtermText(text='crosh> ',
        msg='Could not find "crosh> " prompt')
    self.assertTrue(
        self.GetHtermRowsText(start=0, end=2).endswith('crosh> '),
        msg='Could not find "crosh> " prompt')

    # Run a crosh command.
    self.SendKeysToHterm('help\\n')
    self.WaitForHtermText(text='help_advanced',
        msg='Could not find "help_advanced" in help output.')

    # Exit crosh and close tab.
    self.SendKeysToHterm('exit\\n')
    self.WaitForHtermText(text='command crosh completed with exit code 0',
        msg='Could not exit crosh.')

  def testCroshPreservedBetweenLogins(self):
    """Verify user can continue after re-login."""
    self.testGoodLogin()
    self.CloseBrowserWindow(0)
    test_utils.OpenCroshVerification(self)

    # Verify crosh prompt.
    self.WaitForHtermText(text='crosh> ',
        msg='Could not find "crosh> " prompt')
    self.assertTrue(
        self.GetHtermRowsText(start=0, end=2).endswith('crosh> '),
        msg='Could not find "crosh> " prompt')

    # Open 2 other tabs.
    self.AppendTab(self.GetHttpURLForDataPath('title2.html'))
    self.assertEqual('Title Of Awesomeness', self.GetActiveTabTitle(),
                     msg='Unable to naviage to title2.html and '
                         'verify tab title.')
    self.AppendTab(self.GetHttpURLForDataPath('settings', 'image_page.html'))
    self.assertEqual('Show an image', self.GetActiveTabTitle(),
                     msg='Unable to navigate to image_page and '
                         'verify tab title.')
    self.Logout()
    self.testGoodLogin()  # Re-Login with same account.

    # Verify 3 tabs are still open after re-login.
    self.assertEqual(3, len(self.GetBrowserInfo()['windows'][0]['tabs']))


if __name__ == '__main__':
  pyauto_functional.Main()
