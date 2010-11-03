#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


class SyncTest(pyauto.PyUITest):
  """Tests for sync."""

  def testSignInToSync(self):
    """Sign in to sync."""
    # Need to initialize username and password. See crbug.com/60970.
    username = '<username>@gmail.com'
    password = '<password>'
    self.assertTrue(self.GetSyncInfo()['summary'] == 'OFFLINE_UNUSABLE')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Never')
    self.assertTrue(self.SignInToSync(username, password))
    self.assertTrue(self.GetSyncInfo()['summary'] == 'READY')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')

  def testDisableAndEnableDatatype(self):
    """Sign in, disable and then enable sync for a datatype."""
    # Need to initialize username and password. See crbug.com/60970.
    username = '<username>@gmail.com'
    password = '<password>'
    self.assertTrue(self.SignInToSync(username, password))
    self.assertTrue(self.GetSyncInfo()['summary'] == 'READY')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')
    self.assertTrue(self.DisableSyncForDatatypes(['Bookmarks']))
    self.assertFalse('Bookmarks' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue(self.EnableSyncForDatatypes(['Bookmarks']))
    self.assertTrue('Bookmarks' in self.GetSyncInfo()['synced datatypes'])

  def testRestartBrowser(self):
    """Sign in to sync and restart the browser."""
    # Need to initialize username and password. See crbug.com/60970.
    username = '<username>@gmail.com'
    password = '<password>'
    self.assertTrue(self.SignInToSync(username, password))
    self.assertTrue(self.GetSyncInfo()['summary'] == 'READY')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(self.AwaitSyncCycleCompletion())
    self.assertTrue(self.GetSyncInfo()['summary'] == 'READY')
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')


if __name__ == '__main__':
  pyauto_functional.Main()
