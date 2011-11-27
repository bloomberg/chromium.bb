# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Factory that creates ChromeDriver instances."""

import copy
from selenium.webdriver.remote.webdriver import WebDriver


class WebDriverWrapper(WebDriver):
  def __init__(self, executor, capabilities):
    super(WebDriverWrapper, self).__init__(executor, capabilities)
    self._did_quit = False

  def quit(self):
    if not self._did_quit:
      super(WebDriverWrapper, self).quit()

  def stop_client(self):
    self._did_quit = True


class ChromeDriverFactory(object):
  """Creates and tracks ChromeDriver instances."""
  def __init__(self, server, default_chrome=None):
    self._server = server
    self._default_chrome = default_chrome
    self._drivers = []

  def GetNewDriver(self, capabilities={}):
    """Returns a new RemoteDriver instance."""
    capabilities = copy.copy(capabilities)
    if self._default_chrome is not None and 'chrome.binary' not in capabilities:
      capabilities['chrome.binary'] = self._default_chrome
    driver = WebDriverWrapper(self._server.GetUrl(), capabilities)
    self._drivers += [driver]
    return driver

  def GetServer(self):
    """Returns the ChromeDriver server."""
    return self._server

  def QuitAll(self):
    """Quits all tracked drivers."""
    for driver in self._drivers:
      driver.quit()
    self._drivers = []
