#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class MultiprofileTest(pyauto.PyUITest):
  """Tests for Multi-Profile / Multi-users"""

  def testBasic(self):
    """Multi-profile windows can open"""
    self.assertTrue(1, self.GetBrowserWindowCount())
    self.OpenNewBrowserWindowWithNewProfile()
    self.assertEqual(2, self.GetBrowserWindowCount(),
        msg='New browser window did not open')
    info = self.GetBrowserInfo()
    new_profile_window = info['windows'][1]
    self.assertEqual('profile_1', new_profile_window['profile_path'])
    self.assertEqual(1, len(new_profile_window['tabs']))
    self.assertEqual('chrome://newtab/', new_profile_window['tabs'][0]['url'])


if __name__ == '__main__':
  pyauto_functional.Main()
