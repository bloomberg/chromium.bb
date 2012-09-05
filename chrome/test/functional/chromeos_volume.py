#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys

import pyauto_functional  # Must be imported before pyauto
import pyauto

sys.path.append('/usr/local')  # To make autotest libs importable.
from autotest.cros import cros_ui
from autotest.cros import cryptohome


class ChromeosVolume(pyauto.PyUITest):
  """Test case for volume levels.

  Test volume and mute changes with different state like, login,
  lock, logout, etc...
  """

  def setUp(self):
    # We want a clean session_manager instance for every run,
    # so restart ui now.
    cros_ui.stop(allow_fail=True)
    cryptohome.remove_all_vaults()
    cros_ui.start(wait_for_login_prompt=False)
    pyauto.PyUITest.setUp(self)
    self._initial_volume_info = self.GetVolumeInfo()

  def tearDown(self):
    self.SetVolume(self._initial_volume_info['volume'])
    self.SetMute(self._initial_volume_info['is_mute'])
    pyauto.PyUITest.tearDown(self)

  def ShouldAutoLogin(self):
    return False

  def _Login(self):
    """Perform login"""
    credentials = self.GetPrivateInfo()['test_google_account']
    self.Login(credentials['username'], credentials['password'])
    logging.info('Logged in as %s' % credentials['username'])
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Login failed.')

  def testDefaultVolume(self):
    """Test the default volume settings"""
    self._Login()
    board_name = self.ChromeOSBoard()
    default_volume = self.GetPrivateInfo()['default_volume']
    assert default_volume.get(board_name), \
           'No volume settings available for %s.' % board_name
    expected = {u'volume': default_volume[board_name],
                u'is_mute': default_volume['is_mute']}
    volume = self.GetVolumeInfo()
    self.assertEqual(volume.get('is_mute'), expected.get('is_mute'))
    self.assertAlmostEqual(volume.get('volume'), expected.get('volume'),
        msg='Volume settings are set to %s, not matching with default '
            'volume settings %s.' % (volume, expected))

  def testLoginLogoutVolume(self):
    """Test that volume settings are preserved after login and logout"""
    before_login = self.GetVolumeInfo()
    self._Login()
    after_login = self.GetVolumeInfo()
    self.assertEqual(before_login, after_login,
        msg='Before login : %s and after login : %s, volume states are not '
            'matching' % (before_login, after_login))
    self.Logout()
    after_logout = self.GetVolumeInfo()
    self.assertEqual(after_login, after_logout,
        msg='Before logout : %s and after logout : %s, volume states are not '
            'matching' % (after_login, after_logout))

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
