#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys

import pyauto_functional
import pyauto

sys.path.append('/usr/local')  # To make autotest libs importable.
from autotest.cros import cros_ui
from autotest.cros import cryptohome


class AccessibilityTest(pyauto.PyUITest):
  """Tests for Accessibility.

  Test various chromeos functionalities while Accessibility is turned on. 
  """
  find_test_data_dir = 'find_in_page'

  def setUp(self):
    # We want a clean session_manager instance for every run,
    # so restart ui now.
    cros_ui.stop(allow_fail=True)
    cryptohome.remove_all_vaults()
    cros_ui.start(wait_for_login_prompt=False)
    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    self._DisableSpokenFeedback()
    pyauto.PyUITest.tearDown(self)

  def _Login(self):
    """Perform login."""
    credentials = self.GetPrivateInfo()['test_google_account']
    self.Login(credentials['username'], credentials['password'])
    logging.info('Logging in as %s' % credentials['username'])
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Login failed.')

  def _LoginWithSpokenFeedback(self):
    self.EnableSpokenFeedback(True)
    self._Login()
    self.assertTrue(self.IsSpokenFeedbackEnabled(),
                    msg='Could not enable spoken feedback accessibility mode.')

  def _EnableSpokenFeedback(self):
    self.EnableSpokenFeedback(True)
    self.assertTrue(self.IsSpokenFeedbackEnabled(),
                    msg='Could not enable spoken feedback accessibility mode.')

  def _DisableSpokenFeedback(self):
    self.EnableSpokenFeedback(False)
    self.assertFalse(self.IsSpokenFeedbackEnabled(),
                    msg='Could not disable spoken feedback accessibility mode.')

  def testCanEnableSpokenFeedback(self):
    """Tests that spoken feedback accessibility mode can be enabled."""
    self._EnableSpokenFeedback()

  def testLoginAsGuest(self):
    """Test that Guest user login is possible when Accessibility is on."""
    self._EnableSpokenFeedback()
    self.LoginAsGuest()
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Not logged in at all.')
    self.assertTrue(login_info['is_guest'], msg='Not logged in as guest.')
    url = self.GetFileURLForDataPath('title1.html')
    self.NavigateToURL(url)
    self.assertEqual(1, self.FindInPage('title')['match_count'],
        msg='Failed to load the page or find the page contents.')

  def testAccessibilityBeforeLogin(self):
    """Test Accessibility before login."""
    self._LoginWithSpokenFeedback()
    self.Logout()
    self.assertFalse(self.GetLoginInfo()['is_logged_in'],
                     msg='Still logged in when we should be logged out.')
    self.assertTrue(self.IsSpokenFeedbackEnabled(),
        msg='Spoken feedback accessibility mode disabled after loggin out.')

  def testAccessibilityAfterLogin(self):
    """Test Accessibility after login."""
    self._Login()
    self._EnableSpokenFeedback()

  def testPagePerformance(self):
    """Test Chrome works fine when Accessibility is on."""
    self._LoginWithSpokenFeedback()
    # Verify that opened tab page behaves normally when Spoken Feedback is
    # enabled. crosbug.com/26731
    url = self.GetFileURLForDataPath(self.find_test_data_dir, 'largepage.html')
    self.NavigateToURL(url)
    self.assertEqual(373, self.FindInPage('daughter of Prince')['match_count'],
        msg='Failed to load the page or find the page contents.')


if __name__ == '__main__':
  pyauto_functional.Main()
