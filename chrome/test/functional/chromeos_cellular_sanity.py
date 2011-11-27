#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import time

import pyauto_functional
import pyauto # pyauto_functional must come before pyauto

class ChromeosCellularSanity(pyauto.PyUITest):
  """Tests for ChromeOS network related functions."""

  assert os.popen('modem status').read(), 'Device needs modem to run test.'

  def testConnectCellularNetwork(self):
    """Connect to the cellular network if present."""

    self.ConnectToCellularNetwork()
    self.assertTrue(self.NetworkScan().get('connected_cellular'),
                   'Failed to connect to cellular network.')
    self.DisconnectFromCellularNetwork()
    self.assertFalse(self.NetworkScan().get('connected_cellular'),
                     'Failed to disconnect from cellular network.')


if __name__ == '__main__':
  pyauto_functional.Main()
