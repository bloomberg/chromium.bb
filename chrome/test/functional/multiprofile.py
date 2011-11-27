#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class MultiprofileTest(pyauto.PyUITest):
  """Tests for Multi-Profile / Multi-users"""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Hit <enter> to dump info.. ')
      self.pprint(self.GetMultiProfileInfo())

  def ExtraChromeFlags(self):
    """Enable multi-profiles on linux too."""
    # TODO: Remove when --multi-profiles is enabled by default on linux.
    flags = pyauto.PyUITest.ExtraChromeFlags(self)
    if self.IsLinux():
      flags.append('--multi-profiles')
    return flags

  def testBasic(self):
    """Multi-profile windows can open."""
    self.assertEqual(1, self.GetBrowserWindowCount())
    self.assertTrue(self.GetMultiProfileInfo()['enabled'],
        msg='Multi-profile is not enabled')
    self.OpenNewBrowserWindowWithNewProfile()

    # Verify multi-profile info.
    multi_profile = self.GetMultiProfileInfo()
    self.assertEqual(2, len(multi_profile['profiles']))
    new_profile = multi_profile['profiles'][1]
    self.assertTrue(new_profile['name'])

    # Verify browser windows.
    self.assertEqual(2, self.GetBrowserWindowCount(),
        msg='New browser window did not open')
    info = self.GetBrowserInfo()
    new_profile_window = info['windows'][1]
    self.assertEqual('Profile 1', new_profile_window['profile_path'])
    self.assertEqual(1, len(new_profile_window['tabs']))
    self.assertEqual('about:blank', new_profile_window['tabs'][0]['url'])

  def test20NewProfiles(self):
    """Verify we can create 20 new profiles."""
    for index in range(1, 21):
      self.OpenNewBrowserWindowWithNewProfile()
      multi_profile = self.GetMultiProfileInfo()
      self.assertEqual(index + 1, len(multi_profile['profiles']),
          msg='Expected %d profiles after adding %d new users. Got %d' % (
              index + 1, index, len(multi_profile['profiles'])))


if __name__ == '__main__':
  pyauto_functional.Main()
