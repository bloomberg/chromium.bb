#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ChromeosVolume(pyauto.PyUITest):
  """Test case for volume levels.
     
  Test volume and mute changes with different state like, login,
  lock, logout, etc...
  """

  def setUp(self):
    # We want a clean session_manager instance for every run,
    # so restart session_manager now.
    assert self.WaitForSessionManagerRestart(
        lambda: subprocess.call(['pkill', 'session_manager'])), \
        'Timed out waiting for session_manager to start.'
    pyauto.PyUITest.setUp(self)
    self._initial_volume_info = self.GetVolumeInfo()

  def tearDown(self):
    self.SetVolume(self._initial_volume_info['volume'])
    self.SetMute(self._initial_volume_info['is_mute'])
    pyauto.PyUITest.tearDown(self)

  def _Login(self):
    """Perform login"""
    credentials = self.GetPrivateInfo()['test_google_account']
    self.Login(credentials['username'], credentials['password'])
    logging.info('Logged in as %s' % credentials['username'])   
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Login failed.')

  def testLoginLogoutVolume(self):
    """Test that volume settings are preserved after login and logout"""
    before_login = self.GetVolumeInfo()
    self._Login()
    after_login = self.GetVolumeInfo() 
    self.assertEqual(before_login, after_login,
        msg='Before login : %s and after login : %s, volume states are not '\
        'matching' % (before_login, after_login))
    self.Logout()
    after_logout = self.GetVolumeInfo() 
    self.assertEqual(after_login, after_logout,
        msg='Before logout : %s and after logout : %s, volume states are not '\
        'matching' % (after_login, after_logout))
    # For successive tests
    self._Login()

  def testLoginLockoutVolume(self):
    """Test that volume changes on the lock screen, are preserved"""
    lock_volume = {u'volume': 50.000000000000014, u'is_mute': True}
    self._Login()
    login_vol = self.GetVolumeInfo()
    self.LockScreen()
    self.SetVolume(lock_volume['volume'])
    self.SetMute(lock_volume['is_mute'])
    self.UnlockScreen(self.GetPrivateInfo()['test_google_account']['password'])
    after_login = self.GetVolumeInfo() 
    self.assertEqual(lock_volume, after_login, 
        msg='Locking screen volume changes are not preserved')


if __name__ == '__main__':
  pyauto_functional.Main()
