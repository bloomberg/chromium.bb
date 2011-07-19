#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Factory that creates ChromeDriver instances."""

import unittest

from chromedriver_factory import ChromeDriverFactory
import test_environment


class ChromeDriverTest(unittest.TestCase):
  """Fixture for tests that need to instantiate ChromeDriver(s)."""

  def setUp(self):
    self._factory = ChromeDriverFactory(test_environment.GetServer())

  def tearDown(self):
    self._factory.QuitAll()

  def GetNewDriver(self, capabilities={}):
    """Returns a new RemoteDriver instance."""
    self.assertTrue(self._factory, 'ChromeDriverTest.setUp must be called')
    return self._factory.GetNewDriver(capabilities)
