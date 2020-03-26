# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install/copy the image to the device."""

from __future__ import division
from __future__ import print_function

import os
import re
import shutil
import subprocess
import sys
import tempfile

from chromite.lib import constants
from chromite.lib import auto_updater
from chromite.lib import auto_updater_transfer
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import dev_server_wrapper as ds_wrapper
from chromite.lib import operation
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import remote_access

from chromite.lib.paygen import paygen_payload_lib
from chromite.lib.paygen import paygen_stateful_payload_lib

from chromite.lib.xbuddy import artifact_info


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


DEVSERVER_STATIC_DIR = path_util.FromChrootPath(
    os.path.join(constants.CHROOT_SOURCE_ROOT, 'devserver', 'static'))


class UsbImagerOperation(operation.ProgressBarOperation):
  """Progress bar for flashing image to operation."""

  def __init__(self, image):
    super(UsbImagerOperation, self).__init__()
    self._size = os.path.getsize(image)
    self._transferred = 0
    self._bytes = re.compile(r'(\d+) bytes')

  def _GetDDPid(self):
    """Get the Pid of dd."""
    try:
      pids = cros_build_lib.run(['pgrep', 'dd'], capture_output=True,
                                print_cmd=False, encoding='utf-8').stdout
      for pid in pids.splitlines():
        if osutils.IsChildProcess(int(pid), name='dd'):
          return int(pid)
      return -1
    except cros_build_lib.RunCommandError:
      # If dd isn't still running, then we assume that it is finished.
      return -1

  def _PingDD(self, dd_pid):
    """Send USR1 signal to dd to get status update."""
    try:
      cmd = ['kill', '-USR1', str(dd_pid)]
      cros_build_lib.sudo_run(cmd, print_cmd=False)
    except cros_build_lib.RunCommandError:
      # Here we assume that dd finished in the background.
      return

  def ParseOutput(self, output=None):
    """Parse the output of dd to update progress bar."""
    dd_pid = self._GetDDPid()
    if dd_pid == -1:
      return

    self._PingDD(dd_pid)

    if output is None:
      stdout = self._stdout.read()
      stderr = self._stderr.read()
      output = stdout + stderr

    match = self._bytes.search(output)
    if match:
      self._transferred = int(match.groups()[0])

    self.ProgressBar(self._transferred / self._size)


def _IsFilePathGPTDiskImage(file_path, require_pmbr=False):
  """Determines if a file is a valid GPT disk.

  Args:
    file_path: Path to the file to test.
    require_pmbr: Whether to require a PMBR in LBA0.
  """
  if os.path.isfile(file_path):
    with open(file_path, 'rb') as image_file:
      if require_pmbr:
        # Seek to the end of LBA0 and look for the PMBR boot signature.
        image_file.seek(0x1fe)
        if image_file.read(2) != b'\x55\xaa':
          return False
        # Current file position is start of LBA1 now.
      else:
        # Seek to LBA1 where the GPT starts.
        image_file.seek(0x200)

      # See if there's a GPT here.
      if image_file.read(8) == b'EFI PART':
        return True

  return False


def _ChooseImageFromDirectory(dir_path):
  """Lists all image files in |dir_path| and ask user to select one.

  Args:
    dir_path: Path to the directory.
  """
  images = sorted([x for x in os.listdir(dir_path) if
                   _IsFilePathGPTDiskImage(os.path.join(dir_path, x))])
  idx = 0
  if not images:
    raise ValueError('No image found in %s.' % dir_path)
  elif len(images) > 1:
    idx = cros_build_lib.GetChoice(
        'Multiple images found in %s. Please select one to continue:' % (
            (dir_path,)),
        images)

  return os.path.join(dir_path, images[idx])


class FlashError(Exception):
  """Thrown when there is an unrecoverable error during flash."""


class USBImager(object):
  """Copy image to the target removable device."""

  def __init__(self, device, board, image, debug=False, install=False,
               yes=False):
    """Initalizes USBImager."""
    self.device = device
    self.board = board if board else cros_build_lib.GetDefaultBoard()
    self.image = image
    self.debug = debug
    self.debug_level = logging.DEBUG if debug else logging.INFO
    self.install = install
    self.yes = yes

  def DeviceNameToPath(self, device_name):
    return '/dev/%s' % device_name

  def GetRemovableDeviceDescription(self, device):
    """Returns a informational description of the removable |device|.

    Args:
      device: the device name (e.g. sdc).

    Returns:
      A string describing |device| (e.g. Patriot Memory 7918 MB).
    """
    desc = [
        osutils.GetDeviceInfo(device, keyword='manufacturer'),
        osutils.GetDeviceInfo(device, keyword='product'),
        osutils.GetDeviceSize(self.DeviceNameToPath(device)),
        '(%s)' % self.DeviceNameToPath(device),
    ]
    return ' '.join([x for x in desc if x])

  def ListAllRemovableDevices(self):
    """Returns a list of removable devices.

    Returns:
      A list of device names (e.g. ['sdb', 'sdc']).
    """
    devices = osutils.ListBlockDevices()
    removable_devices = []
    for d in devices:
      if d.TYPE == 'disk' and d.RM == '1':
        removable_devices.append(d.NAME)

    return removable_devices

  def ChooseRemovableDevice(self, devices):
    """Lists all removable devices and asks user to select/confirm.

    Args:
      devices: a list of device names (e.g. ['sda', 'sdb']).

    Returns:
      The device name chosen by the user.
    """
    idx = cros_build_lib.GetChoice(
        'Removable device(s) found. Please select/confirm to continue:',
        [self.GetRemovableDeviceDescription(x) for x in devices])

    return devices[idx]

  def InstallImageToDevice(self, image, device):
    """Installs |image| to the removable |device|.

    Args:
      image: Path to the image to copy.
      device: Device to copy to.
    """
    cmd = [
        'chromeos-install',
        '--yes',
        '--skip_src_removable',
        '--skip_dst_removable',
        '--payload_image=%s' % image,
        '--dst=%s' % device,
        '--skip_postinstall',
    ]
    cros_build_lib.sudo_run(cmd,
                            print_cmd=True,
                            debug_level=logging.NOTICE,
                            stderr=subprocess.STDOUT,
                            log_output=True)

  def CopyImageToDevice(self, image, device):
    """Copies |image| to the removable |device|.

    Args:
      image: Path to the image to copy.
      device: Device to copy to.
    """
    cmd = ['dd', 'if=%s' % image, 'of=%s' % device, 'bs=4M', 'iflag=fullblock',
           'oflag=direct', 'conv=fdatasync']
    if logging.getLogger().getEffectiveLevel() <= logging.NOTICE:
      op = UsbImagerOperation(image)
      op.Run(cros_build_lib.sudo_run, cmd, debug_level=logging.NOTICE,
             encoding='utf-8', update_period=0.5)
    else:
      cros_build_lib.sudo_run(
          cmd, debug_level=logging.NOTICE,
          print_cmd=logging.getLogger().getEffectiveLevel() < logging.NOTICE)

    # dd likely didn't put the backup GPT in the last block. sfdisk fixes this
    # up for us with a 'write' command, so we have a standards-conforming GPT.
    # Ignore errors because sfdisk (util-linux < v2.32) isn't always happy to
    # fix GPT sanity issues.
    cros_build_lib.sudo_run(['sfdisk', device], input='write\n',
                            check=False,
                            debug_level=self.debug_level)

    cros_build_lib.sudo_run(['partx', '-u', device],
                            debug_level=self.debug_level)
    cros_build_lib.sudo_run(['sync', '-d', device],
                            debug_level=self.debug_level)

  def _GetImagePath(self):
    """Returns the image path to use."""
    image_path = translated_path = None
    if os.path.isfile(self.image):
      if not self.yes and not _IsFilePathGPTDiskImage(self.image):
        # TODO(wnwen): Open the tarball and if there is just one file in it,
        #     use that instead. Existing code in upload_symbols.py.
        if cros_build_lib.BooleanPrompt(
            prolog='The given image file is not a valid disk image. Perhaps '
                   'you forgot to untar it.',
            prompt='Terminate the current flash process?'):
          raise FlashError('Update terminated by user.')
      image_path = self.image
    elif os.path.isdir(self.image):
      # Ask user which image (*.bin) in the folder to use.
      image_path = _ChooseImageFromDirectory(self.image)
    else:
      # Translate the xbuddy path to get the exact image to use.
      translated_path, _ = ds_wrapper.GetImagePathWithXbuddy(
          self.image, self.board, static_dir=DEVSERVER_STATIC_DIR)
      image_path = ds_wrapper.TranslatedPathToLocalPath(
          translated_path, DEVSERVER_STATIC_DIR)

    logging.info('Using image %s', translated_path or image_path)
    return image_path

  def Run(self):
    """Image the removable device."""
    devices = self.ListAllRemovableDevices()

    if self.device:
      # If user specified a device path, check if it exists.
      if not os.path.exists(self.device):
        raise FlashError('Device path %s does not exist.' % self.device)

      # Then check if it is removable.
      if self.device not in [self.DeviceNameToPath(x) for x in devices]:
        msg = '%s is not a removable device.' % self.device
        if not (self.yes or cros_build_lib.BooleanPrompt(
            default=False, prolog=msg)):
          raise FlashError('You can specify usb:// to choose from a list of '
                           'removable devices.')
    target = None
    if self.device:
      # Get device name from path (e.g. sdc in /dev/sdc).
      target = self.device.rsplit(os.path.sep, 1)[-1]
    elif devices:
      # Ask user to choose from the list.
      target = self.ChooseRemovableDevice(devices)
    else:
      raise FlashError('No removable devices detected.')

    image_path = self._GetImagePath()
    try:
      device = self.DeviceNameToPath(target)
      if self.install:
        self.InstallImageToDevice(image_path, device)
      else:
        self.CopyImageToDevice(image_path, device)
    except cros_build_lib.RunCommandError:
      logging.error('Failed copying image to device %s',
                    self.DeviceNameToPath(target))


class FileImager(USBImager):
  """Copy image to the target path."""

  def Run(self):
    """Copy the image to the path specified by self.device."""
    if not os.path.isdir(os.path.dirname(self.device)):
      raise FlashError('Parent of path %s is not a directory.' % self.device)

    image_path = self._GetImagePath()
    if os.path.isdir(self.device):
      logging.info('Copying to %s',
                   os.path.join(self.device, os.path.basename(image_path)))
    else:
      logging.info('Copying to %s', self.device)
    try:
      shutil.copy(image_path, self.device)
    except IOError:
      logging.error('Failed to copy image %s to %s', image_path, self.device)


class RemoteDeviceUpdater(object):
  """Performs update on a remote device."""
  STATEFUL_UPDATE_BIN = '/usr/bin/stateful_update'
  UPDATE_ENGINE_BIN = 'update_engine_client'
  # Root working directory on the device. This directory is in the
  # stateful partition and thus has enough space to store the payloads.
  DEVICE_BASE_DIR = '/usr/local/tmp/cros-flash'
  UPDATE_CHECK_INTERVAL_PROGRESSBAR = 0.5
  UPDATE_CHECK_INTERVAL_NORMAL = 10

  def __init__(self, ssh_hostname, ssh_port, image, stateful_update=True,
               rootfs_update=True, clobber_stateful=False, reboot=True,
               board=None, src_image_to_delta=None, wipe=True, debug=False,
               yes=False, force=False, ssh_private_key=None, ping=True,
               disable_verification=False, send_payload_in_parallel=False,
               experimental_au=False):
    """Initializes RemoteDeviceUpdater"""
    if not stateful_update and not rootfs_update:
      raise ValueError('No update operation to perform; either stateful or'
                       ' rootfs partitions must be updated.')
    self.tempdir = tempfile.mkdtemp(prefix='cros-flash')
    self.ssh_hostname = ssh_hostname
    self.ssh_port = ssh_port
    self.image = image
    self.board = board
    self.src_image_to_delta = src_image_to_delta
    self.do_stateful_update = stateful_update
    self.do_rootfs_update = rootfs_update
    self.disable_verification = disable_verification
    self.clobber_stateful = clobber_stateful
    self.reboot = reboot
    self.debug = debug
    self.ssh_private_key = ssh_private_key
    self.ping = ping
    # Do not wipe if debug is set.
    self.wipe = wipe and not debug
    self.yes = yes
    self.force = force
    self.send_payload_in_parallel = send_payload_in_parallel
    self.experimental_au = experimental_au

  def Cleanup(self):
    """Cleans up the temporary directory."""
    if self.wipe:
      logging.info('Cleaning up temporary working directory...')
      osutils.RmDir(self.tempdir)
    else:
      logging.info('You can find the log files and/or payloads in %s',
                   self.tempdir)

  def GetPayloadDir(self, device):
    """Get directory of payload for update.

    This method is used to obtain the directory of payload for cros-flash. The
    given path 'self.image' is passed in when initializing RemoteDeviceUpdater.

    If self.image is a directory, we directly use the provided update payload(s)
    in this directory.

    If self.image is an image, we will generate payloads for it and put them in
    our temporary directory. The reason is that people may modify a local image
    or override it (on the same path) with a different image, so in order to be
    safe each time we need to generate the payloads and not cache them.

    If non of the above cases, we use the xbuddy to first obtain the image path
    (and possibly download it). Then we will generate the payloads in the same
    directory the image is located. The reason is that this is what devserver
    used to do. The path to the image generated by the devserver (or xbuddy) is
    unique and normally nobody override its image with a different one. That is
    why I think it is safe to put the payloads next to the image. This is a poor
    man's version of caching but it makes cros flash faster for users who flash
    the same image multiple times (without doing any change to the image).

    Args:
      device: A ChromiumOSDevice object.

    Returns:
      A string payload_dir, that represents the payload directory.
    """
    if os.path.isdir(self.image):
      # The given path is a directory.
      logging.info('Using provided payloads in %s', self.image)
      return self.image

    image_path = None
    if os.path.isfile(self.image):
      # The given path is an image.
      image_path = self.image
      payload_dir = self.tempdir
    else:
      # Assuming it is an xbuddy path.
      self.board = cros_build_lib.GetBoard(device_board=device.board,
                                           override_board=self.board,
                                           force=self.yes,
                                           strict=True)
      if not self.force and self.board != device.board:
        # If a board was specified, it must be compatible with the device.
        raise FlashError('Device (%s) is incompatible with board %s' %
                         (device.board, self.board))
      logging.info('Board is %s', self.board)


      # TODO(crbug.com/872441): Once devserver code has been moved to chromite,
      # use xbuddy library directly instead of the devserver_wrapper.
      # Fetch the full payload and properties, and stateful files. If this
      # fails, fallback to downloading the image.
      try:
        translated_path, _ = ds_wrapper.GetImagePathWithXbuddy(
            os.path.join(self.image, artifact_info.FULL_PAYLOAD), self.board,
            static_dir=DEVSERVER_STATIC_DIR, silent=True)
        payload_dir = os.path.dirname(
            ds_wrapper.TranslatedPathToLocalPath(translated_path,
                                                 DEVSERVER_STATIC_DIR))
        ds_wrapper.GetImagePathWithXbuddy(
            os.path.join(self.image, artifact_info.STATEFUL_PAYLOAD),
            self.board, static_dir=DEVSERVER_STATIC_DIR, silent=True)
        fetch_image = False
      except (ds_wrapper.ImagePathError, ds_wrapper.ArtifactDownloadError):
        logging.info('Could not find full_payload or stateful for "%s"',
                     self.image)
        fetch_image = True

      # We didn't find the full_payload, attempt to download the image.
      if fetch_image:
        translated_path, _ = ds_wrapper.GetImagePathWithXbuddy(
            self.image, self.board, static_dir=DEVSERVER_STATIC_DIR)
        image_path = ds_wrapper.TranslatedPathToLocalPath(
            translated_path, DEVSERVER_STATIC_DIR)
        payload_dir = os.path.join(os.path.dirname(image_path), 'payloads')
        logging.notice('Using image path %s and payload directory %s',
                       image_path, payload_dir)

    # Generate rootfs and stateful update payloads if they do not exist.
    payload_path = os.path.join(payload_dir,
                                auto_updater_transfer.ROOTFS_FILENAME)
    if not os.path.exists(payload_path):
      paygen_payload_lib.GenerateUpdatePayload(
          image_path, payload_path, src_image=self.src_image_to_delta)
    if not os.path.exists(os.path.join(
        payload_dir, auto_updater_transfer.STATEFUL_FILENAME)):
      paygen_stateful_payload_lib.GenerateStatefulPayload(image_path,
                                                          payload_dir)
    return payload_dir

  def Run(self):
    """Perform remote device update.

    The update process includes:
    1. initialize a device instance for the given remote device.
    2. achieve payload_dir which contains the required payloads for updating.
    3. initialize an auto-updater instance to do RunUpdate().
    4. After auto-update, all temp files and dir will be cleaned up.
    """
    try:
      with remote_access.ChromiumOSDeviceHandler(
          self.ssh_hostname, port=self.ssh_port, base_dir=self.DEVICE_BASE_DIR,
          private_key=self.ssh_private_key, ping=self.ping) as device:

        try:
          # Get payload directory
          payload_dir = self.GetPayloadDir(device)

          # Do auto-update
          chromeos_AU = auto_updater.ChromiumOSUpdater(
              device=device,
              build_name=None,
              payload_dir=payload_dir,
              tempdir=self.tempdir,
              do_rootfs_update=self.do_rootfs_update,
              do_stateful_update=self.do_stateful_update,
              reboot=self.reboot,
              disable_verification=self.disable_verification,
              clobber_stateful=self.clobber_stateful,
              yes=self.yes,
              send_payload_in_parallel=self.send_payload_in_parallel,
              experimental_au=self.experimental_au,
              transfer_class=auto_updater_transfer.LocalTransfer)
          chromeos_AU.CheckPayloads()
          chromeos_AU.PreparePayloadPropsFile()
          chromeos_AU.RunUpdate()

        except Exception:
          logging.error('Device update failed.')
          lsb_entries = sorted(device.lsb_release.items())
          logging.info(
              'Following are the LSB version details of the device:\n%s',
              '\n'.join('%s=%s' % (k, v) for k, v in lsb_entries))
          raise

        logging.notice('Update performed successfully.')

    except remote_access.RemoteAccessException:
      logging.error('Remote device failed to initialize.')
      raise

    finally:
      self.Cleanup()


def Flash(device, image, board=None, install=False, src_image_to_delta=None,
          rootfs_update=True, stateful_update=True, clobber_stateful=False,
          reboot=True, wipe=True, ssh_private_key=None, ping=True,
          disable_rootfs_verification=False, clear_cache=False, yes=False,
          force=False, debug=False, send_payload_in_parallel=False,
          experimental_au=False):
  """Flashes a device, USB drive, or file with an image.

  This provides functionality common to `cros flash` and `brillo flash`
  so that they can parse the commandline separately but still use the
  same underlying functionality.

  Args:
    device: commandline.Device object; None to use the default device.
    image: Path (string) to the update image. Can be a local or xbuddy path;
        non-existant local paths are converted to xbuddy.
    board: Board to use; None to automatically detect.
    install: Install to USB using base disk layout; USB |device| scheme only.
    src_image_to_delta: Local path to an image to be used as the base to
        generate delta payloads; SSH |device| scheme only.
    rootfs_update: Update rootfs partition; SSH |device| scheme only.
    stateful_update: Update stateful partition; SSH |device| scheme only.
    clobber_stateful: Clobber stateful partition; SSH |device| scheme only.
    reboot: Reboot device after update; SSH |device| scheme only.
    wipe: Wipe temporary working directory; SSH |device| scheme only.
    ssh_private_key: Path to an SSH private key file; None to use test keys.
    ping: Ping the device before attempting update; SSH |device| scheme only.
    disable_rootfs_verification: Remove rootfs verification after update; SSH
        |device| scheme only.
    clear_cache: Clear the devserver static directory.
    yes: Assume "yes" for any prompt.
    force: Ignore sanity checks and prompts. Overrides |yes| if True.
    debug: Print additional debugging messages.
    send_payload_in_parallel: Transfer payloads in chunks in parallel to speed
        up transmissions for long haul between endpoints.
    experimental_au: Use the experimental features auto updater. It should be
        deprecated once crbug.com/872441 is fixed.

  Raises:
    FlashError: An unrecoverable error occured.
    ValueError: Invalid parameter combination.
  """
  if force:
    yes = True

  if clear_cache:
    logging.info('Clearing the cache...')
    ds_wrapper.DevServerWrapper.WipeStaticDirectory(DEVSERVER_STATIC_DIR)

  try:
    osutils.SafeMakedirsNonRoot(DEVSERVER_STATIC_DIR)
  except OSError:
    logging.error('Failed to create %s', DEVSERVER_STATIC_DIR)

  if install:
    if not device or device.scheme != commandline.DEVICE_SCHEME_USB:
      raise ValueError(
          '--install can only be used when writing to a USB device')
    if not cros_build_lib.IsInsideChroot():
      raise ValueError('--install can only be used inside the chroot')

  if not device or device.scheme == commandline.DEVICE_SCHEME_SSH:
    if device:
      hostname, port = device.hostname, device.port
    else:
      hostname, port = None, None
    logging.notice('Preparing to update the remote device %s', hostname)
    updater = RemoteDeviceUpdater(
        hostname,
        port,
        image,
        board=board,
        src_image_to_delta=src_image_to_delta,
        rootfs_update=rootfs_update,
        stateful_update=stateful_update,
        clobber_stateful=clobber_stateful,
        reboot=reboot,
        wipe=wipe,
        debug=debug,
        yes=yes,
        force=force,
        ssh_private_key=ssh_private_key,
        ping=ping,
        disable_verification=disable_rootfs_verification,
        send_payload_in_parallel=send_payload_in_parallel,
        experimental_au=experimental_au)
    updater.Run()
  elif device.scheme == commandline.DEVICE_SCHEME_USB:
    path = osutils.ExpandPath(device.path) if device.path else ''
    logging.info('Preparing to image the removable device %s', path)
    imager = USBImager(path,
                       board,
                       image,
                       debug=debug,
                       install=install,
                       yes=yes)
    imager.Run()
  elif device.scheme == commandline.DEVICE_SCHEME_FILE:
    logging.info('Preparing to copy image to %s', device.path)
    imager = FileImager(device.path,
                        board,
                        image,
                        debug=debug,
                        yes=yes)
    imager.Run()
