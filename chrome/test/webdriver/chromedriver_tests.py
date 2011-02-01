#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for ChromeDriver.

If your test is testing a specific part of the WebDriver API, consider adding
it to the appropriate place in the WebDriver tree instead.
"""

import os
import sys
import unittest

from chromedriver_launcher import ChromeDriverLauncher
import chromedriver_paths
from gtest_text_test_runner import GTestTextTestRunner

sys.path += [chromedriver_paths.PYTHON_BINDINGS]

from selenium.webdriver.remote.webdriver import WebDriver


class ChromeDriverTest(unittest.TestCase):
  def setUp(self):
    self._launcher = ChromeDriverLauncher()

  def testSessionCreationDeletion(self):
    driver = WebDriver(self._launcher.GetURL(), 'chrome', 'any')
    driver.quit()


if __name__ == '__main__':
  unittest.main(module='chromedriver_tests',
                testRunner=GTestTextTestRunner(verbosity=1))
