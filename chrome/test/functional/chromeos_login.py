#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class LoginTest(pyauto.PyUITest):
  """TestCases for Logging into ChromeOS."""

  assert os.geteuid() == 0, 'Need to run this test as root'

  def tearDown(self):
    # All test will start with logging in, we need to reset to being logged out
    if self.GetLoginInfo()['is_logged_in']:
      self.Logout()
    pyauto.PyUITest.tearDown(self)

  def _ValidCredentials(self):
    """Obtains a valid username and password from a data file.

    Returns:
      A dictionary with the keys 'username' and 'password'
    """
    credentials_file = os.path.join(pyauto.PyUITest.DataDir(),
                                   'pyauto_private', 'private_tests_info.txt')
    assert os.path.exists(credentials_file), 'Credentials file does not exist.'
    return pyauto.PyUITest.EvalDataFrom(credentials_file)['test_google_account']

  def testGoodLogin(self):
    """Test that login is successful with valid credentials."""
    credentials = self._ValidCredentials()
    username = credentials['username']
    passwd = credentials['password']
    self.Login(username, passwd)
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], 'Login failed.')

  def testBadUsername(self):
    """Test that login fails when passed an invalid username."""
    username = 'doesnotexist@fakedomain.org'
    passwd = 'badpassword'
    self.Login(username, passwd)
    login_info = self.GetLoginInfo()
    self.assertFalse(login_info['is_logged_in'], 'Login succeeded, with bad '
                    'credentials.')

  def testBadPassword(self):
    """Test that login fails when passed an invalid password."""
    credentials = self._ValidCredentials()
    username = credentials['username']
    passwd = 'badpassword'
    self.Login(username, passwd)
    login_info = self.GetLoginInfo()
    self.assertFalse(login_info['is_logged_in'], 'Login succeeded, with bad '
                    'credentials.')

  def testLoginAsGuest(self):
    """Test we can login with guest mode."""
    self.LoginAsGuest()
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_guest'], 'Not logged in as guest.')

  def testLockScreenAfterLogin(self):
    """Test after logging in that the screen can be locked."""
    credentials = self._ValidCredentials()
    username = credentials['username']
    passwd = credentials['password']
    self.Login(username, passwd)
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], 'Login failed.')
    self.assertFalse(login_info['is_screen_locked'], 'Screen is locked, but the'
                     ' screen was not locked.')
    self.LockScreen()
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_screen_locked'], 'The screen is not locked '
                    'after attempting to lock the screen.')

  def testLockAndUnlockScreenAfterLogin(self):
    """Test locking and unlocking the screen after logging in."""
    self.testLockScreenAfterLogin()
    self.UnlockScreen()
    login_info = self.GetLoginInfo()
    self.assertFalse(login_info['is_screen_locked'], 'Screen is locked, but it '
                     'should have been unlocked.')

  ## TODO(krisr) add test case to log out after logging in.
  ## TODO(krisr) add test cases to login, add bad proxy settings, logout and
  ##             log back in testing cached credentials
  ## TODO(krisr) see if we can change the password of an account using GData
  ##             APIs to test password change on login
  ## TODO(krisr) add test for google apps domains


if __name__ == '__main__':
  pyauto_functional.Main()
