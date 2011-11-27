# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Factory that creates ChromeDriver instances."""

import os
import unittest

from chromedriver_factory import ChromeDriverFactory
from chromedriver_launcher import ChromeDriverLauncher
import test_paths


class ChromeDriverTest(unittest.TestCase):
  """Fixture for tests that need to instantiate ChromeDriver(s)."""

  @staticmethod
  def GlobalSetUp(other_driver=None, other_chrome=None):
    driver_path = other_driver or test_paths.CHROMEDRIVER_EXE
    chrome_path = other_chrome or test_paths.CHROME_EXE
    if driver_path is None or not os.path.exists(driver_path):
      raise RuntimeError('ChromeDriver could not be found')
    if chrome_path is None or not os.path.exists(chrome_path):
      raise RuntimeError('Chrome could not be found')

    ChromeDriverTest._driver_path = driver_path
    ChromeDriverTest._chrome_path = chrome_path
    ChromeDriverTest._server = ChromeDriverLauncher(driver_path).Launch()
    ChromeDriverTest._webserver = ChromeDriverLauncher(
        driver_path, test_paths.TEST_DATA_PATH).Launch()

  @staticmethod
  def GlobalTearDown():
    ChromeDriverTest._server.Kill()
    ChromeDriverTest._webserver.Kill()

  @staticmethod
  def GetTestDataUrl():
    """Returns the base url for serving files from the ChromeDriver test dir."""
    return ChromeDriverTest._webserver.GetUrl()

  @staticmethod
  def GetDriverPath():
    """Returns the path to the default ChromeDriver binary to use."""
    return ChromeDriverTest._driver_path

  @staticmethod
  def GetChromePath():
    """Returns the path to the default Chrome binary to use."""
    return ChromeDriverTest._chrome_path

  def setUp(self):
    self._factory = ChromeDriverFactory(self._server,
                                        self.GetChromePath())

  def tearDown(self):
    self._factory.QuitAll()

  def GetNewDriver(self, capabilities={}):
    """Returns a new RemoteDriver instance."""
    self.assertTrue(self._factory, 'ChromeDriverTest.setUp must be called')
    return self._factory.GetNewDriver(capabilities)
