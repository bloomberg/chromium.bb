#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto

sys.path.append('/usr/local')  # Required to import autotest libs
from autotest.cros import constants


class ChromeosPrefsTest(pyauto.PyUITest):
  """TestCase for ChromeOS Preferences."""

  # Defined in src/chrome/browser/chromeos/login/user_manager.cc
  k_logged_in_users = 'LoggedInUsers'
  k_user_images = 'UserImages'
  k_image_path_node_name = 'path'

  def testAllUserImage(self):
    """Verify changing all available default user images in Change picture."""

    logged_in_user = constants.CREDENTIALS['$default'][0]
    for i in range(19):
      image = {
          "index": i,
          "path": ""
      }
      user_images = {}
      user_images[logged_in_user] = image
      self.SetLocalStatePrefs(ChromeosPrefsTest.k_user_images, user_images)
      self.RestartBrowser(clear_profile=False)
      current_user_images = self.GetLocalStatePrefsInfo().Prefs(
          ChromeosPrefsTest.k_user_images)
      current_image = current_user_images.get(logged_in_user)
      self.assertEqual(image, current_image,
                       msg='Default user image was not set in preferences.')


if __name__ == '__main__':
  pyauto_functional.Main()
