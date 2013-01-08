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

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import unittest_util


class ChromeDriverTest(unittest.TestCase):
  """End to end tests for ChromeDriver."""

  def testStartStop(self):
    driver = chromedriver.ChromeDriver(_CHROMEDRIVER_LIB, _CHROME_BINARY)
    driver.Quit()

  def testLoadUrl(self):
    driver = chromedriver.ChromeDriver(_CHROMEDRIVER_LIB, _CHROME_BINARY)
    driver.Load('http://www.google.com')
    driver.Quit()

  def testEvaluateScript(self):
    driver = chromedriver.ChromeDriver(_CHROMEDRIVER_LIB, _CHROME_BINARY)
    self.assertEquals(1, driver.ExecuteScript('return 1'))
    self.assertEquals(None, driver.ExecuteScript(''))
    driver.Quit()

  def testEvaluateScriptWithArgs(self):
    driver = chromedriver.ChromeDriver(_CHROMEDRIVER_LIB, _CHROME_BINARY)
    script = ('document.body.innerHTML = "<div>b</div><div>c</div>";' +
              'return {stuff: document.querySelectorAll("div")};')
    stuff = driver.ExecuteScript(script)['stuff']
    script = 'return arguments[0].innerHTML + arguments[1].innerHTML';
    self.assertEquals('bc', driver.ExecuteScript(script, stuff[0], stuff[1]))
    driver.Quit()

  def testEvaluateInvalidScript(self):
    driver = chromedriver.ChromeDriver(_CHROMEDRIVER_LIB, _CHROME_BINARY)
    self.assertRaises(chromedriver.ChromeDriverException,
                      driver.ExecuteScript, '{{{')
    driver.Quit()


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
