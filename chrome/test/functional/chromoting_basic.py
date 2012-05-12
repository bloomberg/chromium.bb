#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must come before chromoting and pyauto.
import chromoting
import pyauto


class ChromotingBasic(chromoting.ChromotingMixIn, pyauto.PyUITest):
  """Basic tests for Chromoting."""

  _EXTRA_CHROME_FLAGS = [
    '--allow-nacl-socket-api=*',
  ]

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with some custom flags.

    Overrides the default list of extra flags passed to Chrome.  See
    ExtraChromeFlags() in pyauto.py.
    """
    return pyauto.PyUITest.ExtraChromeFlags(self) + self._EXTRA_CHROME_FLAGS

  def setUp(self):
    """Set up test for Chromoting on both local and remote machines.

    Installs the Chromoting app, launches it, and authenticates
    using the default Chromoting test account.
    """
    super(ChromotingBasic, self).setUp()
    self._app = self.InstallExtension(self.GetWebappPath())
    self.LaunchApp(self._app)
    account = self.GetPrivateInfo()['test_chromoting_account']
    self.Authenticate(account['username'], account['password'])

  def testChromoting(self):
    """Verify that we can start and disconnect from a Chromoting session."""
    client_local = (self.remote == None)
    host = self
    client = self if client_local else self.remote
    client_tab_index = 2 if client_local else 1

    access_code = host.Share()
    self.assertTrue(access_code,
                    msg='Host attempted to share, but it failed. '
                        'No access code was found.')

    if client_local:
      client.LaunchApp(self._app)

    self.assertTrue(client.Connect(access_code, client_tab_index),
                    msg='The client attempted to connect to the host, '
                        'but the chromoting session did not start.')

    host.CancelShare()
    client.Disconnect(client_tab_index)


if __name__ == '__main__':
  pyauto_functional.Main()
