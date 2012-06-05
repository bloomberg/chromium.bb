#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # must be imported before pyauto
import pyauto


class ChromeosHTMLTerminalTest(pyauto.PyUITest):
  """Basic tests for ChromeOS HTML Terminal.

  Requires ChromeOS to be logged in.
  """

  def _GetExtensionInfoById(self, extensions, id):
    for x in extensions:
      if x['id'] == id:
        return x
    return None

  def testInstallAndUninstallSecureShellExt(self):
    """Basic installation test for HTML Terminal on ChromeOS."""
    crx_file_path = os.path.abspath(
        os.path.join(self.DataDir(), 'pyauto_private', 'apps',
                     'SecureShell-dev-0.7.9.3.crx'))
    ext_id = self.InstallExtension(crx_file_path)
    self.assertTrue(ext_id, 'Failed to install extension.')
    extension = self._GetExtensionInfoById(self.GetExtensionsInfo(), ext_id)
    self.assertTrue(extension['is_enabled'],
                    msg='Extension was not enabled on installation.')
    self.assertFalse(extension['allowed_in_incognito'],
                     msg='Extension was allowed in incognito on installation.')
    # Uninstall HTML Terminal extension
    self.assertTrue(self.UninstallExtensionById(ext_id),
                    msg='Failed to uninstall extension.')


class CroshTest(pyauto.PyUITest):
  """Tests for crosh."""

  def setUp(self):
    """Close all windows at startup."""
    pyauto.PyUITest.setUp(self)
    for _ in range(self.GetBrowserWindowCount()):
      self.CloseBrowserWindow(0)

  def testBasic(self):
    """Verify crosh basic flow."""
    self.assertEqual(0, self.GetBrowserWindowCount())
    self.OpenCrosh()
    self.assertEqual(1, self.GetBrowserWindowCount())
    self.assertEqual(1, self.GetTabCount(),
        msg='Could not open crosh')
    self.assertEqual('crosh', self.GetActiveTabTitle())

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


if __name__ == '__main__':
  pyauto_functional.Main()
