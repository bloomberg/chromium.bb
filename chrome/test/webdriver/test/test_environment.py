#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sets up and tears down the global environment for all ChromeDriver tests."""

import sys

from chromedriver_factory import ChromeDriverFactory
from chromedriver_launcher import ChromeDriverLauncher
import test_paths


_server = None
_webserver = None


def GetServer():
  return _server


def GetTestDataUrl():
  return _webserver.GetUrl()


def SetUp(alternative_exe=None):
  if alternative_exe is not None:
    test_paths.CHROMEDRIVER_EXE = alternative_exe

  global _server, _webserver
  _server = ChromeDriverLauncher(test_paths.CHROMEDRIVER_EXE).Launch()
  _webserver = ChromeDriverLauncher(
      test_paths.CHROMEDRIVER_EXE, test_paths.CHROMEDRIVER_TEST_DATA).Launch()


def TearDown():
  global _server, _webserver
  _server.Kill()
  _webserver.Kill()
