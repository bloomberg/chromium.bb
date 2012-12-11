# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests that demonstrate use of install test framework."""

import os
import sys
import unittest

from install_test import InstallTest


class SampleUpdater(InstallTest):
  """Sample update tests."""

  def testCanOpenGoogle(self):
    """Simple Navigation.

    This test case illustrates how to run an update test. Update tests require
    two or more builds.
    """
    self.Install(self.GetUpdateBuilds()[0])
    self.StartChrome()
    self._driver.get('http://www.google.com/')
    self.Install(self.GetUpdateBuilds()[1])
    self.StartChrome()
    self._driver.get('http://www.google.org/')

  def testCanOpenGoogleOrg(self):
    """Simple Navigation.

    This test case illustrates how to run a fresh install test. Simple install
    tests, unlike update test, require only a single build.
    """
    self.Install(self.GetInstallBuild())
    self.StartChrome()
    self._driver.get('http://www.google.org/')

  def testCanOpenNewTab(self):
    """Sample of using the extended webdriver interface."""
    self.Install(self.GetInstallBuild())
    self.StartChrome()
    self._driver.create_blank_tab()
