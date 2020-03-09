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

import importlib
import os
import shutil
import sys
import tempfile
import time

from chromite.lib import build_target_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib.firmware import servo_lib


class Error(Exception):
  """Base module error class."""


class DeployFailed(Error):
  """Error raised when deploy fails."""


class DutConnectionError(Error):
  """Error when fetching data from a dut."""


class MissingBuildTargetCommandsError(Error):
  """Error thrown when board-specific functionality can't be imported."""


class DutControl(object):
  """Wrapper for dut_control calls."""

  def __init__(self, port):
    self._base_cmd = ['dut-control']
    if port:
      self._base_cmd.append('--port=%s' % port)

  def get_value(self, arg):
    """Get the value of |arg| from dut_control."""
    try:
      result = cros_build_lib.run(
          self._base_cmd + [arg], stdout=True, encoding='utf-8')
    except cros_build_lib.CalledProcessError as e:
      logging.debug('dut-control error: %s', str(e))
      raise DutConnectionError(
          'Could not establish servo connection. Verify servod is running in '
          'the background, and the servo is properly connected.')

    # Return value from the "key:value" output.
    return result.stdout.partition(':')[2].strip()

  def run(self, cmd_fragment, verbose=False, dryrun=False):
    """Run a dut_control command.

    Args:
      cmd_fragment (list[str]): The dut_control command to run.
      verbose (bool): Whether to print the command before it's run.
      dryrun (bool): Whether to actually execute the command or just print it.
    """
    cros_build_lib.run(
        self._base_cmd + cmd_fragment, print_cmd=verbose, dryrun=dryrun)

  def run_all(self, cmd_fragments, verbose=False, dryrun=False):
    """Run multiple dut_control commands in the order given.

    Args:
      cmd_fragments (list[list[str]]): The dut_control commands to run.
      verbose (bool): Whether to print the commands as they are run.
      dryrun (bool): Whether to actually execute the command or just print it.
    """
    for cmd in cmd_fragments:
      self.run(cmd, verbose=verbose, dryrun=dryrun)


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


def _ssh_flash(futility, path, verbose, ip, fast, dryrun):
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
    dryrun (bool): Whether to actually execute the commands or just print
      the commands that would have been run.

  Returns:
    bool: True on success, False on failure.
  """
  logging.info('connecting to: %s\n', ip)
  id_filename = '/mnt/host/source/chromite/ssh_keys/testing_rsa'
  tmpfile = tempfile.NamedTemporaryFile()
  shutil.copy(id_filename, tmpfile.name)

  scp_cmd, flash_cmd = _build_ssh_cmds(futility, ip, path, tmpfile.name, fast,
                                       verbose)
  try:
    cros_build_lib.run(scp_cmd, print_cmd=verbose, check=True, dryrun=dryrun)
  except cros_build_lib.CalledProcessError:
    logging.error('Could not copy image to dut.')
    return False

  logging.info('Flashing now, may take several minutes.')
  try:
    cros_build_lib.run(flash_cmd, print_cmd=verbose, check=True, dryrun=dryrun)
  except cros_build_lib.CalledProcessError:
    logging.error('Flashing failed.')
    return False

  return True


def _flash(dut_ctl, dut_cmd_on, dut_cmd_off, flash_cmd, verbose, dryrun):
  """Runs subprocesses for setting dut controls and flashing the AP fw.

  Args:
    dut_ctl (DutControl): The dut_control command runner instance.
    dut_cmd_on ([[str]]): 2d array of dut-control commands
      in the form [['dut-control', 'cmd1', 'cmd2'...],
      ['dut-control', 'cmd3'...]]
      that get executed before the flashing.
    dut_cmd_off ([[str]]): 2d array of dut-control commands
      in the same form that get executed after flashing.
    flash_cmd ([str]): array containing all arguments for
      the flash command. Run as root user.
    verbose (bool): if True then print out the various
      commands before running them.
    dryrun (bool): Whether to actually execute the flash or just print the
      commands that would have been run.

  Returns:
    bool: True if flash was successful, otherwise False.
  """
  success = True
  try:
    # Dut on command runs.
    dut_ctl.run_all(dut_cmd_on, verbose=verbose, dryrun=dryrun)

    # Need to wait for SPI chip power to stabilize (for some designs)
    time.sleep(1)

    # Run the flash command.
    cros_build_lib.sudo_run(flash_cmd, print_cmd=verbose, dryrun=dryrun)
  except cros_build_lib.CalledProcessError:
    logging.error('Flashing failed, see output above for more info.')
    success = False
  finally:
    # Run the dut off commands to clean up state if possible.
    try:
      dut_ctl.run_all(dut_cmd_off, verbose=verbose, dryrun=dryrun)
    except cros_build_lib.CalledProcessError:
      logging.error('Dut cmd off failed, see output above for more info.')
      success = False

  return success


def deploy(build_target,
           image,
           device=None,
           flashrom=False,
           fast=False,
           port=None,
           verbose=False,
           dryrun=False):
  """Deploy an AP FW image to a device.

  Args:
    build_target (build_target_lib.BuildTarget): The DUT build target.
    image (str): The image path.
    device (commandline.Device): The device to be used. Temporarily optional.
    flashrom (bool): Whether to use flashrom or futility.
    fast (bool): Whether to do a fast (no verification) flash.
    port (int|None): The servo port.
    verbose (bool): Whether to use verbose output for flash commands.
    dryrun (bool): Whether to actually execute the deployment or just print the
      operations that would have been performed.
  """
  ip = None
  if device:
    port = device.port
    if device.scheme == commandline.DEVICE_SCHEME_SSH:
      hostname = device.hostname
      ip = '%s:%s' % (hostname, port) if port else hostname
  else:
    ip = os.getenv('IP')

  module_name = (
      'chromite.lib.firmware.ap_firmware_config.%s' % build_target.name)
  try:
    module = importlib.import_module(module_name)
  except ImportError:
    raise MissingBuildTargetCommandsError(
        '%s not valid or supported. Please verify the build target name and '
        'try again.' % build_target.name)

  if ip:
    _deploy_ssh(image, module, flashrom, fast, verbose, ip, dryrun)
  else:
    _deploy_servo(image, module, flashrom, fast, verbose, port, dryrun)


def _deploy_servo(image, module, flashrom, fast, verbose, port, dryrun):
  """Deploy to a servo connection.

  Args:
    image (str): Path to the image to flash.
    module: The config module.
    flashrom (bool): Whether to use flashrom or futility.
    fast (bool): Whether to do a fast (no verification) flash.
    verbose (bool): Whether to use verbose output for flash commands.
    port (int|None): The servo port.
    dryrun (bool): Whether to actually execute the deployment or just print the
      operations that would have been performed.
  """
  logging.info('Attempting to flash via servo.')
  dut_ctl = DutControl(port)
  servo = servo_lib.get(dut_ctl)
  # TODO(b/143240576): Fast mode is sometimes necessary to flash successfully.
  if not fast and module.is_fast_required(not flashrom, servo):
    logging.notice('There is a known error with the board and servo type being '
                   'used, enabling --fast to bypass this problem.')
    fast = True
  if hasattr(module, '__use_flashrom__') and module.__use_flashrom__:
    # Futility needs VBoot to flash so boards without functioning VBoot
    # can set this attribute to True to force the use of flashrom.
    flashrom = True
  dut_on, dut_off, flashrom_cmd, futility_cmd = module.get_commands(servo)
  flashrom_cmd += [image]
  futility_cmd += [image]
  futility_cmd += ['--force', '--wp=0']
  if fast:
    futility_cmd += ['--fast']
    flashrom_cmd += ['-n']
  if verbose:
    flashrom_cmd += ['-V']
    futility_cmd += ['-v']
  flash_cmd = flashrom_cmd if flashrom else futility_cmd
  if _flash(dut_ctl, dut_on, dut_off, flash_cmd, verbose, dryrun):
    logging.info('SUCCESS. Exiting flash_ap.')
  else:
    logging.error('Unable to complete flash, verify servo connection '
                  'is correct and servod is running in the background.')


def _deploy_ssh(image, module, flashrom, fast, verbose, ip, dryrun):
  """Deploy to a servo connection.

  Args:
    image (str): Path to the image to flash.
    module: The config module.
    flashrom (bool): Whether to use flashrom or futility.
    fast (bool): Whether to do a fast (no verification) flash.
    verbose (bool): Whether to use verbose output for flash commands.
    ip (str): The DUT ip address.
    dryrun (bool): Whether to execute the deployment or just print the
      commands that would have been executed.
  """
  logging.info('Attempting to flash via ssh.')
  # TODO(b/143241417): Can't use flashrom over ssh on wilco.
  if (hasattr(module, 'use_futility_ssh') and module.use_futility_ssh and
      flashrom):
    logging.warning('Flashing with flashrom over ssh on this device fails '
                    'consistently, flashing with futility instead.')
    flashrom = False
  if _ssh_flash(not flashrom, image, verbose, ip, fast, dryrun):
    logging.info('ssh flash successful. Exiting flash_ap')
  else:
    raise DeployFailed('ssh failed, try using a servo connection instead.')


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
      default=os.getenv('SERVO_PORT'),
      help='Port number being listened to by servo device. '
           'Defaults to $SERVO_PORT if set when not provided, otherwise allows '
           'dut_control to use its default (9999).')
  parser.add_argument(
      '--flashrom',
      action='store_true',
      help='Use flashrom to flash instead of futility.')
  parser.add_argument(
      '--fast',
      action='store_true',
      help='Speed up flashing by not validating flash.')
  parser.add_argument(
      '-n',
      '--dry-run',
      action='store_true',
      help='Perform a dry run, printing the relevant commands rather than '
           'executing them.')
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
  logging.warning('This entry point is now deprecated in favor of '
                  '`cros flash-ap`! Please use that command instead.')
  cros_build_lib.AssertInsideChroot()

  opts = parse_args(argv)
  try:
    deploy(
        build_target_lib.BuildTarget(opts.board),
        opts.image,
        flashrom=opts.flashrom,
        fast=opts.fast,
        port=opts.port,
        verbose=opts.verbose,
        dryrun=opts.dry_run)
  except Error as e:
    cros_build_lib.Die(e)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
