#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enable chrome testing interface on ChromeOS.

Enables chrome automation over a named automation channel on ChromeOS.
Also, allows passing extra flags to chrome (--extra-chrome-flags).
The path to named testing interface automation socket is printed out.

Needs to be run with superuser privileges.

Usage:
  sudo python enable_testing.py --extra-chrome-flags="--homepage=about:blank"
"""

import dbus
import optparse
import os


class EnableChromeTestingOnChromeOS(object):
  """Helper to enable chrome testing interface on ChromeOS.

  Also, can add additional flags to chrome to be used for testing.
  """

  SESSION_MANAGER_INTERFACE = 'org.chromium.SessionManagerInterface'
  SESSION_MANAGER_PATH = '/org/chromium/SessionManager'
  SESSION_MANAGER_SERVICE = 'org.chromium.SessionManager'

  def _ParseArgs(self):
    parser = optparse.OptionParser()
    parser.add_option(
        '', '--extra-chrome-flags', action='append', default=[],
        help='Pass extra flags to chrome.')
    self._options, self._args = parser.parse_args()

  def Run(self):
    self._ParseArgs()
    assert os.geteuid() == 0, 'Needs superuser privileges.'
    system_bus = dbus.SystemBus()
    manager = dbus.Interface(system_bus.get_object(self.SESSION_MANAGER_SERVICE,
                                                   self.SESSION_MANAGER_PATH),
                             self.SESSION_MANAGER_INTERFACE)
    print manager.EnableChromeTesting(True, self._options.extra_chrome_flags)


if __name__ == '__main__':
  enabler = EnableChromeTestingOnChromeOS()
  enabler.Run()
