#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys


def _SetupPaths():
  sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
  sys.path.append('/usr/local')  # to import autotest libs

_SetupPaths()

from autotest.cros import constants
import pyauto


class ChromeosUtils(pyauto.PyUITest):
  """Utils for ChromeOS."""

  def LoginToDefaultAccount(self):
    """Login to ChromeOS using default testing account.

    Usage:
      python chromeos_utils.py \
        chromeos_utils.ChromeosUtils.LoginToDefaultAccount
    """
    creds = constants.CREDENTIALS['$default']
    username = creds[0]
    passwd = creds[1]
    self.Login(username, passwd)
    logging.info('Logged in as %s' % username)


if __name__ == '__main__':
  pyauto.Main()
