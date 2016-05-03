# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing functions to execute auto-update on a remote device.

TODO(xixuan): Make this lib support other update logics, including:
  auto-update CrOS images for DUT
  beaglebones for servo
  stage images to servo usb
  install custom CrOS images for chaos lab
  install firmware images with FAFT
  install android/brillo

Currently, this lib only supports to update ChromiumOS image to a remote test
device by cros flash. The workflow include 4 steps:
  ----Precheck---
  * Pre-check payload's existence before auto-update.
  * Pre-check if the device can run its devserver.

  ----Tranfer----
  * Transfer devserver package at first.
  * Transfer rootfs update files if rootfs update is required.
  * Transfer stateful update files if stateful update is required.

  ----Auto-Update---
  * Do rootfs partition update if it's required.
  * Do stateful partition update if it's required.
  * Do reboot for device if it's required.

  ----Verify----
  * Do verification if it's required.
  * Disable rootfs verification in device if it's required.

This lib does not support:
  * connect to a remove device.
  * stage image/payload.
  * retry any atomic functions, like transfer/update methods.
"""

from __future__ import print_function

import cStringIO
import os
import shutil
import tempfile
import time

from chromite.cli import command
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import dev_server_wrapper as ds_wrapper
from chromite.lib import operation
from chromite.lib import osutils
from chromite.lib import path_util


# Update Status for remote device.
UPDATE_STATUS_IDLE = 'UPDATE_STATUS_IDLE'
UPDATE_STATUS_DOWNLOADING = 'UPDATE_STATUS_DOWNLOADING'
UPDATE_STATUS_FINALIZING = 'UPDATE_STATUS_FINALIZING'
UPDATE_STATUS_UPDATED_NEED_REBOOT = 'UPDATE_STATUS_UPDATED_NEED_REBOOT'

# Error msg in loading shared libraries when running python command.
ERROR_MSG_IN_LOADING_LIB = 'python: error while loading shared libraries'


class ChromiumOSUpdateError(Exception):
  """Thrown when there is a general ChromiumOS-specific update error."""


class RootfsUpdateError(ChromiumOSUpdateError):
  """Raised when the Rootfs partition update fails."""


class StatefulUpdateError(ChromiumOSUpdateError):
  """Raised when the stateful partition update fails."""


class AutoUpdateVerifyError(ChromiumOSUpdateError):
  """Raised when verification after auto-update fails."""


class BaseUpdater(object):
  """The base updater class."""
  def __init__(self, device, payload_dir):
    self.device = device
    self.payload_dir = payload_dir


class ChromiumOSUpdater(BaseUpdater):
  """Used to update DUT with image."""
  DEVSERVER_FILENAME = 'devserver.py'
  STATEFUL_UPDATE_BIN = '/usr/bin/stateful_update'
  UPDATE_ENGINE_BIN = 'update_engine_client'
  DEVSERVER_LOG = 'target_devserver.log'
  UPDATE_ENGINE_LOG = '/var/log/update_engine.log'
  UPDATE_CHECK_INTERVAL_PROGRESSBAR = 0.5
  UPDATE_CHECK_INTERVAL_NORMAL = 10

  def __init__(self, device, payload_dir, tempdir=None,
               do_rootfs_update=True, do_stateful_update=True, reboot=True,
               disable_verification=False, clobber_stateful=False, yes=False):
    """Initialize a ChromiumOSUpdater for auto-update a chromium OS device.

    Args:
      device: the ChromiumOSDevice to be updated.
      payload_dir: the directory of payload(s).
      tempdir: the temp directory in caller, not in the device. For example,
          the tempdir for cros flash is /tmp/cros-flash****/, used to
          temporarily keep files when transferring devserver package, and
          reserve devserver and update engine logs.
      do_rootfs_update: whether to do rootfs partition update. The default is
          True.
      do_stateful_update: whether to do stateful partition update. The default
          is True.
      reboot: whether to reboot device after update. The default is True.
      disable_verification: whether to disabling rootfs verification on the
          device. The default is False.
      clobber_stateful: whether to do a clean stateful update. The default is
          False.
      yes: Assume "yes" (True) for any prompt. The default is False. However,
          it should be set as True if we want to disable all the prompts for
          auto-update.
    """
    super(ChromiumOSUpdater, self).__init__(device, payload_dir)
    if tempdir is not None:
      self.tempdir = tempdir
    else:
      self.tempdir = tempfile.mkdtemp(prefix='cros-update')

    # Update setting
    self._do_stateful_update = do_stateful_update
    self._do_rootfs_update = do_rootfs_update
    self._disable_verification = disable_verification
    self._clobber_stateful = clobber_stateful
    self._reboot = reboot
    self._yes = yes
    # Device's directories
    self.device_dev_dir = os.path.join(self.device.work_dir, 'src')
    self.device_static_dir = os.path.join(self.device.work_dir, 'static')

  def CheckPayloads(self):
    """Verify that all required payloads are in |self.payload_dir|."""
    filenames = []
    filenames += [ds_wrapper.ROOTFS_FILENAME] if self._do_rootfs_update else []
    if self._do_stateful_update:
      filenames += [ds_wrapper.STATEFUL_FILENAME]

    for fname in filenames:
      payload = os.path.join(self.payload_dir, fname)
      if not os.path.exists(payload):
        raise ChromiumOSUpdateError('Payload %s does not exist!' % payload)

  def _CheckRestoreStateful(self):
    """Check whether to restore stateful."""
    restore_stateful = False
    if not self._CanRunDevserver() and self._do_rootfs_update:
      msg = ('Cannot start devserver! The stateful partition may be '
             'corrupted.')
      prompt = 'Attempt to restore the stateful partition?'
      restore_stateful = self._yes or cros_build_lib.BooleanPrompt(
          prompt=prompt, default=False, prolog=msg)
      if not restore_stateful:
        raise ChromiumOSUpdateError('Cannot continue to perform rootfs update!')

    return restore_stateful

  def _CanRunDevserver(self):
    """We can run devserver on |device|.

    If the stateful partition is corrupted, Python or other packages
    (e.g. cherrypy) needed for rootfs update may be missing on |device|.

    This will also use `ldconfig` to update library paths on the target
    device if it looks like that's causing problems, which is necessary
    for base images.

    Returns:
      True if we can start devserver; False otherwise.
    """
    logging.info('Checking if we can run devserver on the device.')
    devserver_bin = os.path.join(self.device_dev_dir, self.DEVSERVER_FILENAME)
    devserver_check_command = ['python', devserver_bin, '--help']
    try:
      self.device.RunCommand(devserver_check_command)
    except cros_build_lib.RunCommandError as e:
      logging.warning('Cannot start devserver: %s', e)
      if ERROR_MSG_IN_LOADING_LIB in str(e):
        logging.info('Attempting to correct device library paths...')
        try:
          self.device.RunCommand(['ldconfig', '-r', '/'])
          self.device.RunCommand(devserver_check_command)
          logging.info('Library path correction successful.')
          return True
        except cros_build_lib.RunCommandError as e2:
          logging.warning('Library path correction failed: %s', e2)

      return False

    return True

  # pylint: disable=unbalanced-tuple-unpacking
  @classmethod
  def GetUpdateStatus(cls, device, keys=None):
    """Returns the status of the update engine on the |device|.

    Retrieves the status from update engine and confirms all keys are
    in the status.

    Args:
      device: A ChromiumOSDevice object.
      keys: the keys to look for in the status result (defaults to
        ['CURRENT_OP']).

    Returns:
      A list of values in the order of |keys|.
    """
    keys = keys or ['CURRENT_OP']
    result = device.RunCommand([cls.UPDATE_ENGINE_BIN, '--status'],
                               capture_output=True)
    if not result.output:
      raise Exception('Cannot get update status')

    try:
      status = cros_build_lib.LoadKeyValueFile(
          cStringIO.StringIO(result.output))
    except ValueError:
      raise ValueError('Cannot parse update status')

    values = []
    for key in keys:
      if key not in status:
        raise ValueError('Missing %s in the update engine status')

      values.append(status.get(key))

    return values

  @classmethod
  def GetRootDev(cls, device):
    """Get the current root device on |device|.

    Args:
      device: a ChromiumOSDevice object, defines whose root device we
      want to fetch.
    """
    rootdev = device.RunCommand(
        ['rootdev', '-s'], capture_output=True).output.strip()
    logging.debug('Current root device is %s', rootdev)
    return rootdev

  def SetupRootfsUpdate(self):
    """Makes sure |device| is ready for rootfs update."""
    logging.info('Checking if update engine is idle...')
    status, = self.GetUpdateStatus(self.device)
    if status == UPDATE_STATUS_UPDATED_NEED_REBOOT:
      logging.info('Device needs to reboot before updating...')
      self.device.Reboot()
      status, = self.GetUpdateStatus(self.device)

    if status != UPDATE_STATUS_IDLE:
      raise RootfsUpdateError('Update engine is not idle. Status: %s' % status)

  def TransferDevServerPackage(self):
    """Transfer devserver package to work directory of the remote device."""
    logging.info('Copying devserver package to device...')
    src_dir = os.path.join(self.tempdir, 'src')
    osutils.RmDir(src_dir, ignore_missing=True)
    shutil.copytree(
        ds_wrapper.DEVSERVER_PKG_DIR, src_dir,
        ignore=shutil.ignore_patterns('*.pyc', 'tmp*', '.*', 'static', '*~'))
    self.device.CopyToWorkDir(src_dir)

  def TransferRootfsUpdate(self):
    """Transfer files for rootfs update.

    Devserver package, and the corresponding payload are copied to the remote
    device for rootfs update.

    Args:
      payload: The payload for rootfs update.
    """
    device_payload_dir = os.path.join(self.device_static_dir, 'pregenerated')
    self.device.RunCommand(['mkdir', '-p', device_payload_dir])
    logging.info('Copying rootfs payload to device...')
    payload = os.path.join(self.payload_dir, ds_wrapper.ROOTFS_FILENAME)
    self.device.CopyToDevice(payload, device_payload_dir)

  def TransferStatefulUpdate(self):
    """Transfer files for stateful update.

    The stateful update bin and the corresponding payloads are copied to the
    target remote device for stateful update.

    Args:
      payload: The payload for stateful update.
    """
    stateful_update_bin = path_util.FromChrootPath(self.STATEFUL_UPDATE_BIN)
    self.device.CopyToWorkDir(stateful_update_bin)
    logging.info('Copying stateful payload to device...')
    payload = os.path.join(self.payload_dir, ds_wrapper.STATEFUL_FILENAME)
    self.device.CopyToWorkDir(payload)

  def RestoreStateful(self):
    """Restore stateful partition for device."""
    logging.warning('Restoring the stateful partition')
    self.RunUpdateStateful()
    self.device.Reboot()
    if self._CanRunDevserver():
      logging.info('Stateful partition restored.')
    else:
      raise ChromiumOSUpdateError('Unable to restore stateful partition.')

  def UpdateRootfs(self):
    """Update the rootfs partition of the device."""
    devserver_bin = os.path.join(self.device_dev_dir, self.DEVSERVER_FILENAME)
    ds = ds_wrapper.RemoteDevServerWrapper(
        self.device, devserver_bin, static_dir=self.device_static_dir,
        log_dir=self.device.work_dir)

    logging.info('Updating rootfs partition')
    try:
      ds.Start()
      # Use the localhost IP address to ensure that update engine
      # client can connect to the devserver.
      omaha_url = ds.GetDevServerURL(
          ip='127.0.0.1', port=ds.port, sub_dir='update/pregenerated')
      cmd = [self.UPDATE_ENGINE_BIN, '-check_for_update',
             '-omaha_url=%s' % omaha_url]
      self.device.RunCommand(cmd)

      # If we are using a progress bar, update it every 0.5s instead of 10s.
      if command.UseProgressBar():
        update_check_interval = self.UPDATE_CHECK_INTERVAL_PROGRESSBAR
        oper = operation.ProgressBarOperation()
      else:
        update_check_interval = self.UPDATE_CHECK_INTERVAL_NORMAL
        oper = None
      end_message_not_printed = True

      # Loop until update is complete.
      while True:
        op, progress = self.GetUpdateStatus(self.device,
                                            ['CURRENT_OP', 'PROGRESS'])
        logging.info('Waiting for update...status: %s at progress %s',
                     op, progress)

        if op == UPDATE_STATUS_UPDATED_NEED_REBOOT:
          logging.notice('Update completed.')
          break

        if op == UPDATE_STATUS_IDLE:
          raise RootfsUpdateError(
              'Update failed with unexpected update status: %s' % op)

        if oper is not None:
          if op == UPDATE_STATUS_DOWNLOADING:
            oper.ProgressBar(float(progress))
          elif end_message_not_printed and op == UPDATE_STATUS_FINALIZING:
            oper.Cleanup()
            logging.notice('Finalizing image.')
            end_message_not_printed = False

        time.sleep(update_check_interval)

      ds.Stop()
    except Exception as e:
      logging.error('Rootfs update failed.')
      logging.warning(ds.TailLog() or 'No devserver log is available.')
      error_msg = 'Failed to perform rootfs update: %r'
      raise RootfsUpdateError(error_msg % e)
    finally:
      ds.Stop()
      self.device.CopyFromDevice(
          ds.log_file,
          os.path.join(self.tempdir, self.DEVSERVER_LOG),
          error_code_ok=True)
      self.device.CopyFromDevice(
          self.UPDATE_ENGINE_LOG, self.tempdir, follow_symlinks=True,
          error_code_ok=True)

  def UpdateStateful(self):
    """Update the stateful partition of the device."""
    msg = 'Updating stateful partition'
    cmd = ['sh',
           os.path.join(self.device.work_dir,
                        os.path.basename(self.STATEFUL_UPDATE_BIN)),
           os.path.join(self.device.work_dir, ds_wrapper.STATEFUL_FILENAME)]

    if self._clobber_stateful:
      cmd.append('--stateful_change=clean')
      msg += ' with clobber enabled'

    logging.info('%s...', msg)
    try:
      self.device.RunCommand(cmd)
    except cros_build_lib.RunCommandError as e:
      logging.error('Stateful update failed.')
      error_msg = 'Failed to perform stateful partition update: %r'
      raise StatefulUpdateError(error_msg % e)

  def RunUpdateRootfs(self):
    """Run all processes needed by updating rootfs.

    1. Check device's status to make sure it can be updated.
    2. Copy files to remote device needed for rootfs update.
    3. Do root updating.
    """
    self.SetupRootfsUpdate()
    # Copy payload for rootfs update.
    self.TransferRootfsUpdate()
    self.UpdateRootfs()

  def RunUpdateStateful(self):
    """Run all processes needed by updating stateful.

    1. Copy files to remote device needed by stateful update.
    2. Do stateful update.
    """
    self.TransferStatefulUpdate()
    self.UpdateStateful()

  def RebootAndVerify(self):
    """Reboot and verify the remote device.

    1. Reboot the remote device. If _clobber_stateful (--clobber-stateful)
    is executed, the stateful partition is wiped, and the working directory
    on the remote device no longer exists. So, recreate the working directory
    for this remote device.
    2. Verify the remote device, by checking that whether the root device
    changed after reboot.
    """
    logging.notice('rebooting device...')
    # Record the current root device. This must be done after SetupRootfsUpdate
    # and before reboot, since SetupRootfsUpdate may reboot the device if there
    # is a pending update, which changes the root device, and reboot will
    # definitely change the root device if update successfully finishes.
    old_root_dev = self.GetRootDev(self.device)
    self.device.Reboot()
    if self._clobber_stateful:
      self.device.BaseRunCommand(['mkdir', '-p', self.device.work_dir])

    if self._do_rootfs_update:
      logging.notice('Verifying that the device has been updated...')
      new_root_dev = self.GetRootDev(self.device)
      if old_root_dev is None:
        raise AutoUpdateVerifyError(
            'Failed to locate root device before update.')

      if new_root_dev is None:
        raise AutoUpdateVerifyError(
            'Failed to locate root device after update.')

      if new_root_dev == old_root_dev:
        raise AutoUpdateVerifyError(
            'Failed to boot into the new version. Possibly there was a '
            'signing problem, or an automated rollback occurred because '
            'your new image failed to boot.')

  def RunUpdate(self):
    """Update the device with image of specific version."""
    self.TransferDevServerPackage()
    restore_stateful = self._CheckRestoreStateful()
    if restore_stateful:
      self.RestoreStateful()

    # Perform device updates.
    if self._do_rootfs_update:
      self.RunUpdateRootfs()
      logging.info('Rootfs update completed.')

    if self._do_stateful_update and not restore_stateful:
      self.RunUpdateStateful()
      logging.info('Stateful update completed.')

    if self._reboot:
      self.RebootAndVerify()

    if self._disable_verification:
      logging.info('Disabling rootfs verification on the device...')
      self.device.DisableRootfsVerification()
