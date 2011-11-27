#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional
import pyauto


class ChromeosPower(pyauto.PyUITest):
  """Tests for ChromeOS power library and daemon."""

  def testBatteryInfo(self):
    """Print some information about the battery."""
    result = self.GetBatteryInfo()
    self.assertTrue(result)
    logging.debug(result)


if __name__ == '__main__':
  pyauto_functional.Main()
