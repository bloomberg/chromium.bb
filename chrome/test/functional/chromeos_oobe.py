#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

import pyauto_functional  # Must be imported before pyauto
import pyauto


sys.path.append('/usr/local')  # To make autotest libs importable.
from autotest.cros import cros_ui
from autotest.cros import cryptohome


class ChromeosOOBE(pyauto.PyUITest):
  """TestCases for ChromeOS OOBE wizard flow."""

  assert os.geteuid() == 0, 'Need to run this test as root'

  def ShouldOOBESkipToLogin(self):
    """Do not skip OOBE."""
    return False

  def setUp(self):
    # We want a clean session_manager instance for every run,
    # so restart ui now.
    cros_ui.stop(allow_fail=True)
    cryptohome.remove_all_vaults()
    cros_ui.start(wait_for_login_prompt=False)
    pyauto.PyUITest.setUp(self)

  def _AssertCurrentScreen(self, screen_name):
    """Verifies current OOBE screen.

    Args:
      screen_name: expected current screen name.
    """
    self.assertEqual(screen_name, self.GetOOBEScreenInfo()['screen_name'])

  def testBasicFlow(self):
    """Test that basic OOBE flow works."""
    self._AssertCurrentScreen('network')
    # Network -> EULA (on Google Chrome builds, Update on Chromium).
    ret = self.AcceptOOBENetworkScreen()
    if self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome':
      self.assertEquals('eula', ret['next_screen'])
      self._AssertCurrentScreen('eula')
      # EULA (accepted) -> Update.
      ret = self.AcceptOOBEEula(accepted=True)
    # Update may have already been completed, so don't check for it.
    # Update (canceled) -> Login.
    ret = self.CancelOOBEUpdate()
    self.assertEquals('login', ret['next_screen'])
    self._AssertCurrentScreen('login')
    # Login -> User picker.
    credentials = self.GetPrivateInfo()['test_google_account']
    self.Login(credentials['username'], credentials['password'])
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Login after OOBE failed.')
    # User Picker -> normal browser session.
    ret = self.PickUserImage(3)
    self.assertEquals('session', ret['next_screen'])
    # Should have 1 browser windows ("Getting started").
    self.assertEqual(1, len(self.GetBrowserInfo()['windows']))
    # Verify user image selection.
    self.assertEqual(3, self.GetLoginInfo()['user_image'])


if __name__ == '__main__':
  pyauto_functional.Main()
