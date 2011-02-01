#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def _SetupPaths():
  global PYTHON_BINDINGS, WEBDRIVER_TEST_DATA
  start_dir = os.path.abspath(os.path.dirname(__file__))
  J = os.path.join
  webdriver = J(start_dir, os.pardir, os.pardir, os.pardir,
                'third_party', 'webdriver')
  PYTHON_BINDINGS = J(webdriver, 'python')
  WEBDRIVER_TEST_DATA = J(webdriver, 'test_data')

_SetupPaths()
