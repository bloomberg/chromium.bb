#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all of the ChromeDriver tests.

For ChromeDriver documentation, refer to:
  http://dev.chromium.org/developers/testing/webdriver-for-chrome
"""

import unittest
from gtest_text_test_runner import GTestTextTestRunner

if __name__ == '__main__':
  unittest.main(module='chromedriver_tests',
                testRunner=GTestTextTestRunner(verbosity=1))
