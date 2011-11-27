# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Factory that creates ChromeDriver instances for pyauto."""

import os
import random
import tempfile

import pyauto_paths
from selenium import webdriver
from selenium.webdriver.chrome import service


class ChromeDriverFactory(object):
  """"Factory that creates ChromeDriver instances for pyauto.

  Starts a single ChromeDriver server when necessary. Users should call 'Stop'
  when no longer using the factory.
  """

  def __init__(self):
    self._chromedriver_server = None

  def NewChromeDriver(self, pyauto):
    """Creates a new remote WebDriver instance.

    This instance will connect to a new automation provider of an already
    running Chrome.

    Args:
      pyauto: pyauto.PyUITest instance

    Returns:
      selenium.webdriver.remote.webdriver.WebDriver instance.
    """
    self._StartServerIfNecessary()
    channel_id = 'testing' + hex(random.getrandbits(20 * 4))[2:-1]
    if not pyauto.IsWin():
      channel_id = os.path.join(tempfile.gettempdir(), channel_id)
    pyauto.CreateNewAutomationProvider(channel_id)
    return webdriver.Remote(self._chromedriver_server.service_url,
                            {'chrome.channel': channel_id})

  def _StartServerIfNecessary(self):
    """Starts the ChromeDriver server, if not already started."""
    if self._chromedriver_server is None:
      exe = pyauto_paths.GetChromeDriverExe()
      assert exe, 'Cannot find chromedriver exe. Did you build it?'
      self._chromedriver_server = service.Service(exe)
      self._chromedriver_server.start()

  def Stop(self):
    """Stops the ChromeDriver server, if running."""
    if self._chromedriver_server is not None:
      self._chromedriver_server.stop()
      self._chromedriver_server = None

  def __del__(self):
    self.Stop()
