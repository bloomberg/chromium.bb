#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def _SetupPaths():
  global SRC_THIRD_PARTY, PYTHON_BINDINGS, WEBDRIVER_TEST_DATA
  start_dir = os.path.abspath(os.path.dirname(__file__))
  J = os.path.join
  SRC_THIRD_PARTY = J(start_dir, os.pardir, os.pardir, os.pardir, 'third_party')
  webdriver = J(SRC_THIRD_PARTY, 'webdriver')
  PYTHON_BINDINGS = J(webdriver, 'python')
  WEBDRIVER_TEST_DATA = J(webdriver, 'test_data')

_SetupPaths()
