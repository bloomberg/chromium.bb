# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""DeDeDe configs."""

from __future__ import print_function

BUILD_WORKON_PACKAGES = (
    'chromeos-ec',
    'coreboot',
    'depthcharge',
    'libpayload',
    'vboot_reference',
)

BUILD_PACKAGES = BUILD_WORKON_PACKAGES + (
    'chromeos-bootimage',
    'coreboot-private-files',
    'intel-jslfsp',
)


def is_fast_required(_use_futility, _servo):
  """Returns true if --fast is necessary to flash successfully.

  The configurations in this function consistently fail on the verify step,
  adding --fast removes verification of the flash and allows these configs to
  flash properly. Meant to be a temporary hack until b/143240576 is fixed.

  Args:
    _use_futility (bool): True if futility is to be used, False if
      flashrom.
    _servo (str): The type name of the servo device being used.

  Returns:
    bool: True if fast is necessary, False otherwise.
  """
  return False


def get_commands(servo):
  """Get specific flash commands for octopus

  Each board needs specific commands including the voltage for Vref, to turn
  on and turn off the SPI flash. The get_*_commands() functions provide a
  board-specific set of commands for these tasks. The voltage for this board
  needs to be set to 3.3 V.

  Args:
    servo (servo_lib.Servo): The servo connected to the target DUT.

  Returns:
    list: [dut_control_on, dut_control_off, flashrom_cmd, futility_cmd]
      dut_control*=2d arrays formmated like [["cmd1", "arg1", "arg2"],
                                             ["cmd2", "arg3", "arg4"]]
                   where cmd1 will be run before cmd2
      flashrom_cmd=command to flash via flashrom
      futility_cmd=command to flash via futility
  """
  dut_control_on = []
  dut_control_off = []
  if servo.is_c2d2:
    dut_control_on.append(['ap_flash_select:on'])
    dut_control_on.append(['spi2_vref:pp3300'])
    dut_control_off.append(['spi2_vref:off'])
    dut_control_off.append(['ap_flash_select:off'])
    programmer = 'raiden_debug_spi:serial=%s' % servo.serial
  elif servo.is_ccd:
    programmer = 'raiden_debug_spi:target=AP,serial=%s ' % servo.serial
  else:
    raise Exception('%s not supported' % servo.version)

  flashrom_cmd = ['flashrom', '-p', programmer, '-w']
  futility_cmd = ['futility', 'update', '-p', programmer, '-i']

  return [dut_control_on, dut_control_off, flashrom_cmd, futility_cmd]
