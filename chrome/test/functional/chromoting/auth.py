#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromoting authentication related test cases."""

import chromoting_base
import pyauto


class ChromotingAuth(chromoting_base.ChromotingBase):
  """Chromoting authentication related test cases."""

  def setUp(self):
    """Set up for auth test."""
    pyauto.PyUITest.setUp(self)

    webapp = self.InstallExtension(self.GetWebappPath())
    self.host.LaunchApp(webapp)
    self.account = self.GetPrivateInfo()['test_chromoting_account']

  def testDenyAllowAccess(self):
    """Denies access and then allows access."""
    self.host.ContinueAuth()
    self.host.SignIn(self.account['username'], self.account['password'])
    self.host.DenyAccess()
    self.host.ContinueAuth()
    self.host.AllowAccess()

  def testSignOutAndSignBackIn(self):
    """Signs out from chromoting and signs back in."""
    self.host.ContinueAuth()
    self.host.SignIn(self.account['username'], self.account['password'])
    self.host.AllowAccess()
    self.host.SignOut()
    self.host.ContinueAuth()
    self.host.AllowAccess()


if __name__ == '__main__':
  chromoting_base.Main()
