# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wilco configs."""

from __future__ import print_function
from chromite.lib import cros_logging as logging

# TODO(b/143241417): Use futility anytime flashing over ssh to avoid failures.
use_futility_ssh = True


def is_fast_required(_use_futility, servo):
  """Returns true if --fast is necessary to flash successfully.

  The configurations in this function consistently fail on the verify step,
  adding --fast removes verification of the flash and allows these configs to
  flash properly. Meant to be a temporary hack until b/143240576 is fixed.

  Args:
    _use_futility (bool): True if futility is to be used, False if
      flashrom.
    servo (servo_lib.Servo): The servo connected to the target DUT.

  Returns:
    bool: True if fast is necessary, False otherwise.
  """
  if servo.is_micro:
    # servo_v4_with_servo_micro or servo_micro
    return True
  return False


def get_commands(servo):
  """Get specific flash commands for wilco

  Each board needs specific commands including the voltage for Vref, to turn
  on and turn off the SPI flash. The get_*_commands() functions provide a
  board-specific set of commands for these tasks. The voltage for this board
  needs to be set to 3.3 V.

  wilco care and feeding doc only lists commands for servo v2 and servo micro
  TODO: support 4 byte addressing?
  From wilco care and feeding doc:
  4 Byte Addressing

  As of 20-Aug-2019 flashrom at ToT cannot flash the 32 MB flashes that
  drallion uses. If you see an error about “4 byte addressing” run the
  following commands to get a useable flashrom

  cd ~/trunk/src/third_party/flashrom/
  git co ff7778ab25d0b343e781cffc0e45f329ee69a5a8~1
  cros_workon --host start flashrom
  sudo emerge flashrom

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
  if servo.is_v2:
    dut_control_on.append(['spi2_vref:pp3300', 'spi2_buf_en:on',
                           'spi2_buf_on_flex_en:on',
                           'cold_reset:on'])
    dut_control_off.append(['spi2_vref:off', 'spi2_buf_en:off',
                            'spi2_buf_on_flex_en:off',
                            'cold_reset:off'])
    programmer = 'ft2232_spi:type=servo-v2,serial=%s' % servo.serial
  elif servo.is_micro:
    dut_control_on.append(['spi2_vref:pp3300', 'spi2_buf_en:on',
                           'cold_reset:on'])
    dut_control_off.append(['spi2_vref:off', 'spi2_buf_en:off',
                            'cold_reset:off'])
    programmer = 'raiden_debug_spi:serial=%s' % servo.serial
  elif servo.is_ccd:
    # According to wilco care and feeding doc there is
    # NO support for CCD on wilco so this will not work.
    logging.error('wilco devices do not support ccd, cannot flash')
    logging.info('Please use a different servo with wilco devices')
    raise Exception('%s not accepted' % servo.version)
  else:
    raise Exception('%s not supported' % servo.version)

  flashrom_cmd = ['flashrom', '-p', programmer, '-w']
  futility_cmd = ['futility', 'update', '-p', programmer, '-i']

  return [dut_control_on, dut_control_off, flashrom_cmd, futility_cmd]
