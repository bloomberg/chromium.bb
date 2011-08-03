#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ChromeosPrefsTest(pyauto.PyUITest):
  """TestCase for ChromeOS Preferences."""

  # Defined in src/chrome/browser/chromeos/login/user_manager.cc
  k_logged_in_users = 'LoggedInUsers'
  k_user_images = 'UserImages'

  def testDefaultUserImage(self):
    """Verify changing default user image prefs work."""

    # Defined in src/chrome/browser/chromeos/login/default_user_images.cc
    image1 = u'default:4'
    image2 = u'default:5'

    for image in image1, image2:
      logged_in_user = \
          self.GetLocalStatePrefsInfo().Prefs(
              ChromeosPrefsTest.k_logged_in_users)[0]
      user_images = {}
      user_images[logged_in_user] = image
      self.SetLocalStatePrefs( ChromeosPrefsTest.k_user_images, user_images)
      self.RestartBrowser(clear_profile=False)
      current_user_images = self.GetLocalStatePrefsInfo().Prefs(
          ChromeosPrefsTest.k_user_images)
      current_image = current_user_images.get(logged_in_user)
      self.assertEqual(image, current_image,
                       msg='Default user image was not set in preferences.')

  def testCaptureUserPhoto(self):
    """Verify capturing/saving user photo works."""

    logged_in_user = \
        self.GetLocalStatePrefsInfo().Prefs(
            ChromeosPrefsTest.k_logged_in_users)[0]
    # Defined in src/chrome/browser/chromeos/login/user_manager.cc
    expected_photo_name = logged_in_user + '.png'

    self.CaptureProfilePhoto()

    for i in range(2):
      current_user_images = self.GetLocalStatePrefsInfo().Prefs(
          ChromeosPrefsTest.k_user_images)
      current_image_path = current_user_images.get(logged_in_user)
      self.assertTrue(expected_photo_name in current_image_path,
                      msg='Captured user photo was not set in preferences.')
      self.assertTrue(os.path.isfile(current_image_path),
          msg='Image file was not saved.') and \
          self.assertTrue(os.path.getsize(current_image_path) > 1000,
          msg='Image file may not be valid.' )
      if not i:
        self.RestartBrowser(clear_profile=False)


if __name__ == '__main__':
  pyauto_functional.Main()
