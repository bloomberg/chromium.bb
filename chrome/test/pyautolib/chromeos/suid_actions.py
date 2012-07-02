#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to perform actions as a super-user on ChromeOS.

Needs to be run with superuser privileges, typically using the
suid_python binary.

Usage:
  sudo python suid_actions.py --action=CleanFlimflamDirs
"""

import logging
import optparse
import os
import shutil
import sys
import time

sys.path.append('/usr/local')  # to import autotest libs.
from autotest.cros import constants
from autotest.cros import cryptohome

# TODO(bartfab): Remove when crosbug.com/20709 is fixed.
sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
from pyauto import AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE


class SuidAction(object):
  """Helper to perform some super-user actions on ChromeOS."""

  def _ParseArgs(self):
    parser = optparse.OptionParser()
    parser.add_option(
        '-a', '--action', help='Action to perform.')
    self._options = parser.parse_args()[0]
    if not self._options.action:
      raise RuntimeError('No action specified.')

  def Run(self):
    self._ParseArgs()
    assert os.geteuid() == 0, 'Needs superuser privileges.'
    handler = getattr(self, self._options.action)
    assert handler and callable(handler), \
        'No handler for %s' % self._options.action
    handler()
    return 0

  ## Actions ##
  def CleanFlimflamDirs(self):
    """Clean the contents of all connection manager (shill/flimflam) profiles.
    """
    flimflam_dirs = ['/home/chronos/user/flimflam',
                     '/home/chronos/user/shill',
                     '/var/cache/flimflam',
                     '/var/cache/shill']

    # The stop/start flimflam command should stop/start shill respectivly if
    # enabled.
    os.system('stop flimflam')
    try:
      for flimflam_dir in flimflam_dirs:
        if not os.path.exists(flimflam_dir):
          continue
        for item in os.listdir(flimflam_dir):
          path = os.path.join(flimflam_dir, item)
          if os.path.isdir(path):
            shutil.rmtree(path)
          else:
            os.remove(path)
    finally:
      os.system('start flimflam')
      # TODO(stanleyw): crosbug.com/29421 This method should wait until
      # flimflam/shill is fully initialized and accessible via DBus again.
      # Otherwise, there is a race conditions and subsequent accesses to
      # flimflam/shill may fail. Until this is fixed, waiting for the
      # resolv.conf file to be created is better than nothing.
      begin = time.time()
      while not os.path.exists(constants.RESOLV_CONF_FILE):
        if time.time() - begin > 10:
          raise RuntimeError('Timeout while waiting for flimflam/shill start.')
        time.sleep(.25)

  def RemoveAllCryptohomeVaults(self):
    """Remove any existing cryptohome vaults."""
    cryptohome.remove_all_vaults()

  def TryToDisableLocalStateAutoClearing(self):
    """Try to disable clearing of the local state on session manager startup.

    This will fail if rootfs verification is on.
    TODO(bartfab): Remove this method when crosbug.com/20709 is fixed.
    """
    os.system('mount -o remount,rw /')
    os.remove(AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE)
    os.system('mount -o remount,ro /')
    if os.path.exists(AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE):
      logging.debug('Failed to remove %s. Session manager will clear local '
                    'state on startup.' % AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE)
    else:
      logging.debug('Removed %s. Session manager will not clear local state on '
                    'startup.' % AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE)

  def TryToEnableLocalStateAutoClearing(self):
    """Try to enable clearing of the local state on session manager startup.

    This will fail if rootfs verification is on.
    TODO(bartfab): Remove this method when crosbug.com/20709 is fixed.
    """
    os.system('mount -o remount,rw /')
    open(AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE, 'w').close()
    os.system('mount -o remount,ro /')
    if os.path.exists(AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE):
      logging.debug('Created %s. Session manager will clear local state on '
                    'startup.' % AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE)
    else:
      logging.debug('Failed to create %s. Session manager will not clear local '
                    'state on startup.' % AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE)


if __name__ == '__main__':
  sys.exit(SuidAction().Run())
