# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing functions to transfer files onto a remote device.

LocalTransfer includes:

  ----Precheck---
  * Pre-check payload's existence before auto-update.

  ----Tranfer----
  * Transfer update-utils (nebraska, et. al.) package at first.
  * Transfer rootfs update files if rootfs update is required.
  * Transfer stateful update files if stateful update is required.
  * @retry to all transfer functions.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import abc
import os
import shutil

import six

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import nebraska_wrapper
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import retry_util

# Naming conventions for global variables:
#   Path on remote host with slash: REMOTE_XXX_PATH
#   File on local server without slash: LOCAL_XXX_FILENAME
#   Path on local server: LOCAL_XXX_PATH

# Max number of the times for retry:
# 1. for transfer functions to be retried.
# 2. for some retriable commands to be retried.
_MAX_RETRY = 5

# The delay between retriable tasks.
_DELAY_SEC_FOR_RETRY = 5

# Update file names for rootfs+kernel and stateful partitions.
ROOTFS_FILENAME = 'update.gz'
STATEFUL_FILENAME = 'stateful.tgz'


class ChromiumOSTransferError(Exception):
  """Thrown when there is a general transfer specific error."""


def GetPayloadPropertiesFileName(payload):
  """Returns the payload properties file given the path to the payload."""
  return payload + '.json'


class Transfer(six.with_metaclass(abc.ABCMeta, object)):
  """Abstract Base Class that handles payload precheck and transfer."""

  @abc.abstractmethod
  def CheckPayloads(self):
    """Verify that all required payloads are in |self.payload_dir|."""

  @abc.abstractmethod
  def TransferUpdateUtilsPackage(self):
    """Transfer update-utils package to work directory of the remote device."""

  @abc.abstractmethod
  def TransferRootfsUpdate(self):
    """Transfer files for rootfs update.

    The corresponding payloads are copied to the remote device for rootfs
    update.
    """

  @abc.abstractmethod
  def TransferStatefulUpdate(self):
    """Transfer files for stateful update.

    The stateful update bin and the corresponding payloads are copied to the
    target remote device for stateful update.
    """


class LocalTransfer(Transfer):
  """Abstracts logic that handles transferring local files to the DUT."""

  # Stateful update files.
  LOCAL_STATEFUL_UPDATE_FILENAME = 'stateful_update'
  LOCAL_CHROOT_STATEFUL_UPDATE_PATH = '/usr/bin/stateful_update'
  REMOTE_STATEFUL_UPDATE_PATH = '/usr/local/bin/stateful_update'

  PAYLOAD_DIR_NAME = 'payloads'

  def __init__(self, device, payload_dir, tempdir, device_restore_dir,
               payload_name, cmd_kwargs, device_payload_dir, dev_dir='',
               payload_mode='scp', original_payload_dir=None,
               transfer_stateful_update=True, transfer_rootfs_update=True):
    """Initialize a LocalTransfer to handle tranferring files from local to DUT.

    Args:
      device: the ChromiumOSDevice to be updated.
      device_payload_dir: Path to the payload directory in the device's work
          directory.
      tempdir: the temp directory in caller, not in the device. For example,
          the tempdir for cros flash is /tmp/cros-flash****/, used to
          temporarily keep files when transferring update-utils package, and
          reserve nebraska and update engine logs.
      dev_dir: the directory of the nebraska that runs the CrOS auto-update.
      payload_dir: the directory of payload(s).
      payload_mode: The payload mode - it can be 'parallel' or 'scp'.
      cmd_kwargs: keyword arguments that are sent along with the commands that
          are run on the device.
      original_payload_dir: The directory containing payloads whose version is
          the same as current host's rootfs partition. If it's None, will first
          try installing the matched stateful.tgz with the host's rootfs
          Partition when restoring stateful. Otherwise, install the target
          stateful.tgz.
      device_restore_dir: Path to the old payload directory in the device's work
          directory.
      payload_name: Filename of exact payload file to use for update.
      transfer_stateful_update: whether to do stateful partition update.
          The default is True.
      transfer_rootfs_update: whether to do rootfs partition update. The default
          is True.
    """
    self._device = device
    self._device_payload_dir = device_payload_dir
    self._tempdir = tempdir
    self._dev_dir = dev_dir
    self._payload_dir = payload_dir
    self._payload_mode = payload_mode
    self._cmd_kwargs = cmd_kwargs
    self._original_payload_dir = original_payload_dir
    self._device_restore_dir = device_restore_dir
    self._payload_name = payload_name
    self._transfer_stateful_update = transfer_stateful_update
    self._transfer_rootfs_update = transfer_rootfs_update
    self._stateful_update_bin = None

  def CheckPayloads(self):
    """Verify that all required payloads are in |self.payload_dir|."""
    logging.debug('Checking if payloads have been stored in directory %s...',
                  self._payload_dir)
    filenames = []

    if self._transfer_rootfs_update:
      filenames += [self._payload_name,
                    GetPayloadPropertiesFileName(self._payload_name)]

    if self._transfer_stateful_update:
      filenames += [STATEFUL_FILENAME]

    for fname in filenames:
      payload = os.path.join(self._payload_dir, fname)
      if not os.path.exists(payload):
        raise ChromiumOSTransferError('Payload %s does not exist!' % payload)

  def _EnsureDeviceDirectory(self, directory):
    """Mkdir the directory no matther whether this directory exists on host.

    Args:
      directory: the directory to be made on the device.
    """
    self._device.RunCommand(['mkdir', '-p', directory], **self._cmd_kwargs)

  def _TransferUpdateUtilsPackage(self):
    """Transfer update-utils package to work directory of the remote device."""
    files_to_copy = (nebraska_wrapper.NEBRASKA_SOURCE_FILE,)
    logging.info('Copying these files to device: %s', files_to_copy)
    source_dir = os.path.join(self._tempdir, 'src')
    osutils.SafeMakedirs(source_dir)
    for f in files_to_copy:
      shutil.copy2(f, source_dir)

    # Make sure the device.work_dir exist after any installation and reboot.
    self._EnsureDeviceDirectory(self._device.work_dir)
    # Python packages are plain text files so we chose rsync --compress.
    self._device.CopyToWorkDir(source_dir, mode='rsync', log_output=True,
                               **self._cmd_kwargs)

  def TransferUpdateUtilsPackage(self):
    """Transfer update-utils package to work directory of the remote device."""
    retry_util.RetryException(
        cros_build_lib.RunCommandError,
        _MAX_RETRY,
        self._TransferUpdateUtilsPackage,
        delay_sec=_DELAY_SEC_FOR_RETRY)

  def _TransferRootfsUpdate(self):
    """Transfer files for rootfs update.

    Copy the update payload to the remote device for rootfs update.
    """
    self._EnsureDeviceDirectory(self._device_payload_dir)
    logging.info('Copying rootfs payload to device...')
    payload = os.path.join(self._payload_dir, self._payload_name)
    self._device.CopyToWorkDir(payload, self.PAYLOAD_DIR_NAME,
                               mode=self._payload_mode,
                               log_output=True, **self._cmd_kwargs)
    payload_properties_path = GetPayloadPropertiesFileName(payload)
    self._device.CopyToWorkDir(payload_properties_path, self.PAYLOAD_DIR_NAME,
                               mode=self._payload_mode,
                               log_output=True, **self._cmd_kwargs)

  def TransferRootfsUpdate(self):
    """Transfer files for rootfs update.

    The corresponding payload(s) are copied to the remote device for rootfs
    update.
    """
    retry_util.RetryException(
        cros_build_lib.RunCommandError,
        _MAX_RETRY,
        self._TransferRootfsUpdate,
        delay_sec=_DELAY_SEC_FOR_RETRY)

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
    stateful_update_path = path_util.FromChrootPath(
        self.LOCAL_CHROOT_STATEFUL_UPDATE_PATH)

    if not os.path.exists(stateful_update_path):
      logging.warning('Could not find chroot stateful_update script in %s, '
                      'falling back to the client copy.', stateful_update_path)
      stateful_update_path = os.path.join(self._dev_dir,
                                          self.LOCAL_STATEFUL_UPDATE_FILENAME)
      if os.path.exists(stateful_update_path):
        logging.debug('Use stateful_update script in devserver path: %s',
                      stateful_update_path)
        return True, stateful_update_path

      logging.debug('Cannot find stateful_update script, will use the script '
                    'on the host')
      return False, self.REMOTE_STATEFUL_UPDATE_PATH
    else:
      return True, stateful_update_path

  def _TransferStatefulUpdate(self):
    """Transfer files for stateful update.

    The stateful update bin and the corresponding payloads are copied to the
    target remote device for stateful update.
    """
    logging.debug('Checking whether file stateful_update_bin needs to be '
                  'transferred to device...')
    need_transfer, stateful_update_bin = self._GetStatefulUpdateScript()
    if need_transfer:
      logging.info('Copying stateful_update binary to device...')
      # stateful_update is a tiny uncompressed text file, so use rsync.
      self._device.CopyToWorkDir(stateful_update_bin, mode='rsync',
                                 log_output=True, **self._cmd_kwargs)
      self._stateful_update_bin = os.path.join(
          self._device.work_dir,
          os.path.basename(self.LOCAL_CHROOT_STATEFUL_UPDATE_PATH))
    else:
      self._stateful_update_bin = stateful_update_bin

    if self._original_payload_dir:
      logging.info('Copying original stateful payload to device...')
      original_payload = os.path.join(
          self._original_payload_dir, STATEFUL_FILENAME)
      self._EnsureDeviceDirectory(self._device_restore_dir)
      self._device.CopyToDevice(original_payload, self._device_restore_dir,
                                mode=self._payload_mode, log_output=True,
                                **self._cmd_kwargs)

    logging.info('Copying target stateful payload to device...')
    payload = os.path.join(self._payload_dir, STATEFUL_FILENAME)
    self._device.CopyToWorkDir(payload, mode=self._payload_mode,
                               log_output=True, **self._cmd_kwargs)

  def TransferStatefulUpdate(self):
    """Transfer files for stateful update.

    The stateful update bin and the corresponding payloads are copied to the
    target remote device for stateful update.
    """
    retry_util.RetryException(
        cros_build_lib.RunCommandError,
        _MAX_RETRY,
        self._TransferStatefulUpdate,
        delay_sec=_DELAY_SEC_FOR_RETRY)
    return self._stateful_update_bin
