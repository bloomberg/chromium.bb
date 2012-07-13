#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class TestOffline(pyauto.PyUITest):
  """Tests the offline detection for ChromeOS."""

  assert pyauto.PyUITest.IsChromeOS(), 'Works on ChromeOS only.'

  def tearDown(self):
    self.RestoreOnline()
    pyauto.PyUITest.tearDown(self)

  def testGoOffline(self):
    """Tests the GoOffline pyauto method."""

    self.GoOffline()
    # Device takes a little bit of time to realize it's offline/online.
    self.assertTrue(self.WaitUntil(
                    lambda: self.NetworkScan().get('offline_mode')),
                    msg='We are not offline.')


if __name__ == '__main__':
  pyauto_functional.Main()
