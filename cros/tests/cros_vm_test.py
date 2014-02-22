#!/usr/bin/python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration test to test the basic functionality of cros flash/deploy.

This module contains a test that runs some sanity integration tests against
a VM. It starts a VM and runs `cros flash` to update the VM, and then
`cros deploy` to install packages to the VM.
"""

import logging

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import remote_access
from chromite.lib import vm


def _FormatLogContent(name, content):
  """Helper function for printing out complete log content."""
  return '\n'.join((
      '----------- Start of %s log -----------\n' % name,
      content,
      '-----------  End of %s log -----------\n' % name,
  ))


class TestError(Exception):
  """Raised on any error during testing."""


class CrosCommandTest(object):
  """Wrapper cros command tests."""

  def __init__(self, board, image_path):
    """Initialize CrosTest.

    Args:
      board: Board of the image under test.
      image_path: Path to the VM image to test.
    """
    self.board = board
    self.image_path = image_path
    self.working_image_path = None
    self.vm = None

  def Cleanup(self):
    """Cleans up any state at the end of the test."""
    try:
      if self.vm:
        self.vm.Stop()
        self.vm = None

    except Exception:
      logging.warning('Received error during cleanup', exc_info=True)

  def Setup(self):
    """Creates and/or starts the VM instance."""
    logging.info('Setting up image %s for vm testing.', self.image_path)
    self.working_image_path = vm.CreateVMImage(
        image=self.image_path, board=self.board, updatable=True,
        dest_dir=self.tempdir)
    logging.info('Using VM image at %s.', self.working_image_path)

    self.vm = vm.VMInstance(self.working_image_path, tempdir=self.tempdir)
    logging.info('Starting the vm on port %d.', self.vm.port)
    self.vm.Start()

  def TestCrosFlash(self):
    """Tests that we can flash the device with the latest image."""
    logging.info('Testing flashing VM with cros flash...')
    device_addr = 'ssh://%s:%d' % (remote_access.LOCALHOST, self.vm.port)
    # We explicitly disable reboot after the update because VMs
    # sometimes do not come back after reboot. `cros flash` does not
    # need to verify the integrity of the updated image. We have AU
    # tests for that.
    cmd = ['cros', 'flash', '--debug', '--no-wipe', '--no-reboot',
           device_addr, 'latest']
    result = cros_build_lib.RunCommand(cmd, capture_output=True,
                                       error_code_ok=True)

    if result.returncode != 0:
      logging.error('Failed updating VM with image. Printing logs...\n%s',
                    _FormatLogContent('Cros Flash', result.error))
      raise TestError('cros flash test failed')

    logging.info('cros flash test completed successfully')

  def TestCrosDeploy(self):
    """Tests that we can deploy packages to the device."""
    logging.info('Testing installing/uninstalling pacakges with cros deploy...')
    device_addr = 'ssh://%s:%d' % (remote_access.LOCALHOST, self.vm.port)
    packages = ['dev-python/cherrypy', 'app-portage/portage-utils']
    # Set the installation root to /usr/local so that `cros deploy`
    # does not attempt to remount rootfs (which leads to VM reboot).
    cmd_base = ['cros', 'deploy', '--debug', '--root=/usr/local', device_addr]

    # Unmerge the packages.
    cmd = cmd_base + ['--unmerge'] + packages
    result = cros_build_lib.RunCommand(cmd, capture_output=True,
                                       enter_chroot=True,
                                       error_code_ok=True)
    if result.returncode != 0:
      logging.error('Failed unmerging packages from VM. Printing logs...\n%s',
                    _FormatLogContent('Cros Deploy', result.error))
      raise TestError('cros deploy unmerge test failed.')

    # Re-install the packages.
    cmd = cmd_base + packages
    result = cros_build_lib.RunCommand(cmd, capture_output=True,
                                       enter_chroot=True,
                                       error_code_ok=True)
    if result.returncode != 0:
      logging.error('Failed deploying packages to VM. Printing logs...\n%s',
                    _FormatLogContent('Cros Deploy', result.error))
      raise TestError('cros deploy test failed.')

    # Verify that the packages are installed.
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.LOCALHOST, port=self.vm.port,
        base_dir=self.tempdir) as device:
      try:
        device.RunCommand(['python', '-c', '"import cherrypy"'])
        device.RunCommand(['qmerge', '-h'])
      except remote_access.SSHConnectionError as e:
        logging.error('Unable to connect to VM after deploy packages')
        raise TestError('Unable to connect to VM: %s' % str(e))
      except cros_build_lib.RunCommandError as e:
        logging.error('Could not verify packages are installed')
        raise TestError('Error verifying packages are installed: %s' % str(e))

    logging.info('cros deploy test completed successfully')

  @osutils.TempDirDecorator
  def Run(self):
    """Runs the tests."""
    try:
      self.Setup()
      self.TestCrosFlash()
      self.TestCrosDeploy()
    finally:
      self.Cleanup()

    logging.info('All tests in cros_vm_test passed.')


def _ParseArguments(argv):
  """Parses command line arguments."""
  parser = commandline.ArgumentParser(caching=True)
  parser.add_argument(
      'board', help='Board of the image.')
  parser.add_argument(
      'image_path', help='Path to the image to test.')

  return parser.parse_args(argv)


def main(argv):
  """Main function of the script."""
  options = _ParseArguments(argv)
  options.Freeze()

  logging.info('Starting cros_vm_test...')
  test = CrosCommandTest(options.board, options.image_path)
  test.Run()
