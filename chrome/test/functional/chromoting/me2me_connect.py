#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromoting me2me connect/disconnect related test cases."""

import chromoting_base
import pyauto


class Me2MeConnect(chromoting_base.ChromotingBase):
  """Drives me2me connect test cases."""

  def setUp(self):
    """Set up for me2me connect test."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    pyauto.PyUITest.setUp(self)

    self.InstallHostDaemon()
    webapp = self.InstallExtension(self.GetWebappPath())
    self.host.LaunchApp(webapp)
    self.host.Authenticate()
    self.host.StartMe2Me()
    self.host.CleanupHostList()
    self.host.EnableConnectionsInstalled()
    self.client.LaunchApp(webapp)

  def tearDown(self):
    """Mainly uninstalls the host daemon."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    self.host.DisableConnections()
    self.UninstallHostDaemon()

    pyauto.PyUITest.tearDown(self)


  def testMe2MeConnectDisconnectReconnectDisconnect(self):
    """Connects, disconnects, reconnects and disconnects"""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    self.client.ConnectMe2Me('111111', 'IN_SESSION',
                             self.client_tab_index)
    self.client.DisconnectMe2Me(False, self.client_tab_index)
    self.client.ReconnectMe2Me('111111', self.client_tab_index)
    self.client.DisconnectMe2Me(True, self.client_tab_index)

  def testMe2MeConnectWithWrongPin(self):
    """Connects and disconnects."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    self.client.ConnectMe2Me('222222', 'CLIENT_CONNECT_FAILED_ME2ME',
                             self.client_tab_index)
    self.client.ReconnectMe2Me('111111', self.client_tab_index)
    self.client.DisconnectMe2Me(True, self.client_tab_index)

  def testMe2MeChangePin(self):
    """Changes pin, connects with new pin and then disconnects."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    self.host.ChangePin('222222')
    self.client.ConnectMe2Me('222222', 'IN_SESSION',
                             self.client_tab_index)
    self.client.DisconnectMe2Me(True, self.client_tab_index)

  def testMe2MeChangeName(self):
    """Changes host name, connects and then disconnects."""
    # Disable test on vista and xp until the failure is figured
    if self.IsWinVista() or self.IsWinXP():
      return

    self.client.ChangeName("Changed")
    self.client.ConnectMe2Me('111111', 'IN_SESSION',
                             self.client_tab_index)
    self.client.DisconnectMe2Me(True, self.client_tab_index)


if __name__ == '__main__':
  chromoting_base.Main()
