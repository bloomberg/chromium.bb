#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ChromeosFirmwareVersionChecker(pyauto.PyUITest):
  """TestCases for verifying the firmware and EC versions.

  The chromeos-firmwareupdate is a shellball that will return what version of
  the firmware and EC is running.  This test will check the versions against a
  whitelist to verify that these items don't change unexpectedly.

  """

  def _GetWhitelistVersion(self, component):
    """Obtains the valid firmware or EC from a data file.

    Args:
      component: The component of which to return the version.  There are
                 currently only two supported types: "BIOS version" and
                 "EC version".
    Returns:
      The version of the component in string form.
    """
    firmware_info = os.path.join(pyauto.PyUITest.DataDir(),
                                 'pyauto_private/chromeos/',
                                 'chromeos_firmware_info.txt')
    assert os.path.exists(firmware_info), 'Data file does not exist.'
    return self.EvalDataFrom(firmware_info)[self.ChromeOSBoard()][component]

  def _GetSystemVersionMario(self, component, info):
    """ Returns the version of the component from the system.

    Mario has a legacy info format so this function does the parsing differently
    than the one below.

    Args:
      component: The component of which to return the version.
      info: The version information to be parsed.
    Returns:
      The version of the component in string form.
    """
    items = info.strip().splitlines()
    # This is going to give us a list of lines, we are looking for the
    # following ones:
    #   BIOS image: <hash> <path>/board.xx.xx.xxx.xxx.xx
    #   EC image: <hash> <path>/board_xxx

    # Convert the passed component string into the format of the mario
    if component == 'BIOS version':
      component = 'BIOS'
    if component == 'EC version':
      component = 'EC'
    for line in items:
      line_components = line.split()
      if len(line_components) >= 4 and line_components[0].strip() == component:
        return os.path.basename(line_components[3])
    self.fail('Could not locate the following item %s in the return value of '
              'chromeos-firmwareupdate.' % component)

  def _GetSystemVersion(self, component, info):
    """ Returns the version of the desired component.

    Args:
      component: The component of which to return the version of.
      info: The version information to be parsed.
    Returns:
      The version of the component in string form.
    """
    # Check if we are on mario, then we need to use the legacy parser
    if self.ChromeOSBoard() == 'x86-mario':
      return self._GetSystemVersionMario(component, info)
    items = info.strip().splitlines()
    # This is going to give us a list of lines, we are looking for the
    # following ones:
    #   BIOS version: board.xx.xx.xxx.xxx.xx
    #   EC version: foobar
    for line in items:
      line_components = line.split(':')
      # The line we are looking for has at least 2 items
      if len(line_components) >= 2 and line_components[0] == component:
        return line_components[1].strip()
    self.fail('Could not locate the following item %s in the return value '
              'of chromeos-firmwareupdate.' % component)

  def _PerformCompare(self, component):
    """Performs a comparision between the component installed and the whitelist.

    Args:
      component: The component of which to return the version of.
    """

    updater_commands = ['/usr/sbin/chromeos-firmwareupdate', '-V']
    content = subprocess.Popen(updater_commands,
                               stdout=subprocess.PIPE).stdout.read()
    system_version = self._GetSystemVersion(component, content)
    whitelist_version = self._GetWhitelistVersion(component)
    self.assertEqual(system_version, whitelist_version, msg='%s does not match'
                     ' what is in the whitelist.\n\tSystem: %s\n\tWhitelist: '
                     '%s' % (component, system_version, whitelist_version))

  def testFirmwareVersion(self):
    self._PerformCompare('BIOS version')

  def testECVersion(self):
    self._PerformCompare('EC version')


if __name__ == '__main__':
  pyauto_functional.Main()
