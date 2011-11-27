#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class PyAutoWebDriverTest(pyauto.PyUITest):
  """Tests PyAuto-WebDriver integration."""

  def testTypeIntoTextBox(self):
    """Type into a text input box and verify its value."""
    driver = self.NewWebDriver()
    driver.get('about:blank')
    driver.execute_script('document.body.innerHTML = "<input type=\'text\'>"')
    input = driver.find_element_by_tag_name('input')
    self.assertEquals('', input.get_attribute('value'))
    input.send_keys('test')
    self.assertEquals('test', input.get_attribute('value'))

  def testCanConnectToRestartedBrowser(self):
    """Restart the browser and connect again with WebDriver."""
    driver = self.NewWebDriver()
    self.RestartBrowser()
    driver = self.NewWebDriver()
    driver.get('about:blank')
    self.assertEquals('about:blank', driver.title)


if __name__ == '__main__':
  pyauto_functional.Main()
