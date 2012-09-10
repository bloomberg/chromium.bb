#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic tests for Chromoting it2me."""

import chromoting_base
import pyauto


class IT2MeBasic(chromoting_base.ChromotingBase):
  """Drives it2me basic test cases."""

  def setUp(self):
    """Set up for it2me basic test."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    pyauto.PyUITest.setUp(self)

    webapp = self.InstallExtension(self.GetWebappPath())
    self.LaunchApp(webapp)
    self.Authenticate()

    if self.client_local:
      self.client.LaunchApp(webapp)

  def testIT2MeBasic(self):
    """Verify that we can start and disconnect a Chromoting it2me session."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    access_code = self.host.Share()
    self.assertTrue(access_code,
                    msg='Host attempted to share, but it failed. '
                        'No access code was found.')

    self.client.Connect(access_code, self.client_tab_index)

    self.host.CancelShare()
    self.client.Disconnect(self.client_tab_index)


if __name__ == '__main__':
  chromoting_base.Main()
