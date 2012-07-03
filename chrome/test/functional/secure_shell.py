#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os
import time

import pyauto_functional  # must be imported before pyauto
import pyauto


class SecureShellTest(pyauto.PyUITest):
  """Tests for Secure Shell app.

  Uses app from chrome/test/data/extensions/secure_shell/.
  The test uses stable app by default.
  Set the env var SECURE_SHELL_USE_DEV=1 to make it use the dev one.
  """

  assert pyauto.PyUITest.IsChromeOS(), 'Works on ChromeOS only'

  def setUp(self):
    """Install secure shell app at startup."""
    pyauto.PyUITest.setUp(self)

    # Pick app from data dir.
    app_dir = os.path.join(os.path.abspath(
        self.DataDir()), 'extensions', 'secure_shell')
    channel = 'dev' if os.getenv('SECURE_SHELL_USE_DEV') else 'stable'
    files = glob.glob(os.path.join(app_dir, 'SecureShell-%s-*.crx' % channel))
    assert files, 'Secure Shell %s app missing in %s' % (channel, app_dir)
    app_path = files[0]

    # Install app.
    logging.debug('Using Secure shell app %s' % app_path)
    self._app_id = self.InstallExtension(app_path, from_webstore=True)

  def testInstall(self):
    """Install Secure Shell."""
    # Installation already done in setUp. Just verify.
    self.assertTrue(self._app_id)
    ssh_info = [x for x in self.GetExtensionsInfo()
                if x['id'] == self._app_id][0]
    self.assertTrue(ssh_info)
    # Uninstall.
    self.UninstallExtensionById(id=self._app_id)
    self.assertFalse([x for x in self.GetExtensionsInfo()
                      if x['id'] == self._app_id],
        msg='Could not uninstall.')

  def testLaunch(self):
    """Launch Secure Shell and verify basic connect/exit flow.

    This basic flow also verifies that NaCl works since secure shell is based
    on it.
    """
    self.assertEqual(1, self.GetTabCount())
    then = time.time()
    self.LaunchApp(self._app_id)
    login_ui_frame = (
        '/descendant::iframe[contains(@src, "nassh_connect_dialog.html")]')
    # Wait for connection dialog iframe to load.
    self.WaitForDomNode(login_ui_frame, tab_index=1,
        msg='Secure shell login dialog did not show up')
    self.WaitForDomNode('id("field-description")', tab_index=1,
        attribute='placeholder',
        expected_value='username@hostname',  # partial match
        frame_xpath=login_ui_frame,
        msg='Did not find secure shell username dialog')
    now = time.time()
    self.assertEqual(2, self.GetTabCount(), msg='Did not launch')
    logging.info('Launched Secure Shell in %.2f secs' % (now - then))

    # Fill in chronos@localhost using webdriver.
    driver = self.NewWebDriver()
    driver.switch_to_window(driver.window_handles[-1])  # last tab
    driver.switch_to_frame(1)
    user = 'chronos@localhost'
    driver.find_element_by_id('field-description').send_keys(user + '\n')

    # Verify yes/no prompt
    self.WaitForHtermText('continue connecting \(yes/no\)\?', tab_index=1,
        msg='Did not get the yes/no prompt')
    welcome_text = self.GetHtermRowsText(0, 8, tab_index=1)
    self.assertTrue('Welcome to Secure Shell' in welcome_text,
        msg='Did not get correct welcome message')

    # Type 'yes' and enter password
    self.SendKeysToHterm('yes\\n', tab_index=1)
    self.WaitForHtermText('Password:', tab_index=1,
        msg='Did not get password prompt')
    self.SendKeysToHterm('test0000\\n', tab_index=1)
    self.WaitForHtermText('chronos@localhost $', tab_index=1,
        msg='Did not get shell login prompt')

    # Type 'exit' and close the tab
    self.SendKeysToHterm('exit\\n', tab_index=1)
    self.WaitForHtermText('completed with exit code 0', tab_index=1,
        msg='Did not get correct exit message')


if __name__ == '__main__':
  pyauto_functional.Main()
