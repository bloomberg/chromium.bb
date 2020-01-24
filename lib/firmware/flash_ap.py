#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to detect flashing infrastructure and flash ap firmware.

This script automatically detects the flashing infrastructure and uses that to
flash AP fw to the DUT. First it checks for the environment variable $IP, then
tries flash via ssh to that address if one is present. If not it looks up what
servo version is connected and uses that to flash the AP firmware. Right now
this script only works with octopus, grunt, wilco, and hatch devices but will
be extended to support more in the future.
"""

from __future__ import print_function

import collections
import importlib
import os
import shutil
import sys
import tempfile
import time

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging

# Data class for servo serialname and version info. Both fields are strings.
ServoInfo = collections.namedtuple('ServoInfo', ('serial', 'version'))


class Error(Exception):
  """Base module error class."""


class MissingBuildTargetCommandsError(Error):
  """Error thrown when board-specific functionality can't be imported."""


class ServoInfoError(Error):
  """Error when fetching servo info."""


def _dut_control_value(dut_ctrl_out):
  """Helper function to return meaningful part of dut-control command output

  Args:
    dut_ctrl_out (string): output from dut-control command

  Returns:
    string: substring of output from ':' to the end with any whitespace
      removed
  """

  return dut_ctrl_out[dut_ctrl_out.find(':') + 1:].strip()


def _build_ssh_cmds(futility, ip, path, tmp_file_name, fast, verbose):
  """Helper function to build commands for flashing over ssh

  Args:
    futility (bool): if True then flash with futility, otherwise flash
      with flashrom.
    ip (string): ip address of dut to flash.
    path (string): path to BIOS image to be flashed.
    tmp_file_name (string): name of tempfile with copy of testing_rsa
      keys.
    fast (bool): if True pass through --fast (-n for flashrom) to
      flashing command.
    verbose (bool): if True set -v flag in flash command.

  Returns:
    scp_cmd ([string]):
    flash_cmd ([string]):
  """
  ssh_parameters = [
      '-o', 'UserKnownHostsFile=/dev/null', '-o', 'StrictHostKeyChecking=no',
      '-o', 'CheckHostIP=no'
  ]
  tmp = '/tmp'
  hostname = 'root@%s' % ip
  scp_cmd = (['scp', '-i', tmp_file_name] + ssh_parameters +
             [path, '%s:%s' % (hostname, tmp)])
  flash_cmd = ['ssh', hostname, '-i', tmp_file_name] + ssh_parameters
  if futility:
    flash_cmd += [
        'futility', 'update', '-p', 'host', '-i',
        os.path.join(tmp, os.path.basename(path))
    ]
    if fast:
      flash_cmd += ['--fast']
    if verbose:
      flash_cmd += ['-v']
  else:
    flash_cmd += [
        'flashrom', '-p', 'host', '-w',
        os.path.join(tmp, os.path.basename(path))
    ]
    if fast:
      flash_cmd += ['-n']
    if verbose:
      flash_cmd += ['-V']
  flash_cmd += ['&& reboot']
  return scp_cmd, flash_cmd


def _ssh_flash(futility, path, verbose, ip, fast):
  """This function flashes AP firmware over ssh.

  Tries to ssh to ip address once. If the ssh connection is successful the
  file to be flashed is copied over to the DUT then flashed using either
  futility or flashrom.

  Args:
    futility (bool): if True then flash with futility, otherwise flash
      with flashrom.
    path (str): path to the BIOS image to be flashed.
    verbose (bool): if True to set -v flag in flash command and
      print other debug info, if False do nothing.
    ip (str): ip address of dut to flash.
    fast (bool): if True pass through --fast (-n for flashrom) to
      flashing command.

  Returns:
    bool: True on success, False on fail
  """
  logging.info('connecting to: %s\n', ip)
  id_filename = '/mnt/host/source/chromite/ssh_keys/testing_rsa'
  tmpfile = tempfile.NamedTemporaryFile()
  shutil.copy(id_filename, tmpfile.name)

  scp_cmd, flash_cmd = _build_ssh_cmds(futility, ip, path, tmpfile.name, fast,
                                       verbose)
  try:
    cros_build_lib.run(scp_cmd, print_cmd=verbose, check=True)
  except cros_build_lib.RunCommandError:
    logging.error('Could not copy image to dut.')
    return False

  logging.info('Flashing now, may take several minutes.')
  try:
    cros_build_lib.run(flash_cmd, print_cmd=verbose, check=True)
  except cros_build_lib.RunCommandError:
    logging.error('Flashing failed.')
    return False

  return True


def _flash(dut_cmd_on, dut_cmd_off, flash_cmd, verbose):
  """Runs subprocesses for setting dut controls and flashing the AP fw.

  Args:
    dut_cmd_on ([[str]]): 2d array of dut-control commands
      in the form [['dut-control', 'cmd1', 'cmd2'...],
      ['dut-control', 'cmd3'...]]
      that get executed before the flashing.
    dut_cmd_off ([[str]]): 2d array of dut-control commands
      in the same form that get executed after flashing.
    flash_cmd ([str]): array containing all arguments for
      the flash command.
    verbose (bool): if True then print out the various
      commands before running them.

  Returns:
    bool: True if flash was successful, otherwise False.
  """
  success = True
  try:
    # Dut on command runs.
    for cmd in dut_cmd_on:
      cros_build_lib.run(cmd, print_cmd=verbose, check=True)
    # Need to wait for SPI chip power to stabilize (for some designs)
    time.sleep(1)
    # Run the flash command.
    cros_build_lib.run(flash_cmd, print_cmd=verbose, check=True)
  except cros_build_lib.RunCommandError as e:
    logging.error('Flashing failed with output:\n%s', e.output)
    success = False
  finally:
    # Run the dut off commands to clean up state if possible.
    try:
      for cmd in dut_cmd_off:
        cros_build_lib.run(cmd, print_cmd=verbose, check=True)
    except cros_build_lib.CalledProcessError as e:
      logging.error('Dut cmd off failed with output:\n%s', e.output)
      success = False

  return success


def _get_servo_info(dut_control):
  """Get version and serialname of connected servo.

  This function returns the current version of the
  servo device connected to the host and the serialname
  of that device, throws error if no device or the
  device is not supported.

  Args:
    dut_control ([str]): either just ['dut-control']
      or ['dut-control', '--port=$PORT'] if the
      port being used is not 9999.

  Returns:
    SerialInfo: The servo serialname and version.
  """
  try:
    out = cros_build_lib.run(dut_control + ['servo_type'], encoding='utf-8')
  except cros_build_lib.RunCommandError:
    raise ServoInfoError(
        'Could not establish servo connection. Verify servod is running in '
        'the background, and the servo is properly connected.')

  servo_version = _dut_control_value(out)
  # Get the serial number.
  sn_ctl = 'serialname'
  if servo_version == 'servo_v4_with_servo_micro':
    sn_ctl = 'servo_micro_serialname'
  elif servo_version == 'servo_v4_with_ccd_cr50':
    sn_ctl = 'ccd_serialname'
  elif servo_version not in ('servo_v2', 'ccd_cr50', 'servo_micro', 'c2d2'):
    raise ServoInfoError('Servo version: %s not recognized verify connection '
                         'and port number' % servo_version)

  serial_out = cros_build_lib.run(dut_control + [sn_ctl], encoding='utf-8')
  serial = _dut_control_value(serial_out)
  return ServoInfo(serial=serial, version=servo_version)


# TODO: Split out to actual arguments rather than an argparse namespace.
def deploy(opts):
  module_name = 'chromite.lib.firmware.flash_ap_commands.%s' % opts.board
  try:
    module = importlib.import_module(module_name)
  except ImportError:
    raise MissingBuildTargetCommandsError(
        '%s not valid or supported. Please verify the build target name and '
        'try again.' % opts.board)

  ip = os.getenv('IP')
  flashrom = opts.flashrom
  fast = opts.fast
  if ip is not None:
    logging.info('Attempting to flash via ssh.')
    # TODO(b/143241417): Can't use flashrom over ssh on wilco.
    if (hasattr(module, 'use_futility_ssh') and module.use_futility_ssh and
        opts.flashrom):
      logging.warning('WARNING: flashing with flashrom over ssh on this device'
                      ' fails consistently, flashing with futility instead.')
      flashrom = False
    if _ssh_flash(not flashrom, opts.image, opts.verbose, ip, fast):
      logging.info('ssh flash successful. Exiting flash_ap')
      return 0
    logging.info('ssh failed, attempting to flash via servo connection.')
  # Dut_ctrl string specifies the port if it is not 9999
  dut_ctrl = ['dut-control']
  if opts.port != 9999:
    dut_ctrl.append('--port=%d' % opts.port)
  servo_info = _get_servo_info(dut_ctrl)

  # TODO(b/143240576): Fast mode is sometimes necessary to flash successfully.
  if module.is_fast_required(not opts.flashrom,
                             servo_info.version) and not opts.fast:
    logging.warning('WARNING: there is a known error with the board and servo '
                    'type being used, enabling --fast to bypass this problem.')
    fast = True
  if hasattr(module, '__use_flashrom__'):
    # Futility needs VBoot to flash so boards without functioning VBoot
    # can set this attribute to force the use of flashrom.
    flashrom = True
  dut_on, dut_off, flashrom_cmd, futility_cmd = module.get_commands(
      servo_info.version, servo_info.serial)
  dut_ctrl_on = [dut_ctrl + x for x in dut_on]
  dut_ctrl_off = [dut_ctrl + x for x in dut_off]
  flashrom_cmd += [opts.image]
  futility_cmd += [opts.image]
  futility_cmd += ['--force', '--wp=0']
  if fast:
    futility_cmd += ['--fast']
    flashrom_cmd += ['-n']
  if opts.verbose:
    flashrom_cmd += ['-V']
    futility_cmd += ['-v']
  if not flashrom:
    if _flash(dut_ctrl_on, dut_ctrl_off, futility_cmd, opts.verbose):
      logging.info('SUCCESS. Exiting flash_ap.')
    else:
      logging.error('Unable to complete flash, verify servo connection is '
                    'correct and servod is running in the background.')
  else:
    if _flash(dut_ctrl_on, dut_ctrl_off, flashrom_cmd, opts.verbose):
      logging.info('SUCCESS. Exiting flash_ap.')
    else:
      logging.error('Unable to complete flash, verify servo connection '
                    'is correct and servod is running in the background.')


def get_parser():
  """Helper function to get parser with all arguments added

  Returns:
    commandline.ArgumentParser: object used to check command line arguments
  """
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('image', type='path', help='/path/to/BIOS_image.bin')
  parser.add_argument(
      '-b',
      '--board',
      '--build-target',
      dest='board',
      type=str,
      help='The build target (board) name.')
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help='Increase output verbosity for .')
  parser.add_argument(
      '--port',
      type=int,
      default=os.getenv('SERVO_PORT', 9999),
      help='Port number being listened to by servo device. '
      'Defaults to $SERVO_PORT or 9999 when not provided.')
  parser.add_argument(
      '--flashrom',
      action='store_true',
      help='Use flashrom to flash instead of futility.')
  parser.add_argument(
      '--fast',
      action='store_true',
      help='Speed up flashing by not validating flash.')
  return parser


def parse_args(argv):
  """Parse the arguments."""
  parser = get_parser()
  opts = parser.parse_args(argv)
  if not os.path.exists(opts.image):
    parser.error('%s does not exist, verify the path of your build and try '
                 'again.' % opts.image)

  opts.Freeze()
  return opts


def main(argv):
  """Main function for flashing ap firmware.

  Detects flashing infrastructure then fetches commands from get_*_commands
  and flashes accordingly.
  """
  opts = parse_args(argv)
  try:
    # TODO: Finish converting return codes to errors and dump the return.
    return deploy(opts)
  except Error as e:
    logging.error(e)
    return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
