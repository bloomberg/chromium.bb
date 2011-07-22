#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ChromeosPrefsTest(pyauto.PyUITest):
  """TestCase for ChromeOS Preferences."""

  def testDefaultUserImage(self):
    """Verify changing default user image prefs work."""

    # Defined in src/chrome/browser/chromeos/login/user_manager.cc
    k_logged_in_users = 'LoggedInUsers'
    k_user_images = 'UserImages'
    # Defined in src/chrome/browser/chromeos/login/default_user_images.cc
    image1 = u'default:4'
    image2 = u'default:5'

    for image in image1, image2:
      logged_in_user = \
          self.GetLocalStatePrefsInfo().Prefs(k_logged_in_users)[0]
      user_images = {}
      user_images[logged_in_user] = image
      self.SetLocalStatePrefs(k_user_images, user_images)
      self.RestartBrowser(clear_profile=False)
      current_user_images = self.GetLocalStatePrefsInfo().Prefs(k_user_images)
      current_image = current_user_images.get(logged_in_user)
      self.assertEqual(image, current_image)


if __name__ == '__main__':
  pyauto_functional.Main()

