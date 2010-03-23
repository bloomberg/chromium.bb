#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import pyauto_functional
from pyauto import PyUITest


class SimpleTest(PyUITest):

  def testCanOpenGoogle(self):
    self.NavigateToURL("http://www.google.com")


if __name__ == '__main__':
  pyauto_functional.Main()
