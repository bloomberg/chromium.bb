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


if __name__ == '__main__':
  pyauto_functional.Main()
