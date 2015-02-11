# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install/copy the image to the device."""

from __future__ import print_function

import cStringIO
import logging
import os
import shutil
import tempfile
import time
import urlparse

from chromite import cros
from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import dev_server_wrapper as ds_wrapper
from chromite.lib import osutils
from chromite.lib import project
from chromite.lib import remote_access


DEVSERVER_STATIC_DIR = cros_build_lib.FromChrootPath(
    os.path.join(constants.CHROOT_SOURCE_ROOT, 'devserver', 'static'))


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
    if not self.board:
      raise Exception('Couldn\'t determine what board to use')
    cmd = [
        '%s/usr/sbin/chromeos-install' % cros_build_lib.GetSysroot(self.board),
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
    # Use pv to display progress bar if possible.
    cmd_base = 'pv -pretb'
    try:
      cros_build_lib.RunCommand(['pv', '--version'], print_cmd=False,
                                capture_output=True)
    except cros_build_lib.RunCommandError:
      cmd_base = 'cat'

    cmd = '%s %s | dd of=%s bs=4M iflag=fullblock oflag=sync' % (
        cmd_base, image, device)
    cros_build_lib.SudoRunCommand(cmd, shell=True)
    cros_build_lib.SudoRunCommand(['sync'], debug_level=self.debug_level)

  def IsFilePathGPTDiskImage(self, file_path):
    """Determines if the file is a valid GPT disk."""
    if os.path.isfile(file_path):
      with cros_build_lib.Open(file_path) as image_file:
        image_file.seek(0x1fe)
        if image_file.read(10) == '\x55\xaaEFI PART':
          return True
    return False

  def ChooseImageFromDirectory(self, dir_path):
    """Lists all image files in |dir_path| and ask user to select one."""
    images = [x for x in os.listdir(dir_path) if
              self.IsFilePathGPTDiskImage(os.path.join(dir_path, x))]
    idx = 0
    if len(images) == 0:
      raise ValueError('No image found in %s.' % dir_path)
    elif len(images) > 1:
      idx = cros_build_lib.GetChoice(
          'Multiple images found in %s. Please select one to continue:' % (
              (dir_path,)),
          images)

    return os.path.join(dir_path, images[idx])

  def _GetImagePath(self):
    """Returns the image path to use."""
    image_path = translated_path = None
    if os.path.isfile(self.image):
      if not self.yes and not self.IsFilePathGPTDiskImage(self.image):
        # TODO(wnwen): Open the tarball and if there is just one file in it,
        #     use that instead. Existing code in upload_symbols.py.
        if cros_build_lib.BooleanPrompt(
            prolog='The given image file is not a valid disk image. Perhaps '
                   'you forgot to untar it.',
            prompt='Terminate the current flash process?'):
          cros_build_lib.Die('Cros Flash terminated by user.')
      image_path = self.image
    elif os.path.isdir(self.image):
      # Ask user which image (*.bin) in the folder to use.
      image_path = self.ChooseImageFromDirectory(self.image)
    else:
      # Translate the xbuddy path to get the exact image to use.
      translated_path = ds_wrapper.GetImagePathWithXbuddy(
          self.image, self.board, static_dir=DEVSERVER_STATIC_DIR,
          device='usb://%s' % self.device)
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
        cros_build_lib.Die('Device path %s does not exist.' % self.device)

      # Then check if it is removable.
      if self.device not in [self.DeviceNameToPath(x) for x in devices]:
        msg = '%s is not a removable device.' % self.device
        if not (self.yes or cros_build_lib.BooleanPrompt(
            default=False, prolog=msg)):
          cros_build_lib.Die('You can specify usb:// to choose from a list of '
                             'removable devices.')
    target = None
    if self.device:
      # Get device name from path (e.g. sdc in /dev/sdc).
      target = self.device.rsplit(os.path.sep, 1)[-1]
    elif devices:
      # Ask user to choose from the list.
      target = self.ChooseRemovableDevice(devices)
    else:
      cros_build_lib.Die('No removable devices detected.')

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
      cros_build_lib.Die('Path %s does not exist.' % self.device)

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


class DeviceUpdateError(Exception):
  """Thrown when there is an error during device update."""


class RemoteDeviceUpdater(object):
  """Performs update on a remote device."""
  DEVSERVER_FILENAME = 'devserver.py'
  STATEFUL_UPDATE_BIN = '/usr/bin/stateful_update'
  UPDATE_ENGINE_BIN = 'update_engine_client'
  UPDATE_CHECK_INTERVAL = 10
  # Root working directory on the device. This directory is in the
  # stateful partition and thus has enough space to store the payloads.
  DEVICE_BASE_DIR = '/mnt/stateful_partition/cros-flash'

  def __init__(self, ssh_hostname, ssh_port, image, stateful_update=True,
               rootfs_update=True, clobber_stateful=False, reboot=True,
               board=None, src_image_to_delta=None, wipe=True, debug=False,
               yes=False, ping=True, disable_verification=False,
               ignore_device_board=False):
    """Initializes RemoteDeviceUpdater"""
    if not stateful_update and not rootfs_update:
      cros_build_lib.Die('No update operation to perform. Use -h to see usage.')

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
    self.ping = ping
    # Do not wipe if debug is set.
    self.wipe = wipe and not debug
    self.yes = yes
    self.ignore_device_board = ignore_device_board

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
    stateful_update_bin = cros_build_lib.FromChrootPath(
        self.STATEFUL_UPDATE_BIN)
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
      raise DeviceUpdateError('Update engine is not idle. Status: %s' % status)

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
        device, devserver_bin, static_dir=static_dir, log_dir=device.work_dir)

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

      # Loop until update is complete.
      while True:
        op, progress = self.GetUpdateStatus(device, ['CURRENT_OP', 'PROGRESS'])
        logging.info('Waiting for update...status: %s at progress %s',
                     op, progress)

        if op == 'UPDATE_STATUS_UPDATED_NEED_REBOOT':
          break

        if op == 'UPDATE_STATUS_IDLE':
          raise DeviceUpdateError(
              'Update failed with unexpected update status: %s' % op)

        time.sleep(self.UPDATE_CHECK_INTERVAL)

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
        cros_build_lib.Die('Payload %s does not exist!' % payload)

  def Verify(self, old_root_dev, new_root_dev):
    """Verifies that the root deivce changed after reboot."""
    assert new_root_dev and old_root_dev
    if new_root_dev == old_root_dev:
      raise DeviceUpdateError(
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
    (e.g. cherrypy) that Cros Flash needs for rootfs update may be
    missing on |device|.

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

        self.board = cros_build_lib.GetBoard(device_board=device.board,
                                             override_board=self.board,
                                             force=self.yes)
        logging.info('Board is %s', self.board)

        # Make sure that a project is found and compatible with the device.
        proj = project.FindProjectByName(self.board)
        if not proj:
          cros_build_lib.Die('Could not find project for board')
        if not (self.ignore_device_board or proj.Inherits(device.board)):
          cros_build_lib.Die('Device (%s) is incompatible with board',
                             device.board)

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
                static_dir=DEVSERVER_STATIC_DIR)
          else:
            # For xbuddy paths, we should do a sanity check / confirmation
            # when the xbuddy board doesn't match the board on the
            # device. Unfortunately this isn't currently possible since we
            # don't want to duplicate xbuddy code.  TODO(sosa):
            # crbug.com/340722 and use it to compare boards.

            device_addr = 'ssh://%s' % self.ssh_hostname
            if self.ssh_port:
              device_addr = '%s:%d' % (device_addr, self.ssh_port)
            # Translate the xbuddy path to get the exact image to use.
            translated_path = ds_wrapper.GetImagePathWithXbuddy(
                self.image, self.board, static_dir=DEVSERVER_STATIC_DIR,
                device=device_addr)
            logging.info('Using image %s', translated_path)
            # Convert the translated path to be used in the update request.
            image_path = ds_wrapper.ConvertTranslatedPath(self.image,
                                                          translated_path)

            # Launch a local devserver to generate/serve update payloads.
            ds_wrapper.GetUpdatePayloads(
                image_path, payload_dir, board=self.board,
                src_image_to_delta=self.src_image_to_delta,
                static_dir=DEVSERVER_STATIC_DIR)

        # Verify that all required payloads are in the payload directory.
        self._CheckPayloads(payload_dir)

        restore_stateful = False
        if (not self._CanRunDevserver(device, self.tempdir) and
            self.do_rootfs_update):
          msg = ('Cannot start devserver! The stateful partition may be '
                 'corrupted. Cros Flash can try to restore the stateful '
                 'partition first.')
          restore_stateful = self.yes or cros_build_lib.BooleanPrompt(
              default=False, prolog=msg)
          if not restore_stateful:
            cros_build_lib.Die('Cannot continue to perform rootfs update!')

        if restore_stateful:
          logging.warning('Restoring the stateful partition...')
          payload = os.path.join(payload_dir, ds_wrapper.STATEFUL_FILENAME)
          self.UpdateStateful(device, payload, clobber=self.clobber_stateful)
          device.Reboot()
          if self._CanRunDevserver(device, self.tempdir):
            logging.info('Stateful partition restored.')
          else:
            cros_build_lib.Die('Unable to restore stateful partition. Exiting.')

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
          logging.info('Rebooting device..')
          device.Reboot()
          if self.clobber_stateful:
            # --clobber-stateful wipes the stateful partition and the
            # working directory on the device no longer exists. To
            # remedy this, we recreate the working directory here.
            device.BaseRunCommand(['mkdir', '-p', device.work_dir])

        if self.do_rootfs_update and self.reboot:
          logging.info('Verifying that the device has been updated...')
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
      logging.info('Update performed successfully.')
    finally:
      self.Cleanup()


@cros.CommandDecorator('flash')
class FlashCommand(cros.CrosCommand):
  """Update the device with an image.

  This command updates the device with the image
  (ssh://<hostname>:{port}, copies an image to a removable device
  (usb://<device_path), or copies a xbuddy path to a local
  file path with (file://file_path).

  For device update, it assumes that device is able to accept ssh
  connections.

  For rootfs partition update, this command may launch a devserver to
  generate payloads. As a side effect, it may create symlinks in
  static_dir/others used by the devserver.
  """

  EPILOG = """
To update/image the device with the latest locally built image:
  cros flash device latest
  cros flash device

To update/image the device with an xbuddy path:
  cros flash device xbuddy://{local, remote}/<board>/<version>

  Common xbuddy version aliases are 'latest' (alias for 'latest-stable')
  latest-{dev, beta, stable, canary}, and latest-official.

To update/image the device with a local image path:
  cros flash device /path/to/image.bin

Examples:
  cros flash 192.168.1.7 xbuddy://remote/x86-mario/latest-canary
  cros flash 192.168.1.7 xbuddy://remote/x86-mario-paladin/R32-4830.0.0-rc1
  cros flash usb:// xbuddy://remote/trybot-x86-mario-paladin/R32-5189.0.0-b100
  cros flash usb:///dev/sde xbuddy://peppy/latest
  cros flash file:///~/images xbuddy://peppy/latest

  For more information and known problems/fixes, please see:
  http://dev.chromium.org/chromium-os/build/cros-flash
"""

  SSH_MODE = 'ssh'
  USB_MODE = 'usb'
  FILE_MODE = 'file'

  # Override base class property to enable stats upload.
  upload_stats = True

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(FlashCommand, cls).AddParser(parser)
    parser.add_argument(
        'device', help='ssh://device_hostname[:port] or usb://{device_path}. '
        'If no device_path is given (i.e. usb://), user will be prompted to '
        'choose from a list of removable devices.')
    parser.add_argument(
        'image', nargs='?', default='latest', help="A local path or an xbuddy "
        "path: xbuddy://{local|remote}/board/version/{image_type} image_type "
        "can be: 'test', 'dev', 'base', or 'recovery'. Note any strings that "
        "do not map to a real file path will be converted to an xbuddy path "
        "i.e., latest, will map to xbuddy://latest.")
    parser.add_argument(
        '--clear-cache', default=False, action='store_true',
        help='Clear the devserver static directory. This deletes all the '
        'downloaded images and payloads, and also payloads generated by '
        'the devserver. Default is not to clear.')

    update = parser.add_argument_group('Advanced device update options')
    update.add_argument(
        '--board', '--project', help='The board to use. By default it is '
        'automatically detected. You can override the detected board with '
        'this option')
    update.add_argument(
        '--yes', default=False, action='store_true',
        help='Force yes to any prompt. Use with caution.')
    update.add_argument(
        '--no-reboot', action='store_false', dest='reboot', default=True,
        help='Do not reboot after update. Default is always reboot.')
    update.add_argument(
        '--no-wipe', action='store_false', dest='wipe', default=True,
        help='Do not wipe the temporary working directory. Default '
        'is always wipe.')
    update.add_argument(
        '--no-stateful-update', action='store_false', dest='stateful_update',
        help='Do not update the stateful partition on the device. '
        'Default is always update.')
    update.add_argument(
        '--no-rootfs-update', action='store_false', dest='rootfs_update',
        help='Do not update the rootfs partition on the device. '
        'Default is always update.')
    update.add_argument(
        '--src-image-to-delta', type='path',
        help='Local path to an image to be used as the base to generate '
        'delta payloads.')
    update.add_argument(
        '--clobber-stateful', action='store_true', default=False,
        help='Clobber stateful partition when performing update.')
    update.add_argument(
        '--no-ping', dest='ping', action='store_false', default=True,
        help='Do not ping the device before attempting to connect to it.')
    update.add_argument(
        '--disable-rootfs-verification', default=False, action='store_true',
        help='Disable rootfs verification after update is completed.')
    parser.add_argument(
        '--ignore-device-board', action='store_true',
        help='Do not require that device be compatible with current '
        'project/board.')

    usb = parser.add_argument_group('USB specific options')
    usb.add_argument(
        '--install', default=False, action='store_true',
        help='Install to the USB device using the base disk layout.')

  def __init__(self, options):
    """Initializes cros flash."""
    cros.CrosCommand.__init__(self, options)
    self.run_mode = None
    self.ssh_hostname = None
    self.ssh_port = None
    self.usb_dev = None
    self.copy_path = None
    self.any = False

  def _ParseDevice(self, device):
    """Parse |device| and set corresponding variables ."""
    # pylint: disable=E1101
    if urlparse.urlparse(device).scheme == '':
      # For backward compatibility, prepend ssh:// ourselves.
      device = 'ssh://%s' % device

    parsed = urlparse.urlparse(device)
    if parsed.scheme == self.SSH_MODE:
      self.run_mode = self.SSH_MODE
      self.ssh_hostname = parsed.hostname
      self.ssh_port = parsed.port
    elif parsed.scheme == self.USB_MODE:
      self.run_mode = self.USB_MODE
      self.usb_dev = device[len('%s://' % self.USB_MODE):]
    elif parsed.scheme == self.FILE_MODE:
      self.run_mode = self.FILE_MODE
      self.copy_path = device[len('%s://' % self.FILE_MODE):]
    else:
      cros_build_lib.Die('Does not support device %s' % device)

  # pylint: disable=E1101
  def Run(self):
    """Perfrom the cros flash command."""
    self.options.Freeze()

    if self.options.clear_cache:
      logging.info('Clearing the cache...')
      ds_wrapper.DevServerWrapper.WipeStaticDirectory(
          DEVSERVER_STATIC_DIR)

    try:
      osutils.SafeMakedirsNonRoot(DEVSERVER_STATIC_DIR)
    except OSError:
      logging.error('Failed to create %s', DEVSERVER_STATIC_DIR)

    self._ParseDevice(self.options.device)

    if self.options.install:
      if self.run_mode != self.USB_MODE:
        logging.error('--install can only be used when writing to a USB device')
        return 1
      if not cros_build_lib.IsInsideChroot():
        logging.error('--install can only be used inside the chroot')
        return 1

    try:
      board = self.options.board or self.curr_project_name
      if self.run_mode == self.SSH_MODE:
        logging.info('Preparing to update the remote device %s',
                     self.options.device)
        updater = RemoteDeviceUpdater(
            self.ssh_hostname,
            self.ssh_port,
            self.options.image,
            board=board,
            src_image_to_delta=self.options.src_image_to_delta,
            rootfs_update=self.options.rootfs_update,
            stateful_update=self.options.stateful_update,
            clobber_stateful=self.options.clobber_stateful,
            reboot=self.options.reboot,
            wipe=self.options.wipe,
            debug=self.options.debug,
            yes=self.options.yes,
            ping=self.options.ping,
            disable_verification=self.options.disable_rootfs_verification,
            ignore_device_board=self.options.ignore_device_board)

        # Perform device update.
        updater.Run()
      elif self.run_mode == self.USB_MODE:
        path = osutils.ExpandPath(self.usb_dev) if self.usb_dev else ''
        logging.info('Preparing to image the removable device %s', path)
        imager = USBImager(path,
                           board,
                           self.options.image,
                           debug=self.options.debug,
                           install=self.options.install,
                           yes=self.options.yes)
        imager.Run()
      elif self.run_mode == self.FILE_MODE:
        path = osutils.ExpandPath(self.copy_path) if self.copy_path else ''
        logging.info('Preparing to copy image to %s', path)
        imager = FileImager(path,
                            board,
                            self.options.image,
                            debug=self.options.debug,
                            yes=self.options.yes)
        imager.Run()

    except (Exception, KeyboardInterrupt) as e:
      logging.error(e)
      logging.error('Cros Flash failed before completing.')
      if self.options.debug:
        raise
    else:
      logging.info('Cros Flash completed successfully.')
