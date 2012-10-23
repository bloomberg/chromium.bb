#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs sample updater and install tests using one or more Chrome builds.

Test requires two arguments: build url and chrome versions to be used, rest of
the arguments are optional. Builds must be specified in ascending order, with
the lower version first and the higher version last, each separated by commas.
Separate arguments are required for install and update test scenarios. The
setUp method creates a ChromeDriver instance, which can be used to run tests.
ChromeDriver is shutdown upon completion of the testcase.

Example:
    $ python sample_updater.py --url=<URL> --builds=19.0.1069.0,19.0.1070.0
"""

import os
import sys
import unittest

from install_test import InstallTest
from install_test import Main


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


if __name__ == '__main__':
  Main()
