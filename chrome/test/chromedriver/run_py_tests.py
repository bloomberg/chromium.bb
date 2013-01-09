#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""End to end tests for ChromeDriver."""

import ctypes
import optparse
import os
import sys
import unittest

import chromedriver
import webserver

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import chrome_paths
from common import unittest_util


class ChromeDriverTest(unittest.TestCase):
  """End to end tests for ChromeDriver."""

  @classmethod
  def setUpClass(cls):
    cls._http_server = webserver.WebServer(chrome_paths.GetTestData())

  @classmethod
  def tearDownClass(cls):
    cls._http_server.Shutdown()

  @staticmethod
  def GetHttpUrlForFile(file_path):
    return ChromeDriverTest._http_server.GetUrl() + file_path

  def setUp(self):
    self._driver = chromedriver.ChromeDriver(_CHROMEDRIVER_LIB, _CHROME_BINARY)

  def tearDown(self):
    self._driver.Quit()

  def testStartStop(self):
    pass

  def testLoadUrl(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))

  def testEvaluateScript(self):
    self.assertEquals(1, self._driver.ExecuteScript('return 1'))
    self.assertEquals(None, self._driver.ExecuteScript(''))

  def testEvaluateScriptWithArgs(self):
    script = ('document.body.innerHTML = "<div>b</div><div>c</div>";' +
              'return {stuff: document.querySelectorAll("div")};')
    stuff = self._driver.ExecuteScript(script)['stuff']
    script = 'return arguments[0].innerHTML + arguments[1].innerHTML';
    self.assertEquals(
        'bc', self._driver.ExecuteScript(script, stuff[0], stuff[1]))

  def testEvaluateInvalidScript(self):
    self.assertRaises(chromedriver.ChromeDriverException,
                      self._driver.ExecuteScript, '{{{')

  def testSwitchToFrame(self):
    self._driver.ExecuteScript(
        'var frame = document.createElement("iframe");'
        'frame.id="id";'
        'frame.name="name";'
        'document.body.appendChild(frame);')
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame('id')
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame('name')
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrameByIndex(0)
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))

  def testGetTitle(self):
    script = 'document.title = "title"; return 1;'
    self.assertEquals(1, self._driver.ExecuteScript(script))
    self.assertEqual('title', self._driver.GetTitle())


if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--chromedriver', type='string', default=None,
      help='Path to a build of the chromedriver library(REQUIRED!)')
  parser.add_option(
      '', '--chrome', type='string', default=None,
      help='Path to a build of the chrome binary')
  parser.add_option(
      '', '--filter', type='string', default='*',
      help='Filter for specifying what tests to run, "*" will run all. E.g., ' +
           '*testStartStop')
  options, args = parser.parse_args()

  if (options.chromedriver is None or not os.path.exists(options.chromedriver)):
    parser.error('chromedriver is required or the given path is invalid.' +
                 'Please run "%s --help" for help' % __file__)

  global _CHROMEDRIVER_LIB
  _CHROMEDRIVER_LIB = os.path.abspath(options.chromedriver)
  global _CHROME_BINARY
  if options.chrome is not None:
    _CHROME_BINARY = os.path.abspath(options.chrome)
  else:
    _CHROME_BINARY = None

  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      sys.modules[__name__])
  tests = unittest_util.FilterTestSuite(all_tests_suite, options.filter)
  result = unittest.TextTestRunner().run(tests)
  sys.exit(len(result.failures) + len(result.errors))
