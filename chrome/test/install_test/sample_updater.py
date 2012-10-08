#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs sample updater test using two Chrome builds.

Test requires two arguments: build url and chrome versions to be used, rest of
the arguments are optional. Builds must be specified in ascending order, with
the lower version first and the higher version last, each separated by commas.
The setUp method creates a ChromeDriver instance, which can be used to run
tests. ChromeDriver is shutdown, upon completion of the testcase.

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
    """Simple Navigation."""
    self._driver.get('http://www.google.com/')
    self.UpdateBuild()
    self._driver.get('http://www.google.org/')


if __name__ == '__main__':
  Main()
