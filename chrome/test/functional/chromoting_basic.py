#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must come before chromoting and pyauto.
import chromoting
import pyauto


class ChromotingBasic(chromoting.ChromotingMixIn, pyauto.PyUITest):
  """Basic tests for Chromoting."""

  def setUp(self):
    """Set up test for Chromoting on both local and remote machines.

    Installs the Chromoting app, launches it, and authenticates
    using the default Chromoting test account.
    """
    super(ChromotingBasic, self).setUp()
    app = self.InstallExtension(self.GetIT2MeAppPath())
    self.LaunchApp(app)
    account = self.GetPrivateInfo()['test_chromoting_account']
    self.Authenticate(account['username'], account['password'])

  def testChromoting(self):
    """Verify that we can start and disconnect from a Chromoting session."""
    host = self
    client = self.remote

    host.SetHostMode()
    access_code = host.Share()
    self.assertTrue(access_code,
                    msg='Host attempted to share, but it failed. '
                        'No access code was found.')

    client.SetClientMode()
    self.assertTrue(client.Connect(access_code),
                    msg='The client attempted to connect to the host, '
                        'but the chromoting session did not start.')

    host.CancelShare()
    client.Disconnect()


if __name__ == '__main__':
  pyauto_functional.Main()
