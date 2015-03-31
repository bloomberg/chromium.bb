# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for integration VM tests for CLI commands.

This module contains the basic functionalities for setting up a VM and testing
the CLI commands.
"""

from __future__ import print_function

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import remote_access
from chromite.lib import vm


class Error(Exception):
  """Base exception for CLI command VM tests."""


class CommandError(Error):
  """Raised when running a command test."""


def _PrintCommandLog(command, content):
  """Print out the log |content| for |command|."""
  if content:
    logging.info('\n----------- Start of %s log -----------\n%s\n'
                 '-----------  End of %s log  -----------',
                 command, content.rstrip(), command)


def TestCommandDecorator(command_name):
  """Decorator that runs the command test function."""

  def Decorator(test_function):
    """Inner decorator that actually wraps the function."""

    def Wrapper(command_test):
      """Wrapper for the test function."""
      command = cros_build_lib.CmdToStr(command_test.BuildCommand(command_name))
      logging.info('Running test for %s.', command)
      try:
        test_function(command_test)
        logging.info('Test for %s passed.', command)
      except CommandError as e:
        _PrintCommandLog(command, str(e))
        raise Error('Test for %s failed.' % command)

    return Wrapper

  return Decorator


class CommandVMTest(object):
  """Base class for CLI command VM tests.

  This class provides the abstract interface for testing CLI commands on a VM.
  The sub-class must define the BuildCommand method in order to be usable. And
  the test functions must use the TestCommandDecorator decorator.
  """

  def __init__(self, board, image_path):
    """Initializes CommandVMTest.

    Args:
      board: Board for the VM to run tests.
      image_path: Path to the image for the VM to run tests.
    """
    self.board = board
    self.image_path = image_path
    self.working_image_path = None
    self.vm = None

  def BuildCommand(self, command, device=None, pos_args=None, opt_args=None):
    """Builds a CLI command.

    Args:
      command: The sub-command to build on (e.g. 'flash', 'deploy').
      device: The device's address for the command.
      pos_args: A list of positional arguments for the command.
      opt_args: A list of optional arguments for the command.
    """
    raise NotImplementedError()

  def SetUp(self):
    """Creates and starts the VM instance for testing."""
    try:
      logging.info('Setting up the VM for testing.')
      self.working_image_path = vm.CreateVMImage(
          image=self.image_path, board=self.board, updatable=True)
      self.vm = vm.VMInstance(self.working_image_path)
      self.vm.Start()
      logging.info('The VM has been successfully set up. Ready to run tests.')
    except vm.VMError as e:
      raise Error('Failed to set up the VM for testing: %s' % e)

  def TearDown(self):
    """Stops the VM instance after testing."""
    try:
      logging.info('Stopping the VM.')
      if self.vm:
        self.vm.Stop()
      logging.info('The VM has been stopped.')
    except vm.VMStopError as e:
      logging.warning('Failed to stop the VM: %s', e)

  @TestCommandDecorator('flash')
  def TestFlash(self):
    """Tests the flash command."""
    # We explicitly disable reboot after the update because VMs sometimes do
    # not come back after reboot. The flash command does not need to verify
    # the integrity of the updated image. We have AU tests for that.
    cmd = self.BuildCommand('flash', device=self.vm.device_addr,
                            pos_args=['latest'],
                            opt_args=['--no-wipe', '--no-reboot'])

    logging.info('Test to flash the VM device with the latest image.')
    result = cros_build_lib.RunCommand(cmd, capture_output=True,
                                       error_code_ok=True)
    if result.returncode:
      logging.error('Failed to flash the VM device.')
      raise CommandError(result.error)

  @TestCommandDecorator('deploy')
  def TestDeploy(self):
    """Tests the deploy command."""
    packages = ['dev-python/cherrypy', 'app-portage/portage-utils']
    # Set the installation root to /usr/local so that the command does not
    # attempt to remount rootfs (which leads to VM reboot).
    cmd = self.BuildCommand('deploy', device=self.vm.device_addr,
                            pos_args=packages, opt_args=['--root=/usr/local'])

    logging.info('Test to uninstall packages on the VM device.')
    result = cros_build_lib.RunCommand(cmd + ['--unmerge'],
                                       capture_output=True,
                                       error_code_ok=True)
    if result.returncode:
      logging.error('Failed to uninstall packages on the VM device.')
      raise CommandError(result.error)

    logging.info('Test to install packages on the VM device.')
    result = cros_build_lib.RunCommand(cmd, capture_output=True,
                                       error_code_ok=True)
    if result.returncode:
      logging.error('Failed to install packages on the VM device.')
      raise CommandError(result.error)

    # Verify that the packages are installed.
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.LOCALHOST, port=self.vm.port) as device:
      try:
        device.RunCommand(['python', '-c', '"import cherrypy"'])
        device.RunCommand(['qmerge', '-h'])
      except remote_access.SSHConnectionError as e:
        logging.error('Unable to connect to the VM to verify packages: %s', e)
        raise CommandError()
      except cros_build_lib.RunCommandError as e:
        logging.error('Unable to verify packages installed on VM: %s', e)
        raise CommandError()

  def RunTests(self):
    """Calls the test functions."""
    self.TestFlash()
    self.TestDeploy()

  def Run(self):
    """Runs the tests."""
    try:
      self.SetUp()
      self.RunTests()
      logging.info('All tests completed successfully.')
    except Exception as e:
      logging.error(e)
    finally:
      self.TearDown()
