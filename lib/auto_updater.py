# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing functions to execute auto-update on a remote device.

ChromiumOSUpdater includes:
  ----Check-----
  * Check functions, including kernel/version/cgpt check.

  ----Precheck---
  * Pre-check if the device can run its nebraska.
  * Pre-check for stateful/rootfs update/whole update.

  ----Tranfer----
  * This step is carried out by Transfer subclasses in
    auto_updater_transfer.py.

  ----Auto-Update---
  * Do rootfs partition update if it's required.
  * Do stateful partition update if it's required.
  * Do reboot for device if it's required.

  ----Verify----
  * Do verification if it's required.
  * Disable rootfs verification in device if it's required.
  * Post-check stateful/rootfs update/whole update.
"""

from __future__ import print_function

import json
import os
import re
import subprocess
import tempfile
import time

from chromite.cli import command
from chromite.lib import auto_update_util
from chromite.lib import auto_updater_transfer
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import nebraska_wrapper
from chromite.lib import operation
from chromite.lib import osutils
from chromite.lib import remote_access
from chromite.lib import retry_util
from chromite.lib import stateful_updater
from chromite.lib import timeout_util

from chromite.utils import key_value_store

# Naming conventions for global variables:
#   File on remote host without slash: REMOTE_XXX_FILENAME
#   File on remote host with slash: REMOTE_XXX_FILE_PATH
#   Path on remote host with slash: REMOTE_XXX_PATH
#   File on local server without slash: LOCAL_XXX_FILENAME

# Update Status for remote device.
UPDATE_STATUS_IDLE = 'UPDATE_STATUS_IDLE'
UPDATE_STATUS_DOWNLOADING = 'UPDATE_STATUS_DOWNLOADING'
UPDATE_STATUS_FINALIZING = 'UPDATE_STATUS_FINALIZING'
UPDATE_STATUS_UPDATED_NEED_REBOOT = 'UPDATE_STATUS_UPDATED_NEED_REBOOT'

# Max number of the times for retry:
# 1. for transfer functions to be retried.
# 2. for some retriable commands to be retried.
MAX_RETRY = 5

# The delay between retriable tasks.
DELAY_SEC_FOR_RETRY = 5

# Number of seconds to wait for the post check version to settle.
POST_CHECK_SETTLE_SECONDS = 15

# Number of seconds to delay between post check retries.
POST_CHECK_RETRY_SECONDS = 5


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


class RebootVerificationError(ChromiumOSUpdateError):
  """Raised for failing to reboot errors."""


class BaseUpdater(object):
  """The base updater class."""

  def __init__(self, device, payload_dir):
    self.device = device
    self.payload_dir = payload_dir


class ChromiumOSUpdater(BaseUpdater):
  """Used to update DUT with image."""

  # Nebraska files.
  LOCAL_NEBRASKA_LOG_FILENAME = 'nebraska.log'
  REMOTE_NEBRASKA_FILENAME = 'nebraska.py'

  # rootfs update files.
  REMOTE_UPDATE_ENGINE_BIN_FILENAME = 'update_engine_client'
  REMOTE_UPDATE_ENGINE_LOGFILE_PATH = '/var/log/update_engine.log'
  REMOTE_PROVISION_FAILED_FILE_PATH = '/var/tmp/provision_failed'
  REMOTE_QUICK_PROVISION_LOGFILE_PATH = '/var/log/quick-provision.log'

  UPDATE_CHECK_INTERVAL_PROGRESSBAR = 0.5
  UPDATE_CHECK_INTERVAL_NORMAL = 10

  # Update engine perf files.
  REMOTE_UPDATE_ENGINE_PERF_SCRIPT_PATH = \
      '/mnt/stateful_partition/unencrypted/preserve/' \
      'update_engine_performance_monitor.py'
  REMOTE_UPDATE_ENGINE_PERF_RESULTS_PATH = '/var/log/perf_data_results.json'

  # `mode` parameter when copying payload files to the DUT.
  PAYLOAD_MODE_PARALLEL = 'parallel'
  PAYLOAD_MODE_SCP = 'scp'

  # Related to crbug.com/276094: Restore to 5 mins once the 'host did not
  # return from reboot' bug is solved.
  REBOOT_TIMEOUT = 480

  REMOTE_STATEFUL_PATH_TO_CHECK = ('/var', '/home', '/mnt/stateful_partition')
  REMOTE_STATEFUL_TEST_FILENAME = '.test_file_to_be_deleted'
  REMOTE_UPDATED_MARKERFILE_PATH = '/run/update_engine_autoupdate_completed'
  REMOTE_LAB_MACHINE_FILE_PATH = '/mnt/stateful_partition/.labmachine'
  KERNEL_A = {'name': 'KERN-A', 'kernel': 2, 'root': 3}
  KERNEL_B = {'name': 'KERN-B', 'kernel': 4, 'root': 5}
  KERNEL_UPDATE_TIMEOUT = 180

  PAYLOAD_DIR_NAME = 'payloads'

  def __init__(self, device, build_name, payload_dir, transfer_class,
               dev_dir='', log_file=None, tempdir=None,
               original_payload_dir=None, clobber_stateful=True,
               local_devserver=False, yes=False, do_rootfs_update=True,
               do_stateful_update=True, reboot=True, disable_verification=False,
               send_payload_in_parallel=False, payload_filename=None,
               experimental_au=False, staging_server=None):
    """Initialize a ChromiumOSUpdater for auto-update a chromium OS device.

    Args:
      device: the ChromiumOSDevice to be updated.
      build_name: the target update version for the device.
      payload_dir: the directory of payload(s).
      transfer_class: A reference to any subclass of
          auto_updater_transfer.Transfer class.
      dev_dir: the directory of the nebraska that runs the CrOS auto-update.
      log_file: The file to save running logs.
      tempdir: the temp directory in caller, not in the device. For example,
          the tempdir for cros flash is /tmp/cros-flash****/, used to
          temporarily keep files when transferring update-utils package, and
          reserve nebraska and update engine logs.
      original_payload_dir: The directory containing payloads whose version is
          the same as current host's rootfs partition. If it's None, will first
          try installing the matched stateful.tgz with the host's rootfs
          Partition when restoring stateful. Otherwise, install the target
          stateful.tgz.
      do_rootfs_update: whether to do rootfs partition update. The default is
          True.
      do_stateful_update: whether to do stateful partition update. The default
          is True.
      reboot: whether to reboot device after update. The default is True.
      disable_verification: whether to disabling rootfs verification on the
          device. The default is False.
      clobber_stateful: whether to do a clean stateful update. The default is
          False.
      local_devserver: Indicate whether users use their local devserver.
          Default: False.
      yes: Assume "yes" (True) for any prompt. The default is False. However,
          it should be set as True if we want to disable all the prompts for
          auto-update.
      payload_filename: Filename of exact payload file to use for
          update instead of the default: update.gz. Defaults to None. Use
          only if you staged a payload by filename (i.e not artifact) first.
      send_payload_in_parallel: whether to transfer payload in chunks
          in parallel. The default is False.
      experimental_au: Use experimental features of auto updater instead. It
          should be deprecated once crbug.com/872441 is fixed.
      staging_server: URL (str) of the server that's staging the payload files.
          Assuming transfer_class is None, if value for staging_server is None
          or empty, an auto_updater_transfer.LocalTransfer reference must be
          passed through the transfer_class parameter.
    """
    super(ChromiumOSUpdater, self).__init__(device, payload_dir)

    self.tempdir = (tempdir if tempdir is not None
                    else tempfile.mkdtemp(prefix='cros-update'))
    self.inactive_kernel = None
    self.update_version = None if local_devserver else build_name

    self.dev_dir = dev_dir
    self.original_payload_dir = original_payload_dir

    # Update setting
    self._cmd_kwargs = {}
    self._cmd_kwargs_omit_error = {'check': False}
    self._do_stateful_update = do_stateful_update
    self._do_rootfs_update = do_rootfs_update
    self._disable_verification = disable_verification
    self._clobber_stateful = clobber_stateful
    self._reboot = reboot
    self._yes = yes
    # Device's directories
    self.device_dev_dir = os.path.join(self.device.work_dir, 'src')
    self.device_payload_dir = os.path.join(self.device.work_dir,
                                           self.PAYLOAD_DIR_NAME)
    self.device_restore_dir = os.path.join(self.device.work_dir, 'old')
    # autoupdate_EndToEndTest uses exact payload filename for update
    self.payload_filename = payload_filename
    if send_payload_in_parallel:
      self.payload_mode = self.PAYLOAD_MODE_PARALLEL
    else:
      self.payload_mode = self.PAYLOAD_MODE_SCP
    self.perf_id = None
    self.experimental_au = experimental_au

    if log_file:
      log_kwargs = {
          'stdout': log_file,
          'append_to_file': True,
          'stderr': subprocess.STDOUT,
      }
      self._cmd_kwargs.update(log_kwargs)
      self._cmd_kwargs_omit_error.update(log_kwargs)

    self._staging_server = staging_server
    self._transfer_obj = self._CreateTransferObject(transfer_class)

  @property
  def is_au_endtoendtest(self):
    return self.payload_filename is not None

  def _CreateTransferObject(self, transfer_class):
    """Create the correct Transfer class.

    Args:
      transfer_class: A variable that contains a reference to one of the
          Transfer classes in auto_updater_transfer.
    """
    assert issubclass(transfer_class, auto_updater_transfer.Transfer)

    # Determine if staging_server needs to be passed as an argument to
    # class_ref.
    cls_kwargs = {}
    if self._staging_server:
      cls_kwargs['staging_server'] = self._staging_server

    return transfer_class(
        device=self.device, payload_dir=self.payload_dir,
        payload_name=self._GetRootFsPayloadFileName(),
        cmd_kwargs=self._cmd_kwargs,
        transfer_rootfs_update=self._do_rootfs_update,
        transfer_stateful_update=self._do_rootfs_update, dev_dir=self.dev_dir,
        original_payload_dir=self.original_payload_dir,
        device_restore_dir=self.device_restore_dir,
        device_payload_dir=self.device_payload_dir, tempdir=self.tempdir,
        payload_mode=self.payload_mode, **cls_kwargs)

  def CheckPayloads(self):
    """DEPRECATED.  Use auto_updater_transfer.Transfer Class instead.

    Verify that all required payloads are in |self.payload_dir|.
    """
    self._transfer_obj.CheckPayloads()

  def CheckRestoreStateful(self):
    """Check whether to restore stateful."""
    logging.debug('Checking whether to restore stateful...')
    restore_stateful = False
    try:
      self._CheckNebraskaCanRun()
      return restore_stateful
    except nebraska_wrapper.NebraskaStartupError as e:
      if self._do_rootfs_update:
        msg = ('Cannot start nebraska! The stateful partition may be '
               'corrupted: %s' % e)
        prompt = 'Attempt to restore the stateful partition?'
        restore_stateful = self._yes or cros_build_lib.BooleanPrompt(
            prompt=prompt, default=False, prolog=msg)
        if not restore_stateful:
          raise ChromiumOSUpdateError(
              'Cannot continue to perform rootfs update!')

    logging.debug('Restore stateful partition is%s required.',
                  ('' if restore_stateful else ' not'))
    return restore_stateful

  def _CheckNebraskaCanRun(self):
    """We can run Nebraska on |device|."""
    nebraska_bin = os.path.join(self.device_dev_dir,
                                self.REMOTE_NEBRASKA_FILENAME)
    nebraska = nebraska_wrapper.RemoteNebraskaWrapper(
        self.device, nebraska_bin=nebraska_bin)
    nebraska.CheckNebraskaCanRun()

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
    result = device.run([cls.REMOTE_UPDATE_ENGINE_BIN_FILENAME, '--status'],
                        capture_output=True, log_output=True)

    if not result.output:
      raise Exception('Cannot get update status')

    try:
      status = key_value_store.LoadData(result.output)
    except ValueError:
      raise ValueError('Cannot parse update status')

    values = []
    for key in keys:
      if key not in status:
        raise ValueError('Missing "%s" in the update engine status' % key)

      values.append(status.get(key))

    return values

  @classmethod
  def GetRootDev(cls, device):
    """Get the current root device on |device|.

    Args:
      device: a ChromiumOSDevice object, defines whose root device we
          want to fetch.
    """
    rootdev = device.run(
        ['rootdev', '-s'], capture_output=True).output.strip()
    logging.debug('Current root device is %s', rootdev)
    return rootdev

  def _StartUpdateEngineIfNotRunning(self, device):
    """Starts update-engine service if it is not running.

    Args:
      device: a ChromiumOSDevice object, defines the target root device.
    """
    try:
      result = device.run(['start', 'update-engine'],
                          capture_output=True, log_output=True).stdout
      if 'start/running' in result:
        logging.info('update engine was not running, so we started it.')
    except cros_build_lib.RunCommandError as e:
      if e.result.returncode != 1 or 'is already running' not in e.result.error:
        raise e

  def SetupRootfsUpdate(self):
    """Makes sure |device| is ready for rootfs update."""
    logging.info('Checking if update engine is idle...')
    self._StartUpdateEngineIfNotRunning(self.device)
    status = self.GetUpdateStatus(self.device)[0]
    if status == UPDATE_STATUS_UPDATED_NEED_REBOOT:
      logging.info('Device needs to reboot before updating...')
      self._Reboot('setup of Rootfs Update')
      status = self.GetUpdateStatus(self.device)[0]

    if status != UPDATE_STATUS_IDLE:
      raise RootfsUpdateError('Update engine is not idle. Status: %s' % status)

  def _GetDevicePythonSysPath(self):
    """Get python sys.path of the given |device|."""
    sys_path = self.device.run(
        ['python', '-c', '"import json, sys; json.dump(sys.path, sys.stdout)"'],
        capture_output=True, log_output=True).output
    return json.loads(sys_path)

  def _FindDevicePythonPackagesDir(self):
    """Find the python packages directory for the given |device|."""
    third_party_host_dir = ''
    sys_path = self._GetDevicePythonSysPath()
    for p in sys_path:
      if p.endswith('site-packages') or p.endswith('dist-packages'):
        third_party_host_dir = p
        break

    if not third_party_host_dir:
      raise ChromiumOSUpdateError(
          'Cannot find proper site-packages/dist-packages directory from '
          'sys.path for storing packages: %s' % sys_path)

    return third_party_host_dir

  def _GetRootFsPayloadFileName(self):
    """Get the correct RootFs payload filename.

    Returns:
      The payload filename. (update.gz or a custom payload filename).
    """
    if self.is_au_endtoendtest:
      return self.payload_filename
    else:
      return auto_updater_transfer.ROOTFS_FILENAME

  def ResetStatefulPartition(self):
    """Clear any pending stateful update request."""
    logging.debug('Resetting stateful partition...')
    try:
      stateful_updater.StatefulUpdater(self.device).Reset()
    except stateful_updater.Error as e:
      raise StatefulUpdateError(e)

  def RevertBootPartition(self):
    """Revert the boot partition."""
    part = self.GetRootDev(self.device)
    logging.warning('Reverting update; Boot partition will be %s', part)
    try:
      self.device.run(['/postinst', part], **self._cmd_kwargs)
    except cros_build_lib.RunCommandError as e:
      logging.warning('Reverting the boot partition failed: %s', e)

  def UpdateRootfs(self):
    """Update the rootfs partition of the device (utilizing nebraska)."""
    logging.info('Updating rootfs partition with Nebraska.')
    nebraska_bin = os.path.join(self.device_dev_dir,
                                self.REMOTE_NEBRASKA_FILENAME)

    nebraska = nebraska_wrapper.RemoteNebraskaWrapper(
        self.device, nebraska_bin=nebraska_bin,
        update_payloads_address='file://' + self.device_payload_dir,
        update_metadata_dir=self.device_payload_dir)

    try:
      nebraska.Start()

      # Use the localhost IP address (default) to ensure that update engine
      # client can connect to the nebraska.
      nebraska_url = nebraska.GetURL(critical_update=True)
      cmd = [self.REMOTE_UPDATE_ENGINE_BIN_FILENAME, '--check_for_update',
             '--omaha_url="%s"' % nebraska_url]

      self._StartPerformanceMonitoringForAUTest()
      self.device.run(cmd, **self._cmd_kwargs)

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
        # Number of times to retry `update_engine_client --status`. See
        # crbug.com/744212.
        update_engine_status_retry = 30
        op, progress = retry_util.RetryException(
            cros_build_lib.RunCommandError,
            update_engine_status_retry,
            self.GetUpdateStatus,
            self.device,
            ['CURRENT_OP', 'PROGRESS'],
            delay_sec=DELAY_SEC_FOR_RETRY)[0:2]
        logging.info('Waiting for update...status: %s at progress %s',
                     op, progress)

        if op == UPDATE_STATUS_UPDATED_NEED_REBOOT:
          logging.notice('Update completed.')
          break

        if op == UPDATE_STATUS_IDLE:
          # Something went wrong. Try to get last error code.
          cmd = ['cat', self.REMOTE_UPDATE_ENGINE_LOGFILE_PATH]
          log = self.device.run(cmd).stdout.strip().splitlines()
          err_str = 'Updating payload state for error code: '
          targets = [line for line in log if err_str in line]
          logging.debug('Error lines found: %s', targets)
          if not targets:
            raise RootfsUpdateError(
                'Update failed with unexpected update status: %s' % op)
          else:
            # e.g 20 (ErrorCode::kDownloadStateInitializationError)
            raise RootfsUpdateError(targets[-1].rpartition(err_str)[2])

        if oper is not None:
          if op == UPDATE_STATUS_DOWNLOADING:
            oper.ProgressBar(float(progress))
          elif end_message_not_printed and op == UPDATE_STATUS_FINALIZING:
            oper.Cleanup()
            logging.notice('Finalizing image.')
            end_message_not_printed = False

        time.sleep(update_check_interval)
    # TODO(ahassani): Scope the Exception to finer levels. For example we don't
    # need to revert the boot partition if the Nebraska fails to start, etc.
    except Exception as e:
      logging.error('Rootfs update failed %s', e)
      self.RevertBootPartition()
      logging.warning(nebraska.PrintLog() or 'No nebraska log is available.')
      raise RootfsUpdateError('Failed to perform rootfs update: %r' % e)
    finally:
      self._CopyHostLogFromDevice(nebraska, 'rootfs')
      nebraska.Stop()

      nebraska.CollectLogs(os.path.join(self.tempdir,
                                        self.LOCAL_NEBRASKA_LOG_FILENAME))

      self.device.CopyFromDevice(
          self.REMOTE_UPDATE_ENGINE_LOGFILE_PATH,
          os.path.join(self.tempdir, os.path.basename(
              self.REMOTE_UPDATE_ENGINE_LOGFILE_PATH)),
          follow_symlinks=True,
          **self._cmd_kwargs_omit_error)
      self.device.CopyFromDevice(
          self.REMOTE_QUICK_PROVISION_LOGFILE_PATH,
          os.path.join(self.tempdir, os.path.basename(
              self.REMOTE_QUICK_PROVISION_LOGFILE_PATH)),
          follow_symlinks=True,
          ignore_failures=True,
          **self._cmd_kwargs_omit_error)

      self._StopPerformanceMonitoringForAUTest()

  def UpdateStateful(self, use_original_build=False):
    """Update the stateful partition of the device.

    Args:
      use_original_build: True if we use stateful.tgz of original build for
        stateful update, otherwise, as default, False.
    """
    if self.original_payload_dir and use_original_build:
      payload_dir = self.device_restore_dir
    else:
      payload_dir = self.device.work_dir

    try:
      updater = stateful_updater.StatefulUpdater(self.device)
      updater.Update(
          os.path.join(payload_dir, auto_updater_transfer.STATEFUL_FILENAME),
          update_type=(stateful_updater.StatefulUpdater.UPDATE_TYPE_CLOBBER if
                       self._clobber_stateful else None))
    except stateful_updater.Error as e:
      error_msg = 'Stateful update failed with error: %s' % str(e)
      logging.exception(error_msg)
      self.ResetStatefulPartition()
      raise StatefulUpdateError(error_msg)

  def _FixPayloadPropertiesFile(self):
    """Fix the update payload properties file so nebraska can use it.

    Update the payload properties file to make sure that nebraska can use it.
    The reason is that very old payloads are still being used for provisioning
    the AU tests, but those properties files are not compatible with recent
    nebraska protocols.

    TODO(ahassani): Once we only test delta or full payload with
    source image of M77 or higher, this function can be deprecated.

    TODO(ahassani): Merge this somehow with ResolveAPPIDMismatchIfAny().
    """
    logging.info('Fixing payload properties file.')
    payload_properties_path = self._transfer_obj.GetPayloadPropsFile()
    props = json.loads(osutils.ReadFile(payload_properties_path))
    values = self._transfer_obj.GetPayloadProps()

    # TODO(ahassani): Use the keys form nebraska.py once it is moved to
    # chromite.
    valid_entries = {
        'appid': '',
        # Since only old payloads don't have this and they are only used for
        # provisioning, they will be full payloads.
        'is_delta': False,
        'size': values['size'],
        'target_version': values['image_version'],
    }

    are_props_modified = False
    for key, value in valid_entries.items():
      if props.get(key) is None:
        props[key] = value
        are_props_modified = True

    if are_props_modified:
      with open(payload_properties_path, 'w') as fp:
        json.dump(props, fp)

  def RunUpdateRootfs(self):
    """Run all processes needed by updating rootfs.

    1. Check device's status to make sure it can be updated.
    2. Copy files to remote device needed for rootfs update.
    3. Do root updating.
    """
    self.SetupRootfsUpdate()

    # Any call to self._transfer_obj.TransferRootfsUpdate() must be preceeded by
    # a conditional call to self._FixPayloadPropertiesFile() as this handles the
    # usecase in reported in crbug.com/1012520. Whenever
    # self._FixPayloadPropertiesFile() gets deprecated, this call can be safely
    # removed. For more details on TODOs, refer to self.TransferRootfsUpdate()
    # docstrings.

    self._FixPayloadPropertiesFile()

    # Copy payload for rootfs update.

    self._transfer_obj.TransferRootfsUpdate()

    self.UpdateRootfs()

  def RunUpdateStateful(self):
    """Run all processes needed by updating stateful.

    1. Copy files to remote device needed by stateful update.
    2. Do stateful update.
    """
    self._transfer_obj.TransferStatefulUpdate()
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
    logging.notice('Rebooting device...')
    # Record the current root device. This must be done after SetupRootfsUpdate
    # and before reboot, since SetupRootfsUpdate may reboot the device if there
    # is a pending update, which changes the root device, and reboot will
    # definitely change the root device if update successfully finishes.
    old_root_dev = self.GetRootDev(self.device)
    self.device.Reboot()
    if self._clobber_stateful:
      self.device.run(['mkdir', '-p', self.device.work_dir])

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

  def PreparePayloadPropsFile(self):
    """Triggers download for payload properties file for LabTransfer usecase."""
    prop_file = self._transfer_obj.GetPayloadPropsFile()
    self.ResolveAPPIDMismatchIfAny(prop_file)

  def ResolveAPPIDMismatchIfAny(self, prop_file):
    """Resolves and APP ID mismatch between the payload and device.

    If the APP ID of the payload is different than the device, then the nebraska
    will fail. We empty the payload's AppID so nebraska can do partial APP ID
    matching.
    """
    content = json.loads(osutils.ReadFile(prop_file))
    payload_app_id = content.get('appid')

    if ((self.device.app_id and self.device.app_id == payload_app_id) or
        payload_app_id == ''):
      return

    logging.warn('You are installing an image with a different release '
                 'App ID than the device (%s vs %s), we are forcing the '
                 'install!', payload_app_id, self.device.app_id)
    # Override the properties file with the new empty APP ID.
    content['appid'] = ''
    osutils.WriteFile(prop_file, json.dumps(content))

  def RunUpdate(self):
    """Update the device with image of specific version."""
    self._transfer_obj.TransferUpdateUtilsPackage()

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

  def _StartPerformanceMonitoringForAUTest(self):
    """Start update_engine performance monitoring script in rootfs update.

    This script is used by autoupdate_EndToEndTest.
    """
    if self._clobber_stateful or not self.is_au_endtoendtest:
      return None

    cmd = ['python', self.REMOTE_UPDATE_ENGINE_PERF_SCRIPT_PATH, '--start-bg']
    try:
      perf_id = self.device.run(cmd).stdout.strip()
      logging.info('update_engine_performance_monitors pid is %s.', perf_id)
      self.perf_id = perf_id
    except cros_build_lib.RunCommandError as e:
      logging.debug('Could not start performance monitoring script: %s', e)

  def _StopPerformanceMonitoringForAUTest(self):
    """Stop the performance monitoring script and save results to file."""
    if self.perf_id is None:
      return
    cmd = ['python', self.REMOTE_UPDATE_ENGINE_PERF_SCRIPT_PATH, '--stop-bg',
           self.perf_id]
    try:
      perf_json_data = self.device.run(cmd).stdout.strip()
      self.device.run(['echo', json.dumps(perf_json_data), '>',
                       self.REMOTE_UPDATE_ENGINE_PERF_RESULTS_PATH])
    except cros_build_lib.RunCommandError as e:
      logging.debug('Could not stop performance monitoring process: %s', e)

  def _CopyHostLogFromDevice(self, nebraska, partial_filename):
    """Copy the hostlog file generated by the nebraska from the device.

    Args:
      nebraska: The nebraska_wrapper.RemoteNebraskawrapper instance.
      partial_filename: A string that will be appended to
        'devserver_hostlog_'. This is to handle current autotests.
    """
    if not self.is_au_endtoendtest:
      return

    nebraska_hostlog_file = os.path.join(
        self.tempdir, 'devserver_hostlog_' + partial_filename)
    nebraska.CollectRequestLogs(nebraska_hostlog_file)

  def _Reboot(self, error_stage, timeout=None):
    try:
      if timeout is None:
        timeout = self.REBOOT_TIMEOUT
      self.device.Reboot(timeout_sec=timeout)
    except cros_build_lib.DieSystemExit:
      raise ChromiumOSUpdateError('Could not recover from reboot at %s' %
                                  error_stage)
    except remote_access.SSHConnectionError:
      raise ChromiumOSUpdateError('Failed to connect at %s' % error_stage)

  def _cgpt(self, flag, kernel, dev='$(rootdev -s -d)'):
    """Return numeric cgpt value for the specified flag, kernel, device."""
    cmd = ['cgpt', 'show', '-n', '-i', '%d' % kernel['kernel'], flag, dev]
    return int(self._RetryCommand(
        cmd, capture_output=True, log_output=True).output.strip())

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
    lsb_release_content = self._RetryCommand(
        ['cat', '/etc/lsb-release'],
        capture_output=True, log_output=True).output.strip()
    regex = r'^CHROMEOS_RELEASE_VERSION=(.+)$'
    return auto_update_util.GetChromeosBuildInfo(
        lsb_release_content=lsb_release_content, regex=regex)

  def _GetReleaseBuilderPath(self):
    """Get release version of the device."""
    lsb_release_content = self._RetryCommand(
        ['cat', '/etc/lsb-release'],
        capture_output=True, log_output=True).output.strip()
    regex = r'^CHROMEOS_RELEASE_BUILDER_PATH=(.+)$'
    return auto_update_util.GetChromeosBuildInfo(
        lsb_release_content=lsb_release_content, regex=regex)

  def CheckVersion(self):
    """Check the image running in DUT has the expected version.

    Returns:
      True if the DUT's image version matches the version that the
      ChromiumOSUpdater tries to update to.
    """
    if not self.update_version:
      return False

    # Use CHROMEOS_RELEASE_BUILDER_PATH to match the build version if it exists
    # in lsb-release, otherwise, continue using CHROMEOS_RELEASE_VERSION.
    release_builder_path = self._GetReleaseBuilderPath()
    if release_builder_path:
      return self.update_version == release_builder_path

    return self.update_version.endswith(self._GetReleaseVersion())

  def _ResetUpdateEngine(self):
    """Resets the host to prepare for a clean update regardless of state."""
    self._RetryCommand(['rm', '-f', self.REMOTE_UPDATED_MARKERFILE_PATH],
                       **self._cmd_kwargs)
    self._RetryCommand(['stop', 'ui'], **self._cmd_kwargs_omit_error)
    self._RetryCommand(['stop', 'update-engine'],
                       **self._cmd_kwargs_omit_error)
    self._RetryCommand(['start', 'update-engine'], **self._cmd_kwargs)

    op = self.GetUpdateStatus(self.device)[0]
    if op != UPDATE_STATUS_IDLE:
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
      self.device.run(['cgpt', 'show', '$(rootdev -s -d)'],
                      **self._cmd_kwargs)
      logging.debug('Dumping crossystem for firmware debugging.')
      self.device.run(['crossystem', '--all'], **self._cmd_kwargs)
      raise RootfsUpdateError(rollback_message)

    # Make sure chromeos-setgoodkernel runs
    try:
      timeout_util.WaitForReturnTrue(
          lambda: (self._GetKernelTries(active_kernel_state) == 0
                   and self._GetKernelSuccess(active_kernel_state)),
          self.KERNEL_UPDATE_TIMEOUT,
          period=5)
    except timeout_util.TimeoutError:
      services_status = self.device.run(
          ['status', 'system-services'], capture_output=True,
          log_output=True).output
      logging.debug('System services_status: %r', services_status)
      if services_status != 'system-services start/running\n':
        event = ('Chrome failed to reach login screen')
      else:
        event = ('update-engine failed to call '
                 'chromeos-setgoodkernel')
      raise RootfsUpdateError(
          'After update and reboot, %s '
          'within %d seconds' % (event, self.KERNEL_UPDATE_TIMEOUT))

  def _CheckVersionToConfirmInstall(self):
    logging.debug('Checking whether the new build is successfully installed...')
    if not self.update_version:
      logging.debug('No update_version is provided if test is executed with'
                    'local nebraska.')
      return True

    # Always try the default check_version method first, this prevents
    # any backward compatibility issue.
    if self.CheckVersion():
      return True

    return auto_update_util.VersionMatch(
        self.update_version, self._GetReleaseVersion())

  def _RetryCommand(self, cmd, **kwargs):
    """Retry commands if SSHConnectionError happens.

    Args:
      cmd: the command to be run by device.
      kwargs: the parameters for device to run the command.

    Returns:
      the output of running the command.
    """
    return retry_util.RetryException(
        remote_access.SSHConnectionError,
        MAX_RETRY,
        self.device.run,
        cmd, delay_sec=DELAY_SEC_FOR_RETRY, **kwargs)

  # TODO(crbug.com/872441): cros_autoupdate in platform/dev-utils package still
  # calls this function, but in fact it needs to call the
  # auto_updater_transfer.Transfer Class's TransferUpdateUtilsPackage() instead.
  # So delete this function once all the callers have been moved.
  def TransferDevServerPackage(self):
    """DEPRECATED."""
    self._transfer_obj.TransferUpdateUtilsPackage()

  def TransferUpdateUtilsPackage(self):
    """DEPRECATED. Use auto_updater_transfer.Transfer Class instead.

    TODO (sanikak): Once this method is removed, remove corresponding tests in
    chromite.lib.auto_updater_unittest.
    """
    self._transfer_obj.TransferUpdateUtilsPackage()

  def TransferRootfsUpdate(self):
    """Transfer files for rootfs update.

    The corresponding payload are copied to the remote device for rootfs
    update.

    DEPRECATED. Use auto_updater_transfer.Transfer Class instead. Until the
    TODOs below are addressed, new calls to
    self._transfer_obj.TransferRootfsUpdate() must be preceded with a
    conditional call to self._FixPayloadPropertiesFile().

    TODO (sanikak): src.platform.dev.cros_updater.py calls
    self.TransferRootfsUpdate() independently. Once the code flow in
    cros_updater.py is cleaned up so that it calls self.RunUpdate(),
    self.TransferRootfsUpdate() can be deprecated fully in favor of
    self.auto_updater_transfer.TransferRootfsUpdate().

    TODO (sanikak): Once this method is removed, remove corresponding tests in
    chromite.lib.auto_updater_unittest.
    """
    # TODO(ahassani): This is not the ideal place to do this, but since any
    # changes to this needs to be reflected in cros_update.py too, just do it
    # for now here.
    self._FixPayloadPropertiesFile()

    self._transfer_obj.TransferRootfsUpdate()

  def TransferStatefulUpdate(self):
    """DEPRECATED. Use auto_updater_transfer.Transfer Class instead.

    Transfer files for stateful update.

    The stateful update bin and the corresponding payloads are copied to the
    target remote device for stateful update.

    TODO (sanikak): Once this method is removed, remove corresponding tests in
    chromite.lib.auto_updater_unittest.
    """
    self._transfer_obj.TransferStatefulUpdate()

  def PreSetupCrOSUpdate(self):
    """Pre-setup for whole auto-update process for cros_host.

    It includes:
      1. Create a file to indicate if provision fails for cros_host.
         The file will be removed by stateful update or full install.
    """
    logging.debug('Start pre-setup for the whole CrOS update process...')
    if not self.is_au_endtoendtest:
      self._RetryCommand(['touch', self.REMOTE_PROVISION_FAILED_FILE_PATH],
                         **self._cmd_kwargs)

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
    self._RetryCommand(['sudo', 'stop', 'ap-update-manager'],
                       **self._cmd_kwargs_omit_error)
    if self._clobber_stateful:
      for folder in self.REMOTE_STATEFUL_PATH_TO_CHECK:
        touch_path = os.path.join(folder, self.REMOTE_STATEFUL_TEST_FILENAME)
        self._RetryCommand(['touch', touch_path], **self._cmd_kwargs)

    self._ResetUpdateEngine()

  def PostCheckStatefulUpdate(self):
    """Post-check for stateful update for CrOS host."""
    logging.debug('Start post check for stateful update...')
    self._Reboot('post check of stateful update')
    if self._clobber_stateful:
      for folder in self.REMOTE_STATEFUL_PATH_TO_CHECK:
        test_file_path = os.path.join(folder,
                                      self.REMOTE_STATEFUL_TEST_FILENAME)
        # If stateful update succeeds, these test files should not exist.
        if self.device.IfFileExists(test_file_path,
                                    **self._cmd_kwargs_omit_error):
          raise StatefulUpdateError('failed to post-check stateful update.')

  def PreSetupRootfsUpdate(self):
    """Pre-setup for rootfs update for CrOS host."""
    logging.debug('Start pre-setup for rootfs update...')
    self._Reboot('pre-setup of rootfs update')
    self._RetryCommand(['sudo', 'stop', 'ap-update-manager'],
                       **self._cmd_kwargs_omit_error)
    self._ResetUpdateEngine()

  def _IsUpdateUtilsPackageInstalled(self):
    """Check whether update-utils package is well installed.

    There's a chance that nebraska package is removed in the middle of
    auto-update process. This function double check it and transfer it if it's
    removed.
    """
    logging.info('Checking whether nebraska files are still on the device...')
    try:
      nebraska_bin = os.path.join(self.device_dev_dir,
                                  self.REMOTE_NEBRASKA_FILENAME)
      if not self.device.IfFileExists(
          nebraska_bin, **self._cmd_kwargs_omit_error):
        logging.info('Nebraska files not found on device. Resending them...')

        self._transfer_obj.TransferUpdateUtilsPackage()

      return True
    except cros_build_lib.RunCommandError as e:
      logging.warning('Failed to verify whether packages still exist: %s', e)
      return False

  # TODO(crbug.com/872441): cros_autoupdate in platform/dev-utils package still
  # calls this function, but in fact it needs to call CheckNebrskaCanRun()
  # instead. So delete this function once all the callers have been moved.
  def CheckDevserverRun(self):
    """DEPRECATED"""
    self.CheckNebraskaCanRun()

  def CheckNebraskaCanRun(self):
    """Check if nebraska can successfully run for ChromiumOSUpdater."""
    self._IsUpdateUtilsPackageInstalled()
    self._CheckNebraskaCanRun()

  def RestoreStateful(self):
    """Restore stateful partition for device."""
    logging.warning('Restoring the stateful partition.')
    self.PreSetupStatefulUpdate()
    self._transfer_obj.TransferStatefulUpdate()
    self.ResetStatefulPartition()
    use_original_build = bool(self.original_payload_dir)
    self.UpdateStateful(use_original_build=use_original_build)
    self.PostCheckStatefulUpdate()
    self._Reboot('stateful partition restoration')
    try:
      self.CheckNebraskaCanRun()
      logging.info('Stateful partition restored.')
    except nebraska_wrapper.NebraskaStartupError as e:
      raise ChromiumOSUpdateError(
          'Unable to restore stateful partition: %s' % e)

  def SetClearTpmOwnerRequest(self):
    """Set clear_tpm_owner_request flag."""
    # The issue is that certain AU tests leave the TPM in a bad state which
    # most commonly shows up in provisioning.  Executing this 'crossystem'
    # command before rebooting clears the problem state during the reboot.
    # It's also worth mentioning that this isn't a complete fix:  The bad
    # TPM state in theory might happen some time other than during
    # provisioning.  Also, the bad TPM state isn't supposed to happen at
    # all; this change is just papering over the real bug.
    logging.info('Setting clear_tpm_owner_request to 1.')
    self._RetryCommand('crossystem clear_tpm_owner_request=1',
                       **self._cmd_kwargs_omit_error)

  def PostCheckRootfsUpdate(self):
    """Post-check for rootfs update for CrOS host."""
    logging.debug('Start post check for rootfs update...')
    active_kernel, inactive_kernel = self._GetKernelState()
    logging.debug('active_kernel= %s, inactive_kernel=%s',
                  active_kernel, inactive_kernel)
    if (self._GetKernelPriority(inactive_kernel) <
        self._GetKernelPriority(active_kernel)):
      raise RootfsUpdateError('Update failed. The priority of the inactive '
                              'kernel partition is less than that of the '
                              'active kernel partition.')

    self.inactive_kernel = inactive_kernel
    if not self.is_au_endtoendtest:
      self.SetClearTpmOwnerRequest()

    # If the source image during an AU test is old, the device will powerwash
    # after applying rootfs. On older devices this is taking longer than the
    # allowed time to reboot. So double reboot timeout for this step only.
    timeout = self.REBOOT_TIMEOUT
    if self.is_au_endtoendtest:
      timeout = self.REBOOT_TIMEOUT * 2
    self._Reboot('post check of rootfs update', timeout=timeout)

  def PostCheckCrOSUpdate(self):
    """Post check for the whole auto-update process."""
    logging.debug('Post check for the whole CrOS update...')
    start_time = time.time()
    # Not use 'sh' here since current device.run cannot recognize
    # the content of $FILE.
    autoreboot_cmd = ('FILE="%s" ; [ -f "$FILE" ] || '
                      '( touch "$FILE" ; start autoreboot )')
    self._RetryCommand(autoreboot_cmd % self.REMOTE_LAB_MACHINE_FILE_PATH,
                       **self._cmd_kwargs)

    # Loop in case the initial check happens before the reboot.
    while True:
      try:
        start_verify_time = time.time()
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
      except RebootVerificationError as e:
        # If a minimum amount of time since starting the check has not
        # occurred, wait and retry.  Use the start of the verification
        # time in case an SSH call takes a long time to return/fail.
        if start_verify_time - start_time < POST_CHECK_SETTLE_SECONDS:
          logging.warning('Delaying for re-check of %s to update to %s (%s)',
                          self.device.hostname, self.update_version, e)
          time.sleep(POST_CHECK_RETRY_SECONDS)
          continue
        raise
      break

    # For autoupdate_EndToEndTest only, we have one extra step to verify.
    if self.is_au_endtoendtest and not self._clobber_stateful:
      self.PostRebootUpdateCheckForAUTest()

  def PostRebootUpdateCheckForAUTest(self):
    """Do another update check after reboot to get the post update hostlog.

    This is only done with autoupdate_EndToEndTest.
    """
    logging.debug('Doing one final update check to get post update hostlog.')
    nebraska_bin = os.path.join(self.device_dev_dir,
                                self.REMOTE_NEBRASKA_FILENAME)
    nebraska = nebraska_wrapper.RemoteNebraskaWrapper(
        self.device, nebraska_bin=nebraska_bin,
        update_metadata_dir=self.device_payload_dir)

    try:
      nebraska.Start()

      nebraska_url = nebraska.GetURL(critical_update=True, no_update=True)
      cmd = [self.REMOTE_UPDATE_ENGINE_BIN_FILENAME, '--check_for_update',
             '--omaha_url="%s"' % nebraska_url]
      self.device.run(cmd, **self._cmd_kwargs)
      op = self.GetUpdateStatus(self.device)[0]
      logging.info('Post update check status: %s', op)
    except Exception as err:
      logging.error('Post reboot update check failed: %s', str(err))
      logging.warning(nebraska.PrintLog() or 'No nebraska log is available.')
    finally:
      self._CopyHostLogFromDevice(nebraska, 'reboot')
      nebraska.Stop()

  def AwaitReboot(self, old_boot_id):
    """Await a reboot, ensuring that it is no longer running old_boot_id.

    Args:
      old_boot_id: The boot_id that must be transitioned away from for success.

    Returns:
      True if the device has successfully rebooted.

    Raises:
      RebootVerificationError if a successful reboot has not occurred.
    """
    logging.debug('Awaiting reboot from %s...', old_boot_id)

    if not self.device.AwaitReboot(old_boot_id):
      raise RebootVerificationError('Device has not rebooted from %s' %
                                    old_boot_id)
    return True
