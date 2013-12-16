# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script reports time spent by setup.exe in each install/update phase.

It does so by probing for InstallerExtraCode1 changes in the registry and can
run besides any setup.exe. It's best to launch it before setup.exe itself
starts, but can also time remaining stages if launched half-way through.

Refer to InstallerStage in chrome/installer/util/util_constants.h for a brief
description of each stage.

Note that the stages are numbered in the order they were added to setup's
implementation, not in the order they are meant to occur.

This script never ends, it will endlessly report stage timings until killed.
"""

import _winreg
import json
import optparse
import sys
import time


def TimeSetupStages(hive_str, state_key, product_guid, observed_code):
  """Observes setup.exe and reports about timings for each install/update stage.

  Does so by observing the registry value |observed_code| in the key at:
  |hive_str_|\|state_key|\|product_guid|.
  """
  hive = (_winreg.HKEY_LOCAL_MACHINE if hive_str == 'HKLM' else
          _winreg.HKEY_CURRENT_USER)
  key = 0
  try:
    key = _winreg.OpenKey(hive, state_key + product_guid, 0, _winreg.KEY_READ)
  except WindowsError as e:
    print 'Error opening %s\\%s\\%s: %s' % (hive_str, state_key, product_guid,
                                            e)
    return

  timings = []
  start_time = 0
  saw_start = False
  current_stage = 0
  try:
    current_stage, value_type = _winreg.QueryValueEx(key, observed_code)
    assert value_type == _winreg.REG_DWORD
    print 'Starting in already ongoing stage %u' % current_stage
    start_time = time.clock()
  except WindowsError:
    print 'No ongoing stage, waiting for next install/update cycle...'

  while True:
    new_stage = 0
    try:
      new_stage, value_type = _winreg.QueryValueEx(key, observed_code)
      assert value_type == _winreg.REG_DWORD
    except WindowsError:
      # Handle the non-existant case by simply leaving |new_stage == 0|.
      pass
    if current_stage == new_stage:
      # Keep probing until a change is seen.
      time.sleep(0.01)
      continue

    if current_stage != 0:
      # Round elapsed time to 2 digits precision; anything beyond that would be
      # bogus given the above polling loop's precision.
      elapsed_time = round(time.clock() - start_time, 2)
      if saw_start:
        print '%s: Stage %u took %.2f seconds.' % (
            time.strftime("%x %X", time.localtime()), current_stage,
            elapsed_time)
        timings.append({'stage': current_stage, 'time': elapsed_time})
      else:
        print '%s: The remainder of stage %u took %.2f seconds.' % (
            time.strftime("%x %X", time.localtime()), current_stage,
            elapsed_time)
        # Log this timing, but mark that it was already ongoing when this script
        # started timing it.
        timings.append({'stage': current_stage, 'time': elapsed_time,
                        'status': 'missed_start'})

    if new_stage != 0:
      print '%s: Starting stage %u...' % (
          time.strftime("%x %X", time.localtime()), new_stage)
      saw_start = True
    else:
      print '%s: Install/update complete, stages timings:' % (
          time.strftime("%x %X", time.localtime()))
      print json.dumps(timings, indent=2, sort_keys=True)
      timings = []
      print '%s: No more stages, waiting for next install/update cycle...' % (
          time.strftime("%x %X", time.localtime()))

    current_stage = new_stage
    start_time = time.clock()


def main():
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage,
                                 description="Times Chrome's installer stages.")
  parser.add_option('--hive', default='HKLM',
                    help='The hive to observe: "HKLM" for system-level '
                         'installs, "HKCU" for user-level installs, defaults '
                         'to HKLM.')
  parser.add_option('--state-key',
                    default='Software\\Google\\Update\\ClientState\\',
                    help="The client state key to observe, defaults to Google "
                         "Update's.")
  parser.add_option('--product-guid',
                    default='{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}',
                    help="The GUID of the product to observe: defaults to "
                         "the GUID for the Google Chrome Binaries which is the "
                         "one being written to on updates.")
  parser.add_option('--observed-code', default='InstallerExtraCode1',
                    help='The installer code to observe under '
                         '|state_key|\\|product_guid|, defaults to '
                         'InstallerExtraCode1.')
  options, _ = parser.parse_args()

  TimeSetupStages(options.hive, options.state_key, options.product_guid,
                  options.observed_code)


if __name__ == '__main__':
  sys.exit(main())
