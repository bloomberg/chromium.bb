# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome-specific options for configuring a ChromeDriver instance."""

import base64


class ChromeOptions(object):
  """Chrome-specific options for configuring a ChromeDriver instance."""

  def __init__(self):
    """Initialize ChromeOptions object."""
    self._capabilities = {'chrome.switches': [], 'chrome.extensions': []}

  def AddSwitch(self, switch):
    """Add a switch to be passed to Chrome.

    Args:
      switch: String switch to be passed to Chrome.
    """
    self._capabilities['chrome.switches'].append(switch)

  def AddExtension(self, extension):
    """Add an extension to be loaded onto Chrome.

    Args:
      extension: String path to the extension to be loaded onto Chrome.
    """
    with open(extension, 'rb') as ext_file:
      self._capabilities['chrome.extensions'].append(
          base64.b64encode(ext_file.read()))

  def SetUserDataDir(self, user_data_dir):
    """Set the Chrome user data dir.

    Args:
      user_data_dir: String path to the profile directory.
    """
    self.AddSwitch('user-data-dir=%s' % user_data_dir)

  def GetCapabilities(self):
    """Returns a capabilities object suitable for using with ChromeDriver."""
    return self._capabilities
