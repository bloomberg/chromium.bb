# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install/copy the image to the device."""

from __future__ import print_function

import cStringIO
import os
import re
import shutil
import tempfile
import time

from chromite.cbuildbot import constants
from chromite.cli import command
from chromite.lib import blueprint_lib
from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import dev_server_wrapper as ds_wrapper
from chromite.lib import operation
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import project_sdk
from chromite.lib import remote_access
from chromite.lib import workspace_lib


_DEVSERVER_STATIC_DIR = path_util.FromChrootPath(
    os.path.join(constants.CHROOT_SOURCE_ROOT, 'devserver', 'static'))


class UsbImagerOperation(operation.ProgressBarOperation):
  """Progress bar for flashing image to operation."""

  def __init__(self, image):
    super(UsbImagerOperation, self).__init__()
    self._size = os.path.getsize(image)
    self._transferred = 0.
    self._bytes = re.compile(r'(\d+) bytes')

  def _GetDDPid(self):
    """Get the Pid of dd."""
    try:
      pids = cros_build_lib.RunCommand(['pgrep', 'dd'], capture_output=True,
                                       print_cmd=False).output
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
      cros_build_lib.SudoRunCommand(cmd, print_cmd=False)
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
      self._transferred = match.groups()[0]

    self.ProgressBar(float(self._transferred) / self._size)


def _IsFilePathGPTDiskImage(file_path):
  """Determines if a file is a valid GPT disk.

  Args:
    file_path: Path to the file to test.
  """
  if os.path.isfile(file_path):
    with cros_build_lib.Open(file_path) as image_file:
      image_file.seek(0x1fe)
      if image_file.read(10) == '\x55\xaaEFI PART':
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
  if len(images) == 0:
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

  def __init__(self, device, board, image, workspace_path=None,
               sdk_version=None, debug=False, install=False, yes=False):
    """Initalizes USBImager."""
    self.device = device
    self.board = board if board else cros_build_lib.GetDefaultBoard()
    self.image = image
    self.workspace_path = workspace_path
    self.sdk_version = sdk_version
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
    cros_build_lib.SudoRunCommand(cmd)

  def CopyImageToDevice(self, image, device):
    """Copies |image| to the removable |device|.

    Args:
      image: Path to the image to copy.
      device: Device to copy to.
    """
    cmd = ['dd', 'if=%s' % image, 'of=%s' % device, 'bs=4M', 'iflag=fullblock',
           'oflag=sync']
    if logging.getLogger().getEffectiveLevel() <= logging.NOTICE:
      op = UsbImagerOperation(image)
      op.Run(cros_build_lib.SudoRunCommand, cmd, debug_level=logging.NOTICE,
             update_period=0.5)
    else:
      cros_build_lib.SudoRunCommand(
          cmd, debug_level=logging.NOTICE,
          print_cmd=logging.getLogger().getEffectiveLevel() < logging.NOTICE)

    cros_build_lib.SudoRunCommand(['sync'], debug_level=self.debug_level)

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
          self.image, self.board, version=self.sdk_version,
          static_dir=_DEVSERVER_STATIC_DIR)
      image_path = ds_wrapper.TranslatedPathToLocalPath(
          translated_path, _DEVSERVER_STATIC_DIR,
          workspace_path=self.workspace_path)

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
    if not os.path.exists(self.device):
      raise FlashError('Path %s does not exist.' % self.device)

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
  DEVSERVER_FILENAME = 'devserver.py'
  STATEFUL_UPDATE_BIN = '/usr/bin/stateful_update'
  UPDATE_ENGINE_BIN = 'update_engine_client'
  # Root working directory on the device. This directory is in the
  # stateful partition and thus has enough space to store the payloads.
  DEVICE_BASE_DIR = '/mnt/stateful_partition/cros-flash'
  UPDATE_CHECK_INTERVAL_PROGRESSBAR = 0.5
  UPDATE_CHECK_INTERVAL_NORMAL = 10

  def __init__(self, ssh_hostname, ssh_port, image, stateful_update=True,
               rootfs_update=True, clobber_stateful=False, reboot=True,
               board=None, workspace_path=None, src_image_to_delta=None,
               wipe=True, debug=False, yes=False, force=False, ping=True,
               disable_verification=False, sdk_version=None):
    """Initializes RemoteDeviceUpdater"""
    if not stateful_update and not rootfs_update:
      raise ValueError('No update operation to perform; either stateful or'
                       ' rootfs partitions must be updated.')
    self.tempdir = tempfile.mkdtemp(prefix='cros-flash')
    self.ssh_hostname = ssh_hostname
    self.ssh_port = ssh_port
    self.image = image
    self.board = board
    self.workspace_path = workspace_path
    self.src_image_to_delta = src_image_to_delta
    self.do_stateful_update = stateful_update
    self.do_rootfs_update = rootfs_update
    self.disable_verification = disable_verification
    self.clobber_stateful = clobber_stateful
    self.reboot = reboot
    self.debug = debug
    self.ping = ping
    # Do not wipe if debug is set.
    self.wipe = wipe and not debug
    self.yes = yes
    self.force = force
    self.sdk_version = sdk_version

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
    keys = ['CURRENT_OP'] if not keys else keys
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

  def UpdateStateful(self, device, payload, clobber=False):
    """Update the stateful partition of the device.

    Args:
      device: The ChromiumOSDevice object to update.
      payload: The path to the update payload.
      clobber: Clobber stateful partition (defaults to False).
    """
    # Copy latest stateful_update to device.
    stateful_update_bin = path_util.FromChrootPath(
        self.STATEFUL_UPDATE_BIN, workspace_path=self.workspace_path)
    device.CopyToWorkDir(stateful_update_bin)
    msg = 'Updating stateful partition'
    logging.info('Copying stateful payload to device...')
    device.CopyToWorkDir(payload)
    cmd = ['sh',
           os.path.join(device.work_dir,
                        os.path.basename(self.STATEFUL_UPDATE_BIN)),
           os.path.join(device.work_dir, os.path.basename(payload))]

    if clobber:
      cmd.append('--stateful_change=clean')
      msg += ' with clobber enabled'

    logging.info('%s...', msg)
    try:
      device.RunCommand(cmd)
    except cros_build_lib.RunCommandError:
      logging.error('Faild to perform stateful partition update.')

  def _CopyDevServerPackage(self, device, tempdir):
    """Copy devserver package to work directory of device.

    Args:
      device: The ChromiumOSDevice object to copy the package to.
      tempdir: The directory to temporarily store devserver package.
    """
    logging.info('Copying devserver package to device...')
    src_dir = os.path.join(tempdir, 'src')
    osutils.RmDir(src_dir, ignore_missing=True)
    shutil.copytree(
        ds_wrapper.DEVSERVER_PKG_DIR, src_dir,
        ignore=shutil.ignore_patterns('*.pyc', 'tmp*', '.*', 'static', '*~'))
    device.CopyToWorkDir(src_dir)
    return os.path.join(device.work_dir, os.path.basename(src_dir))

  def SetupRootfsUpdate(self, device):
    """Makes sure |device| is ready for rootfs update."""
    logging.info('Checking if update engine is idle...')
    status, = self.GetUpdateStatus(device)
    if status == 'UPDATE_STATUS_UPDATED_NEED_REBOOT':
      logging.info('Device needs to reboot before updating...')
      device.Reboot()
      status, = self.GetUpdateStatus(device)

    if status != 'UPDATE_STATUS_IDLE':
      raise FlashError('Update engine is not idle. Status: %s' % status)

  def UpdateRootfs(self, device, payload, tempdir):
    """Update the rootfs partition of the device.

    Args:
      device: The ChromiumOSDevice object to update.
      payload: The path to the update payload.
      tempdir: The directory to store temporary files.
    """
    # Setup devserver and payload on the target device.
    static_dir = os.path.join(device.work_dir, 'static')
    payload_dir = os.path.join(static_dir, 'pregenerated')
    src_dir = self._CopyDevServerPackage(device, tempdir)
    device.RunCommand(['mkdir', '-p', payload_dir])
    logging.info('Copying rootfs payload to device...')
    device.CopyToDevice(payload, payload_dir)
    devserver_bin = os.path.join(src_dir, self.DEVSERVER_FILENAME)
    ds = ds_wrapper.RemoteDevServerWrapper(
        device, devserver_bin, workspace_path=self.workspace_path,
        static_dir=static_dir, log_dir=device.work_dir)

    logging.info('Updating rootfs partition')
    try:
      ds.Start()
      # Use the localhost IP address to ensure that update engine
      # client can connect to the devserver.
      omaha_url = ds.GetDevServerURL(
          ip='127.0.0.1', port=ds.port, sub_dir='update/pregenerated')
      cmd = [self.UPDATE_ENGINE_BIN, '-check_for_update',
             '-omaha_url=%s' % omaha_url]
      device.RunCommand(cmd)

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
        op, progress = self.GetUpdateStatus(device, ['CURRENT_OP', 'PROGRESS'])
        logging.info('Waiting for update...status: %s at progress %s',
                     op, progress)

        if op == 'UPDATE_STATUS_UPDATED_NEED_REBOOT':
          logging.notice('Update completed.')
          break

        if op == 'UPDATE_STATUS_IDLE':
          raise FlashError(
              'Update failed with unexpected update status: %s' % op)

        if oper is not None:
          if op == 'UPDATE_STATUS_DOWNLOADING':
            oper.ProgressBar(float(progress))
          elif end_message_not_printed and op == 'UPDATE_STATUS_FINALIZING':
            oper.Cleanup()
            logging.notice('Finalizing image.')
            end_message_not_printed = False

        time.sleep(update_check_interval)

      ds.Stop()
    except Exception:
      logging.error('Rootfs update failed.')
      logging.warning(ds.TailLog() or 'No devserver log is available.')
      raise
    finally:
      ds.Stop()
      device.CopyFromDevice(ds.log_file,
                            os.path.join(tempdir, 'target_devserver.log'),
                            error_code_ok=True)
      device.CopyFromDevice('/var/log/update_engine.log', tempdir,
                            follow_symlinks=True,
                            error_code_ok=True)

  def _CheckPayloads(self, payload_dir):
    """Checks that all update payloads exists in |payload_dir|."""
    filenames = []
    filenames += [ds_wrapper.ROOTFS_FILENAME] if self.do_rootfs_update else []
    if self.do_stateful_update:
      filenames += [ds_wrapper.STATEFUL_FILENAME]
    for fname in filenames:
      payload = os.path.join(payload_dir, fname)
      if not os.path.exists(payload):
        raise FlashError('Payload %s does not exist!' % payload)

  def Verify(self, old_root_dev, new_root_dev):
    """Verifies that the root deivce changed after reboot."""
    assert new_root_dev and old_root_dev
    if new_root_dev == old_root_dev:
      raise FlashError(
          'Failed to boot into the new version. Possibly there was a '
          'signing problem, or an automated rollback occurred because '
          'your new image failed to boot.')

  @classmethod
  def GetRootDev(cls, device):
    """Get the current root device on |device|."""
    rootdev = device.RunCommand(
        ['rootdev', '-s'], capture_output=True).output.strip()
    logging.debug('Current root device is %s', rootdev)
    return rootdev

  def Cleanup(self):
    """Cleans up the temporary directory."""
    if self.wipe:
      logging.info('Cleaning up temporary working directory...')
      osutils.RmDir(self.tempdir)
    else:
      logging.info('You can find the log files and/or payloads in %s',
                   self.tempdir)

  def _CanRunDevserver(self, device, tempdir):
    """We can run devserver on |device|.

    If the stateful partition is corrupted, Python or other packages
    (e.g. cherrypy) needed for rootfs update may be missing on |device|.

    This will also use `ldconfig` to update library paths on the target
    device if it looks like that's causing problems, which is necessary
    for base images.

    Args:
      device: A ChromiumOSDevice object.
      tempdir: A temporary directory to store files.

    Returns:
      True if we can start devserver; False otherwise.
    """
    logging.info('Checking if we can run devserver on the device.')
    src_dir = self._CopyDevServerPackage(device, tempdir)
    devserver_bin = os.path.join(src_dir, self.DEVSERVER_FILENAME)
    devserver_check_command = ['python', devserver_bin, '--help']
    try:
      device.RunCommand(devserver_check_command)
    except cros_build_lib.RunCommandError as e:
      logging.warning('Cannot start devserver: %s', e)
      if 'python: error while loading shared libraries' in str(e):
        logging.info('Attempting to correct device library paths...')
        try:
          device.RunCommand(['ldconfig', '-r', '/'])
          device.RunCommand(devserver_check_command)
          logging.info('Library path correction successful.')
          return True
        except cros_build_lib.RunCommandError as e2:
          logging.warning('Library path correction failed: %s', e2)

      return False

    return True

  def Run(self):
    """Performs remote device update."""
    old_root_dev, new_root_dev = None, None
    try:
      device_connected = False
      with remote_access.ChromiumOSDeviceHandler(
          self.ssh_hostname, port=self.ssh_port,
          base_dir=self.DEVICE_BASE_DIR, ping=self.ping) as device:
        device_connected = True

        payload_dir = self.tempdir
        if os.path.isdir(self.image):
          # If the given path is a directory, we use the provided
          # update payload(s) in the directory.
          payload_dir = self.image
          logging.info('Using provided payloads in %s', payload_dir)
        else:
          if os.path.isfile(self.image):
            # If the given path is an image, make sure devserver can
            # access it and generate payloads.
            logging.info('Using image %s', self.image)
            ds_wrapper.GetUpdatePayloadsFromLocalPath(
                self.image, payload_dir,
                src_image_to_delta=self.src_image_to_delta,
                static_dir=_DEVSERVER_STATIC_DIR,
                workspace_path=self.workspace_path)
          else:
              # We should ignore the given/inferred board value and stick to the
              # device's basic designation. We do emit a warning for good
              # measure.
              # TODO(garnold) In fact we should find the board/overlay that the
              # device inherits from and which defines the SDK "baseline" image
              # (brillo:339).
            if self.sdk_version and self.board and not self.force:
              logging.warning(
                  'Ignoring board value (%s) and deferring to device; use '
                  '--force to override',
                  self.board)
              self.board = None

            self.board = cros_build_lib.GetBoard(device_board=device.board,
                                                 override_board=self.board,
                                                 force=self.yes)
            if not self.board:
              raise FlashError('No board identified')

            if not self.force and self.board != device.board:
              # If a board was specified, it must be compatible with the device.
              raise FlashError('Device (%s) is incompatible with board %s',
                               device.board, self.board)

            logging.info('Board is %s', self.board)

            # Translate the xbuddy path to get the exact image to use.
            translated_path, resolved_path = ds_wrapper.GetImagePathWithXbuddy(
                self.image, self.board, version=self.sdk_version,
                static_dir=_DEVSERVER_STATIC_DIR, lookup_only=True)
            logging.info('Using image %s', translated_path)
            # Convert the translated path to be used in the update request.
            image_path = ds_wrapper.ConvertTranslatedPath(resolved_path,
                                                          translated_path)

            # Launch a local devserver to generate/serve update payloads.
            ds_wrapper.GetUpdatePayloads(
                image_path, payload_dir, board=self.board,
                src_image_to_delta=self.src_image_to_delta,
                workspace_path=self.workspace_path,
                static_dir=_DEVSERVER_STATIC_DIR)

        # Verify that all required payloads are in the payload directory.
        self._CheckPayloads(payload_dir)

        restore_stateful = False
        if (not self._CanRunDevserver(device, self.tempdir) and
            self.do_rootfs_update):
          msg = ('Cannot start devserver! The stateful partition may be '
                 'corrupted.')
          prompt = 'Attempt to restore the stateful partition?'
          restore_stateful = self.yes or cros_build_lib.BooleanPrompt(
              prompt=prompt, default=False, prolog=msg)
          if not restore_stateful:
            raise FlashError('Cannot continue to perform rootfs update!')

        if restore_stateful:
          logging.warning('Restoring the stateful partition...')
          payload = os.path.join(payload_dir, ds_wrapper.STATEFUL_FILENAME)
          self.UpdateStateful(device, payload, clobber=self.clobber_stateful)
          device.Reboot()
          if self._CanRunDevserver(device, self.tempdir):
            logging.info('Stateful partition restored.')
          else:
            raise FlashError('Unable to restore stateful partition.')

        # Perform device updates.
        if self.do_rootfs_update:
          self.SetupRootfsUpdate(device)
          # Record the current root device. This must be done after
          # SetupRootfsUpdate because SetupRootfsUpdate may reboot the
          # device if there is a pending update, which changes the
          # root device.
          old_root_dev = self.GetRootDev(device)
          payload = os.path.join(payload_dir, ds_wrapper.ROOTFS_FILENAME)
          self.UpdateRootfs(device, payload, self.tempdir)
          logging.info('Rootfs update completed.')

        if self.do_stateful_update and not restore_stateful:
          payload = os.path.join(payload_dir, ds_wrapper.STATEFUL_FILENAME)
          self.UpdateStateful(device, payload, clobber=self.clobber_stateful)
          logging.info('Stateful update completed.')

        if self.reboot:
          logging.notice('Rebooting device...')
          device.Reboot()
          if self.clobber_stateful:
            # --clobber-stateful wipes the stateful partition and the
            # working directory on the device no longer exists. To
            # remedy this, we recreate the working directory here.
            device.BaseRunCommand(['mkdir', '-p', device.work_dir])

        if self.do_rootfs_update and self.reboot:
          logging.notice('Verifying that the device has been updated...')
          new_root_dev = self.GetRootDev(device)
          self.Verify(old_root_dev, new_root_dev)

        if self.disable_verification:
          logging.info('Disabling rootfs verification on the device...')
          device.DisableRootfsVerification()

    except Exception:
      logging.error('Device update failed.')
      if device_connected and device.lsb_release:
        lsb_entries = sorted(device.lsb_release.items())
        logging.info('Following are the LSB version details of the device:\n%s',
                     '\n'.join('%s=%s' % (k, v) for k, v in lsb_entries))
      raise
    else:
      logging.notice('Update performed successfully.')
    finally:
      self.Cleanup()


# TODO(dpursell): replace |brick| argument with blueprints when they're ready.
def Flash(device, image, project_sdk_image=False, sdk_version=None, board=None,
          brick_name=None, blueprint_name=None, install=False,
          src_image_to_delta=None, rootfs_update=True, stateful_update=True,
          clobber_stateful=False, reboot=True, wipe=True, ping=True,
          disable_rootfs_verification=False, clear_cache=False, yes=False,
          force=False, debug=False):
  """Flashes a device, USB drive, or file with an image.

  This provides functionality common to `cros flash` and `brillo flash`
  so that they can parse the commandline separately but still use the
  same underlying functionality.

  Args:
    device: commandline.Device object; None to use the default device.
    image: Path (string) to the update image. Can be a local or xbuddy path;
        non-existant local paths are converted to xbuddy.
    project_sdk_image: Use a clean project SDK image. Overrides |image| if True.
    sdk_version: Which version of SDK image to flash; autodetected if None.
    board: Board to use; None to automatically detect.
    brick_name: Brick locator to use. Overrides |board| if not None.
    blueprint_name: Blueprint locator to use. Overrides |board| and
        |brick_name|.
    install: Install to USB using base disk layout; USB |device| scheme only.
    src_image_to_delta: Local path to an image to be used as the base to
        generate delta payloads; SSH |device| scheme only.
    rootfs_update: Update rootfs partition; SSH |device| scheme only.
    stateful_update: Update stateful partition; SSH |device| scheme only.
    clobber_stateful: Clobber stateful partition; SSH |device| scheme only.
    reboot: Reboot device after update; SSH |device| scheme only.
    wipe: Wipe temporary working directory; SSH |device| scheme only.
    ping: Ping the device before attempting update; SSH |device| scheme only.
    disable_rootfs_verification: Remove rootfs verification after update; SSH
        |device| scheme only.
    clear_cache: Clear the devserver static directory.
    yes: Assume "yes" for any prompt.
    force: Ignore sanity checks and prompts. Overrides |yes| if True.
    debug: Print additional debugging messages.

  Raises:
    FlashError: An unrecoverable error occured.
    ValueError: Invalid parameter combination.
  """
  if force:
    yes = True

  if clear_cache:
    logging.info('Clearing the cache...')
    ds_wrapper.DevServerWrapper.WipeStaticDirectory(_DEVSERVER_STATIC_DIR)

  try:
    osutils.SafeMakedirsNonRoot(_DEVSERVER_STATIC_DIR)
  except OSError:
    logging.error('Failed to create %s', _DEVSERVER_STATIC_DIR)

  if install:
    if not device or device.scheme != commandline.DEVICE_SCHEME_USB:
      raise ValueError(
          '--install can only be used when writing to a USB device')
    if not cros_build_lib.IsInsideChroot():
      raise ValueError('--install can only be used inside the chroot')

  # If installing an SDK image, find the version and override image path.
  if project_sdk_image:
    image = 'project_sdk'
    if sdk_version is None:
      sdk_version = project_sdk.FindVersion()
      if not sdk_version:
        raise FlashError('Could not find SDK version')

  # We don't have enough information on the device to make a good guess on
  # whether this device is compatible with the blueprint.
  # TODO(bsimonnet): Add proper compatibility checks. (brbug.com/969)
  if blueprint_name:
    board = None
    if image == 'latest':
      blueprint = blueprint_lib.Blueprint(blueprint_name)
      image_dir = os.path.join(
          workspace_lib.WorkspacePath(), workspace_lib.WORKSPACE_IMAGES_DIR,
          blueprint.FriendlyName(), 'latest')
      image = _ChooseImageFromDirectory(image_dir)
    elif not os.path.exists(image):
      raise ValueError('Cannot find blueprint image "%s". Only "latest" and '
                       'full image path are supported.' % image)
  elif brick_name:
    board = brick_lib.Brick(brick_name).FriendlyName()

  workspace_path = workspace_lib.WorkspacePath()

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
        workspace_path=workspace_path,
        src_image_to_delta=src_image_to_delta,
        rootfs_update=rootfs_update,
        stateful_update=stateful_update,
        clobber_stateful=clobber_stateful,
        reboot=reboot,
        wipe=wipe,
        debug=debug,
        yes=yes,
        force=force,
        ping=ping,
        disable_verification=disable_rootfs_verification,
        sdk_version=sdk_version)
    updater.Run()
  elif device.scheme == commandline.DEVICE_SCHEME_USB:
    path = osutils.ExpandPath(device.path) if device.path else ''
    logging.info('Preparing to image the removable device %s', path)
    imager = USBImager(path,
                       board,
                       image,
                       workspace_path=workspace_path,
                       sdk_version=sdk_version,
                       debug=debug,
                       install=install,
                       yes=yes)
    imager.Run()
  elif device.scheme == commandline.DEVICE_SCHEME_FILE:
    logging.info('Preparing to copy image to %s', device.path)
    imager = FileImager(device.path,
                        board,
                        image,
                        sdk_version=sdk_version,
                        debug=debug,
                        yes=yes)
    imager.Run()
