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

TODO(xixuan): crbugs.com/631837, re-consider the structure of this file,
like merging check functions into one class.

Currently, this lib supports ChromiumOSFlashUpdater and ChromiumOSUpdater.

    ---------------
    | BaseUpdater | : Updater
    ---------------
           |
           |
     -------------------------
     | ChromiumOSFlashUpdater | : Chromium OS Updater by cros flash
     -------------------------
                 |
                 |
            ---------------------
            | ChromiumOSUpdater | : Chromium OS Updater by cros flash
            ---------------------   with more checks

ChromiumOSFlashUpdater includes:
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

ChromiumOSUpdater adds:
  ----Check-----
  * Check functions, including kernel/version/cgpt check.

  ----Precheck---
  * Pre-check for stateful/rootfs update/whole update.

  ----Tranfer----
  * Add @retry to all transfer functions.

  ----Verify----
  * Post-check stateful/rootfs update/whole update.
"""

from __future__ import print_function

import cStringIO
import os
import re
import shutil
import tempfile
import time

from chromite.lib import auto_update_util
from chromite.cli import command
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import dev_server_wrapper as ds_wrapper
from chromite.lib import operation
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import retry_util
from chromite.lib import timeout_util


# Update Status for remote device.
UPDATE_STATUS_IDLE = 'UPDATE_STATUS_IDLE'
UPDATE_STATUS_DOWNLOADING = 'UPDATE_STATUS_DOWNLOADING'
UPDATE_STATUS_FINALIZING = 'UPDATE_STATUS_FINALIZING'
UPDATE_STATUS_UPDATED_NEED_REBOOT = 'UPDATE_STATUS_UPDATED_NEED_REBOOT'

# Error msg in loading shared libraries when running python command.
ERROR_MSG_IN_LOADING_LIB = 'python: error while loading shared libraries'

# Max number of the times that transfer functions will be retried for.
MAX_RETRY = 5

# The timeout limit for retrying transfer tasks.
DELAY_SEC_FOR_RETRY = 5


class ChromiumOSUpdateError(Exception):
  """Thrown when there is a general ChromiumOS-specific update error."""


class PreSetupUpdateError(ChromiumOSUpdateError):
  """Raised for the rootfs/stateful update pre-setup failures."""


class RootfsUpdateError(ChromiumOSUpdateError):
  """Raised for the Rootfs partition update failures."""


class StatefulUpdateError(ChromiumOSUpdateError):
  """Raised for the stateful partition update failures."""


class AutoUpdateVerifyError(ChromiumOSUpdateError):
  """Raised for verification failures after auto-update."""


class BaseUpdater(object):
  """The base updater class."""
  def __init__(self, device, payload_dir):
    self.device = device
    self.payload_dir = payload_dir


class ChromiumOSFlashUpdater(BaseUpdater):
  """Used to update DUT with image."""
  DEVSERVER_FILENAME = 'devserver.py'
  STATEFUL_UPDATE_FILE = 'stateful_update'
  STATEFUL_UPDATE_BIN = '/usr/bin/stateful_update'
  REMOTE_STATEUL_UPDATE_PATH = '/usr/local/bin/stateful_update'
  UPDATE_ENGINE_BIN = 'update_engine_client'
  DEVSERVER_LOG = 'target_devserver.log'
  UPDATE_ENGINE_LOG = '/var/log/update_engine.log'
  PROVISION_FAILED = '/var/tmp/provision_failed'
  UPDATE_CHECK_INTERVAL_PROGRESSBAR = 0.5
  UPDATE_CHECK_INTERVAL_NORMAL = 10


  def __init__(self, device, payload_dir, dev_dir='', tempdir=None,
               do_rootfs_update=True, do_stateful_update=True, reboot=True,
               disable_verification=False, clobber_stateful=False, yes=False):
    """Initialize a ChromiumOSFlashUpdater for auto-update a chromium OS device.

    Args:
      device: the ChromiumOSDevice to be updated.
      payload_dir: the directory of payload(s).
      dev_dir: the directory of the devserver that runs the CrOS auto-update.
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
    super(ChromiumOSFlashUpdater, self).__init__(device, payload_dir)
    if tempdir is not None:
      self.tempdir = tempdir
    else:
      self.tempdir = tempfile.mkdtemp(prefix='cros-update')

    self.dev_dir = dev_dir

    # Update setting
    self._cmd_kwargs = {}
    self._cmd_kwargs_omit_error = {}
    self._do_stateful_update = do_stateful_update
    self._do_rootfs_update = do_rootfs_update
    self._disable_verification = disable_verification
    self._clobber_stateful = clobber_stateful
    self._reboot = reboot
    self._yes = yes
    # Device's directories
    self.device_dev_dir = os.path.join(self.device.work_dir, 'src')
    self.device_static_dir = os.path.join(self.device.work_dir, 'static')
    self.stateful_update_bin = None


  def CheckPayloads(self):
    """Verify that all required payloads are in |self.payload_dir|."""
    logging.debug('Checking if payloads have been stored in directory %s...',
                  self.payload_dir)
    filenames = []
    filenames += [ds_wrapper.ROOTFS_FILENAME] if self._do_rootfs_update else []
    if self._do_stateful_update:
      filenames += [ds_wrapper.STATEFUL_FILENAME]

    for fname in filenames:
      payload = os.path.join(self.payload_dir, fname)
      if not os.path.exists(payload):
        raise ChromiumOSUpdateError('Payload %s does not exist!' % payload)

  def CheckRestoreStateful(self):
    """Check whether to restore stateful."""
    logging.debug('Checking whether to restore stateful...')
    restore_stateful = False
    if not self._CanRunDevserver() and self._do_rootfs_update:
      msg = ('Cannot start devserver! The stateful partition may be '
             'corrupted.')
      prompt = 'Attempt to restore the stateful partition?'
      restore_stateful = self._yes or cros_build_lib.BooleanPrompt(
          prompt=prompt, default=False, prolog=msg)
      if not restore_stateful:
        raise ChromiumOSUpdateError('Cannot continue to perform rootfs update!')

    logging.debug('Restore stateful partition is%s required.',
                  ('' if restore_stateful else ' not'))
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
    logging.info('Checking if we can run devserver on the device...')
    devserver_bin = os.path.join(self.device_dev_dir, self.DEVSERVER_FILENAME)
    devserver_check_command = ['python', devserver_bin, '--help']
    try:
      self.device.RunCommand(devserver_check_command, **self._cmd_kwargs)
    except cros_build_lib.RunCommandError as e:
      logging.warning('Cannot start devserver: %s', e)
      if ERROR_MSG_IN_LOADING_LIB in str(e):
        logging.info('Attempting to correct device library paths...')
        try:
          self.device.RunCommand(['ldconfig', '-r', '/'], **self._cmd_kwargs)
          self.device.RunCommand(devserver_check_command, **self._cmd_kwargs)
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


  def _GetStatefulUpdateScript(self):
    """Returns the path to the stateful_update_bin on the target.

    Returns:
      <need_transfer, path>:
      need_transfer is True if stateful_update_bin is found in local path,
      False if we directly use stateful_update_bin on the host.
      path: If need_transfer is True, it represents the local path of
      stateful_update_bin, and is used for further transferring. Otherwise,
      it refers to the host path.
    """
    # We attempt to load the local stateful update path in 2 different
    # ways. If this doesn't exist, we attempt to use the Chromium OS
    # Chroot path to the installed script. If all else fails, we use the
    # stateful update script on the host.
    stateful_update_path = path_util.FromChrootPath(self.STATEFUL_UPDATE_BIN)

    if not os.path.exists(stateful_update_path):
      logging.warning('Could not find chroot stateful_update script in %s, '
                      'falling back to the client copy.', stateful_update_path)
      stateful_update_path = os.path.join(self.dev_dir,
                                          self.STATEFUL_UPDATE_FILE)
      if os.path.exists(stateful_update_path):
        logging.debug('Use stateful_update script in devserver path: %s',
                      stateful_update_path)
        return True, stateful_update_path

      logging.debug('Cannot find stateful_update script, will use the script '
                    'on the host')
      return False, self.REMOTE_STATEUL_UPDATE_PATH
    else:
      return True, stateful_update_path


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
    self.device.CopyToWorkDir(src_dir, log_output=True, **self._cmd_kwargs)

  def TransferRootfsUpdate(self):
    """Transfer files for rootfs update.

    Copy the update payload to the remote device for rootfs update.
    """
    device_payload_dir = os.path.join(self.device_static_dir, 'pregenerated')
    self.device.RunCommand(['mkdir', '-p', device_payload_dir],
                           **self._cmd_kwargs)
    logging.info('Copying rootfs payload to device...')
    payload = os.path.join(self.payload_dir, ds_wrapper.ROOTFS_FILENAME)
    self.device.CopyToDevice(payload, device_payload_dir, log_output=True,
                             **self._cmd_kwargs)

  def TransferStatefulUpdate(self):
    """Transfer files for stateful update.

    The stateful update bin and the corresponding payloads are copied to the
    target remote device for stateful update.
    """
    logging.debug('Checking whether file stateful_update_bin needs to be '
                  'transferred to device...')
    need_transfer, stateful_update_bin = self._GetStatefulUpdateScript()
    if need_transfer:
      self.device.CopyToWorkDir(stateful_update_bin, log_output=True,
                                **self._cmd_kwargs)
      self.stateful_update_bin = os.path.join(
          self.device.work_dir, os.path.basename(self.STATEFUL_UPDATE_BIN))
    else:
      self.stateful_update_bin = stateful_update_bin

    logging.info('Copying stateful payload to device...')
    payload = os.path.join(self.payload_dir, ds_wrapper.STATEFUL_FILENAME)
    self.device.CopyToWorkDir(payload, log_output=True, **self._cmd_kwargs)

  def RestoreStateful(self):
    """Restore stateful partition for device."""
    logging.warning('Restoring the stateful partition')
    self.RunUpdateStateful()
    self.device.Reboot()
    if self._CanRunDevserver():
      logging.info('Stateful partition restored.')
    else:
      raise ChromiumOSUpdateError('Unable to restore stateful partition.')

  def ResetStatefulPartition(self):
    """Clear any pending stateful update request."""
    logging.debug('Resetting stateful partition...')
    self.device.RunCommand(['sh', self.stateful_update_bin,
                            '--stateful_change=reset'],
                           **self._cmd_kwargs)

  def RevertBootPartition(self):
    """Revert the boot partition."""
    part = self.GetRootDev(self.device)
    logging.warning('Reverting update; Boot partition will be %s', part)
    self.device.RunCommand(['/postinst', part], **self._cmd_kwargs)

  def UpdateRootfs(self):
    """Update the rootfs partition of the device."""
    logging.info('Updating rootfs partition')
    devserver_bin = os.path.join(self.device_dev_dir, self.DEVSERVER_FILENAME)
    ds = ds_wrapper.RemoteDevServerWrapper(
        self.device, devserver_bin, static_dir=self.device_static_dir,
        log_dir=self.device.work_dir)

    try:
      ds.Start()
      logging.debug('Successfully started devserver on the device.')
      # Use the localhost IP address to ensure that update engine
      # client can connect to the devserver.
      omaha_url = ds.GetDevServerURL(
          ip='127.0.0.1', port=ds.port, sub_dir='update/pregenerated')
      cmd = [self.UPDATE_ENGINE_BIN, '-check_for_update',
             '-omaha_url=%s' % omaha_url]
      self.device.RunCommand(cmd, **self._cmd_kwargs)

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
      self.RevertBootPartition()
      logging.warning(ds.TailLog() or 'No devserver log is available.')
      error_msg = 'Failed to perform rootfs update: %r'
      raise RootfsUpdateError(error_msg % e)
    finally:
      ds.Stop()
      self.device.CopyFromDevice(
          ds.log_file,
          os.path.join(self.tempdir, self.DEVSERVER_LOG),
          **self._cmd_kwargs_omit_error)
      self.device.CopyFromDevice(
          self.UPDATE_ENGINE_LOG, self.tempdir, follow_symlinks=True,
          **self._cmd_kwargs_omit_error)

  def UpdateStateful(self):
    """Update the stateful partition of the device."""
    msg = 'Updating stateful partition'
    cmd = ['sh',
           self.stateful_update_bin,
           os.path.join(self.device.work_dir, ds_wrapper.STATEFUL_FILENAME)]

    if self._clobber_stateful:
      cmd.append('--stateful_change=clean')
      msg += ' with clobber enabled'

    logging.info('%s...', msg)
    try:
      self.device.RunCommand(cmd, **self._cmd_kwargs)
    except cros_build_lib.RunCommandError as e:
      logging.error('Stateful update failed.')
      self.ResetStatefulPartition()
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
    restore_stateful = self.CheckRestoreStateful()
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


class ChromiumOSUpdater(ChromiumOSFlashUpdater):
  """Used to auto-update Cros DUT with image.

  Different from ChromiumOSFlashUpdater, which only contains cros-flash
  related auto-update methods, ChromiumOSUpdater includes pre-setup and
  post-check methods for both rootfs and stateful update. It also contains
  various single check functions, like CheckVersion() and _ResetUpdateEngine().

  Furthermore, this class adds retry to package transfer-related functions.
  """
  STATEFUL_FOLDER_TO_CHECK = ['/var', '/home', '/mnt/stateful_partition']
  STATEFUL_TEST_FILE = '.test_file_to_be_deleted'
  UPDATED_MARKER = '/var/run/update_engine_autoupdate_completed'
  _LAB_MACHINE_FILE = '/mnt/stateful_partition/.labmachine'
  AUTO_DIR = '/usr/local/autotest'
  KERNEL_A = {'name': 'KERN-A', 'kernel': 2, 'root': 3}
  KERNEL_B = {'name': 'KERN-B', 'kernel': 4, 'root': 5}
  KERNEL_UPDATE_TIMEOUT = 120
  # Related to crbug.com/276094: Restore to 5 mins once the 'host did not
  # return from reboot' bug is solved.
  REBOOT_TIMEOUT = 480

  def __init__(self, device, build_name, payload_dir, dev_dir='',
               log_file=None, tempdir=None, autodir=None,
               clobber_stateful=True, local_devserver=False, yes=False):
    """Initialize a ChromiumOSUpdater for auto-update a chromium OS device.

    Args:
      device: the ChromiumOSDevice to be updated.
      build_name: the target update version for the device.
      payload_dir: the directory of payload(s).
      dev_dir: the directory of the devserver that runs the CrOS auto-update.
      log_file: The file to save running logs.
      tempdir: the temp directory in caller, not in the device. For example,
          the tempdir for cros flash is /tmp/cros-flash****/, used to
          temporarily keep files when transferring devserver package, and
          reserve devserver and update engine logs.
      autodir: the directory of autotest files in the device.
      clobber_stateful: whether to do a clean stateful update. The default is
          True for CrOS update.
      local_devserver: Indecate whether users use their local devserver.
          Default: False.
      yes: Assume "yes" (True) for any prompt. The default is False. However,
          it should be set as True if we want to disable all the prompts for
          auto-update.
    """
    super(ChromiumOSUpdater, self).__init__(
        device, payload_dir, dev_dir=dev_dir, tempdir=tempdir,
        clobber_stateful=clobber_stateful, yes=yes)

    if log_file:
      self._cmd_kwargs['log_stdout_to_file'] = log_file
      self._cmd_kwargs['append_to_file'] = True
      self._cmd_kwargs['combine_stdout_stderr'] = True
      self._cmd_kwargs_omit_error['log_stdout_to_file'] = log_file
      self._cmd_kwargs_omit_error['append_to_file'] = True
      self._cmd_kwargs_omit_error['combine_stdout_stderr'] = True
      self._cmd_kwargs_omit_error['error_code_ok'] = True

    self.inactive_kernel = None
    if local_devserver:
      self.update_version = None
    else:
      self.update_version = build_name

    if not autodir:
      self.device_auto_dir = self.AUTO_DIR
    else:
      self.device_auto_dir = autodir


  def _cgpt(self, flag, kernel, dev='$(rootdev -s -d)'):
    """Return numeric cgpt value for the specified flag, kernel, device."""
    cmd = ['cgpt', 'show', '-n', '-i', '%d' % kernel['kernel'], flag, dev]
    return int(self.device.RunCommand(
        cmd, capture_output=True).output.strip())

  def _GetKernelPriority(self, kernel):
    """Return numeric priority for the specified kernel.

    Args:
      kernel: information of the given kernel, KERNEL_A or KERNEL_B.
    """
    return self._cgpt('-P', kernel)

  def _GetKernelSuccess(self, kernel):
    """Return boolean success flag for the specified kernel.

    Args:
      kernel: information of the given kernel, KERNEL_A or KERNEL_B.
    """
    return self._cgpt('-S', kernel) != 0

  def _GetKernelTries(self, kernel):
    """Return tries count for the specified kernel.

    Args:
      kernel: information of the given kernel, KERNEL_A or KERNEL_B.
    """
    return self._cgpt('-T', kernel)

  def _GetKernelState(self):
    """Returns the (<active>, <inactive>) kernel state as a pair."""
    active_root = int(re.findall(r'(\d+\Z)', self.GetRootDev(self.device))[0])
    if active_root == self.KERNEL_A['root']:
      return self.KERNEL_A, self.KERNEL_B
    elif active_root == self.KERNEL_B['root']:
      return self.KERNEL_B, self.KERNEL_A
    else:
      raise ChromiumOSUpdateError('Encountered unknown root partition: %s' %
                                  active_root)

  def _GetReleaseVersion(self):
    """Get release version of the device."""
    lsb_release_content = self.device.RunCommand(
        ['cat', '/etc/lsb-release'],
        capture_output=True).output.strip()
    return auto_update_util.GetChromeosReleaseVersion(
        lsb_release_content=lsb_release_content)

  def CheckVersion(self):
    """Check the image running in DUT has the expected version.

    Returns:
      True if the DUT's image version matches the version that the
      ChromiumOSUpdater tries to update to.
    """
    booted_version = self._GetReleaseVersion()
    return (self.update_version and
            self.update_version.endswith(booted_version))

  def _ResetUpdateEngine(self):
    """Resets the host to prepare for a clean update regardless of state."""
    self.device.RunCommand(['rm', '-f', self.UPDATED_MARKER],
                           **self._cmd_kwargs)
    self.device.RunCommand(['stop', 'ui'], **self._cmd_kwargs_omit_error)
    self.device.RunCommand(['stop', 'update-engine'],
                           **self._cmd_kwargs_omit_error)
    self.device.RunCommand(['start', 'update-engine'], **self._cmd_kwargs)

    status = retry_util.RetryException(
        Exception,
        MAX_RETRY,
        self.GetUpdateStatus, self.device,
        delay_sec=DELAY_SEC_FOR_RETRY)

    if status[0] != UPDATE_STATUS_IDLE:
      raise PreSetupUpdateError('%s is not in an installable state' %
                                self.device.hostname)

  def _VerifyBootExpectations(self, expected_kernel_state, rollback_message):
    """Verify that we fully booted given expected kernel state.

    It verifies that we booted using the correct kernel state, and that the
    OS has marked the kernel as good.

    Args:
      expected_kernel_state: kernel state that we're verifying with i.e. I
        expect to be booted onto partition 4 etc. See output of _GetKernelState.
      rollback_message: string to raise as a RootfsUpdateError if we booted
        with the wrong partition.
    """
    logging.debug('Start verifying boot expectations...')
    # Figure out the newly active kernel
    active_kernel_state = self._GetKernelState()[0]

    # Rollback
    if (expected_kernel_state and
        active_kernel_state != expected_kernel_state):
      logging.debug('Dumping partition table.')
      self.device.RunCommand(['cgpt', 'show', '$(rootdev -s -d)'],
                             **self._cmd_kwargs)
      logging.debug('Dumping crossystem for firmware debugging.')
      self.device.RunCommand(['crossystem', '--all'], **self._cmd_kwargs)
      raise RootfsUpdateError(rollback_message)

    # Make sure chromeos-setgoodkernel runs
    try:
      timeout_util.WaitForReturnTrue(
          lambda: (self._GetKernelTries(active_kernel_state) == 0
                   and self._GetKernelSuccess(active_kernel_state)),
          self.KERNEL_UPDATE_TIMEOUT,
          period=5)
    except timeout_util.TimeoutError:
      services_status = self.device.RunCommand(
          ['status', 'system-services'], capture_output=True).output
      logging.debug('System services_status: %r' % services_status)
      if services_status != 'system-services start/running\n':
        event = ('Chrome failed to reach login screen')
      else:
        event = ('update-engine failed to call '
                 'chromeos-setgoodkernel')
      raise RootfsUpdateError(
          'After update and reboot, %s '
          'within %d seconds' % (event, self.KERNEL_UPDATE_TIMEOUT))


  def _CheckVersionToConfirmInstall(self):
    # In the local_devserver case, we can't know the expected
    # build, so just pass.
    logging.debug('Checking whether the new build is successfully installed...')
    if not self.update_version:
      logging.debug('No update_version is provided if test is executed with'
                    'local devserver.')
      return True

    # Always try the default check_version method first, this prevents
    # any backward compatibility issue.
    if self.CheckVersion():
      return True

    return auto_update_util.VersionMatch(
        self.update_version, self._GetReleaseVersion())

  def TransferDevServerPackage(self):
    """Transfer devserver package to work directory of the remote device."""
    retry_util.RetryException(
        cros_build_lib.RunCommandError,
        MAX_RETRY,
        super(ChromiumOSUpdater, self).TransferDevServerPackage,
        delay_sec=DELAY_SEC_FOR_RETRY)

  def TransferRootfsUpdate(self):
    """Transfer files for rootfs update.

    The corresponding payload are copied to the remote device for rootfs
    update.
    """
    retry_util.RetryException(
        cros_build_lib.RunCommandError,
        MAX_RETRY,
        super(ChromiumOSUpdater, self).TransferRootfsUpdate,
        delay_sec=DELAY_SEC_FOR_RETRY)

  def TransferStatefulUpdate(self):
    """Transfer files for stateful update.

    The stateful update bin and the corresponding payloads are copied to the
    target remote device for stateful update.
    """
    retry_util.RetryException(
        cros_build_lib.RunCommandError,
        MAX_RETRY,
        super(ChromiumOSUpdater, self).TransferStatefulUpdate,
        delay_sec=DELAY_SEC_FOR_RETRY)

  def PreSetupCrOSUpdate(self):
    """Pre-setup for whole auto-update process for cros_host.

    It includes:
      1. Create a file to indicate if provision fails for cros_host.
         The file will be removed by stateful update or full install.
    """
    logging.debug('Start pre-setup for the whole CrOS update process...')
    self.device.RunCommand(['touch', self.PROVISION_FAILED], **self._cmd_kwargs)

    # Related to crbug.com/360944.
    release_pattern = r'^.*-release/R[0-9]+-[0-9]+\.[0-9]+\.0$'
    if not re.match(release_pattern, self.update_version):
      logging.debug('The update version is not matched to release pattern')
      return False

    if not self.CheckVersion():
      logging.debug('The update version is not matched to the current version')
      return False

    return True

  def PreSetupStatefulUpdate(self):
    """Pre-setup for stateful update for CrOS host."""
    logging.debug('Start pre-setup for stateful update...')
    self.device.RunCommand(['sudo', 'stop', 'ap-update-manager'],
                           **self._cmd_kwargs_omit_error)

    for folder in self.STATEFUL_FOLDER_TO_CHECK:
      touch_path = os.path.join(folder, self.STATEFUL_TEST_FILE)
      self.device.RunCommand(['touch', touch_path], **self._cmd_kwargs)

    self._ResetUpdateEngine()
    self.ResetStatefulPartition()

  def PostCheckStatefulUpdate(self):
    """Post-check for stateful update for CrOS host."""
    logging.debug('Start post check for stateful update...')
    self.device.Reboot(timeout_sec=self.REBOOT_TIMEOUT)
    check_file_cmd = 'test -f %s; echo $?'
    for folder in self.STATEFUL_FOLDER_TO_CHECK:
      test_file_path = os.path.join(folder, self.STATEFUL_TEST_FILE)
      result = self.device.RunCommand([check_file_cmd % test_file_path],
                                      **self._cmd_kwargs_omit_error)
      if result.returncode == 1:
        raise StatefulUpdateError('failed to post-check stateful update.')

  def PreSetupRootfsUpdate(self):
    """Pre-setup for rootfs update for CrOS host."""
    logging.debug('Start pre-setup for rootfs update...')
    self.device.Reboot(timeout_sec=self.REBOOT_TIMEOUT)
    self.device.RunCommand(['sudo', 'stop', 'ap-update-manager'],
                           **self._cmd_kwargs_omit_error)
    self._ResetUpdateEngine()

  def RestoreStateful(self):
    """Restore stateful partition for device."""
    logging.warning('Restoring the stateful partition')
    self.PreSetupStatefulUpdate()
    self.UpdateStateful()
    self.PostCheckStatefulUpdate()
    if self._CanRunDevserver():
      logging.info('Stateful partition restored.')
    else:
      raise ChromiumOSUpdateError('Unable to restore stateful partition.')

  def PostCheckRootfsUpdate(self):
    """Post-check for rootfs update for CrOS host."""
    logging.debug('Start post check for rootfs update...')
    active_kernel, inactive_kernel = self._GetKernelState()
    if (self._GetKernelPriority(inactive_kernel) <
        self._GetKernelPriority(active_kernel)):
      raise RootfsUpdateError('Update failed. The priority of the inactive '
                              'kernel partition is less than that of the '
                              'active kernel partition.')

    self.inactive_kernel = inactive_kernel
    self.device.Reboot(timeout_sec=self.REBOOT_TIMEOUT)

  def PostCheckCrOSUpdate(self):
    """Post check for the whole auto-update process."""
    logging.debug('Post check for the whole CrOS update...')
    # Not use 'sh' here since current device.RunCommand cannot recognize
    # the content of $FILE.
    autoreboot_cmd = ('FILE="%s" ; [ -f "$FILE" ] || '
                      '( touch "$FILE" ; start autoreboot )')
    self.device.RunCommand(autoreboot_cmd % self._LAB_MACHINE_FILE,
                           **self._cmd_kwargs)
    self._VerifyBootExpectations(
        self.inactive_kernel, rollback_message=
        'Build %s failed to boot on %s; system rolled back to previous '
        'build' % (self.update_version, self.device.hostname))
    # Check that we've got the build we meant to install.
    if not self._CheckVersionToConfirmInstall():
      raise ChromiumOSUpdateError(
          'Failed to update %s to build %s; found build '
          '%s instead' % (self.device.hostname,
                          self.update_version,
                          self._GetReleaseVersion()))

    logging.debug('Cleaning up old autotest directories...')
    try:
      self.device.RunCommand(['rm', '-rf', self.device_auto_dir])
    except cros_build_lib.RunCommandError as e:
      logging.debug('Cannot clean up the old autotest directories: %s', e)
