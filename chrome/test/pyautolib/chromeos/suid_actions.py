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
import subprocess
import sys
import time

sys.path.append('/usr/local')  # to import autotest libs.
from autotest.cros import constants
from autotest.cros import cryptohome

# TODO(bartfab): Remove when crosbug.com/20709 is fixed.
sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
from pyauto import AUTO_CLEAR_LOCAL_STATE_MAGIC_FILE

TEMP_BACKCHANNEL_FILE = '/tmp/pyauto_network_backchannel_file'

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

  def _GetEthInterfaces(self):
    """Returns a list of the eth* interfaces detected by the device."""
    # Assumes ethernet interfaces all have "eth" in the name.
    import pyudev
    return sorted([iface.sys_name for iface in
                   pyudev.Context().list_devices(subsystem='net')
                   if 'eth' in iface.sys_name])

  def _Renameif(self, old_iface, new_iface, mac_address):
    """Renames the interface with mac_address from old_iface to new_iface.

    Args:
      old_iface: The name of the interface you want to change.
      new_iface: The name of the interface you want to change to.
      mac_address:  The mac address of the interface being changed.
    """
    subprocess.call(['stop', 'flimflam'])
    subprocess.call(['ifconfig', old_iface, 'down'])
    subprocess.call(['nameif', new_iface, mac_address])
    subprocess.call(['ifconfig', new_iface, 'up'])
    subprocess.call(['start', 'flimflam'])

    # Check and make sure interfaces have been renamed
    eth_ifaces = self._GetEthInterfaces()
    if new_iface not in eth_ifaces:
      raise RuntimeError('Interface %s was not renamed to %s' %
                         (old_iface, new_iface))
    elif old_iface in eth_ifaces:
      raise RuntimeError('Old iface %s is still present' % old_iface)

  def SetupBackchannel(self):
    """Renames the connected ethernet interface to eth_test for offline mode
       testing.  Does nothing if no connected interface is found.
    """
    # Return the interface with ethernet connected or returns if none found.
    for iface in self._GetEthInterfaces():
      with open('/sys/class/net/%s/operstate' % iface, 'r') as fp:
        if 'up' in fp.read():
          eth_iface = iface
          break
    else:
      return

    # Write backup file to be used by TeardownBackchannel to restore the
    # interface names.
    with open(TEMP_BACKCHANNEL_FILE, 'w') as fpw:
      with open('/sys/class/net/%s/address' % eth_iface) as fp:
        mac_address = fp.read().strip()
        fpw.write('%s, %s' % (eth_iface, mac_address))

    self._Renameif(eth_iface, 'eth_test', mac_address)

  def TeardownBackchannel(self):
    """Restores the eth interface names if SetupBackchannel was called."""
    if not os.path.isfile(TEMP_BACKCHANNEL_FILE):
      return

    with open(TEMP_BACKCHANNEL_FILE, 'r') as fp:
      eth_iface, mac_address = fp.read().split(',')

    self._Renameif('eth_test', eth_iface, mac_address)
    os.remove(TEMP_BACKCHANNEL_FILE)


if __name__ == '__main__':
  sys.exit(SuidAction().Run())
