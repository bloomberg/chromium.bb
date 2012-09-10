#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromoting me2me enable/disable related test cases."""

import chromoting_base
import pyauto


class Me2MeEnable(chromoting_base.ChromotingBase):
  """Drives the me2me enable test cases."""

  def setUp(self):
    """Set up for me2me enable test."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    pyauto.PyUITest.setUp(self)

    self.InstallHostDaemon()
    webapp = self.InstallExtension(self.GetWebappPath())
    self.host.LaunchApp(webapp)
    self.host.Authenticate()
    self.host.StartMe2Me()

  def tearDown(self):
    """Mainly uninstalls the host daemon."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    self.UninstallHostDaemon()

    pyauto.PyUITest.tearDown(self)

  def testMe2MeEnableDisable(self):
    """Enables/disables remote connections.

    This test also exercises different pin conditions.
    """
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    self.host.EnableConnectionsInstalled(True)
    self.host.DisableConnections()


if __name__ == '__main__':
  chromoting_base.Main()
