# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install/copy the image to the device."""

import cStringIO
import hashlib
import logging
import os
import shutil
import tempfile
import time
import urlparse

from chromite import cros
from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import dev_server_wrapper as ds_wrapper
from chromite.lib import osutils
from chromite.lib import remote_access


# The folder in devserver's static_dir that cros flash uses to store
# symlinks to local images given by user. Note that this means if
# there is a board or board-version (e.g. peppy-release) named
# 'others', there will be a conflict.
DEVSERVER_LOCAL_IMAGE_SYMLINK_DIR = 'others'

IMAGE_NAME_TO_TYPE = {
    'chromiumos_test_image.bin': 'test',
    'chromiumos_image.bin': 'dev',
    'chromiumos_base_image.bin': 'base'
}


# pylint: disable=E1101
def GenerateXbuddyRequest(path, static_dir):
  """Generate an xbuddy request used to retreive payloads.

  This function generates a xbuddy request based on |path|. If the
  request is sent to the devserver, the server will respond with a
  URL pointing to the folder of update payloads.

  If |path| is an xbuddy path (xbuddy://subpath), strip the '://"
  and returns xbuddy/subpath. If |path| is a local path to an image,
  creates a symlink in the static_dir, so that xbuddy can access the
  image; returns the corresponding xbuddy path. If |path| can't be found,
  convert it to an xbuddy path.

  Args:
    path: Either a local path to an image or an xbuddy path (with or without
      xbuddy://).
    static_dir: static directory of the local devserver.

  Returns:
    A xbuddy request.
  """
  # Path used to store the string that xbuddy understands when finding an image.
  xbuddy_path = None
  parsed = urlparse.urlparse(path)

  # For xbuddy paths, we should do a sanity check / confirmation when the xbuddy
  # board doesn't match the board on the device. Unfortunately this isn't
  # currently possible since we don't want to duplicate xbuddy code.
  # TODO(sosa): crbug.com/340722 and use it to compare boards.
  if parsed.scheme == 'xbuddy':
    xbuddy_path = '%s%s' % (parsed.netloc, parsed.path)
  elif not os.path.exists(path):
    logging.debug('Cannot find path "%s". Assuming it is an xbuddy path.', path)
    xbuddy_path = path
  else:
    # We have a list of known image names that are recognized by
    # devserver. User cannot arbitrarily rename their images.
    if os.path.basename(path) not in IMAGE_NAME_TO_TYPE:
      raise ValueError('Unknown image name %s' % os.path.basename(path))

    chroot_path = cros_build_lib.ToChrootPath(path)
    # Create and link static_dir/DEVSERVER_LOCAL_IMAGE_SYMLINK_DIR/hashed_path
    # to the image folder, so that xbuddy/devserver can understand the path.
    # Alternatively, we can to pass '--image' at devserver startup, but this
    # flag is to be deprecated soon.
    relative_dir = os.path.join(DEVSERVER_LOCAL_IMAGE_SYMLINK_DIR,
                                hashlib.md5(chroot_path).hexdigest())
    abs_dir = os.path.join(static_dir, relative_dir)
    # Make the parent directory if it doesn't exist.
    osutils.SafeMakedirsNonRoot(os.path.dirname(abs_dir))
    # Create the symlink if it doesn't exist.
    if not os.path.lexists(abs_dir):
      logging.info('Creating a symlink %s -> %s', abs_dir,
                   os.path.dirname(chroot_path))
      os.symlink(os.path.dirname(chroot_path), abs_dir)

    xbuddy_path = os.path.join(relative_dir,
                               IMAGE_NAME_TO_TYPE[os.path.basename(path)])

  return 'xbuddy/%s?for_update=true&return_dir=true' % xbuddy_path


class DeviceUpdateError(Exception):
  """Thrown when there is an error during device update."""


class RemoteDeviceUpdater(object):
  """Performs update on a remote device."""
  ROOTFS_FILENAME = 'update.gz'
  STATEFUL_FILENAME = 'stateful.tgz'
  DEVSERVER_STATIC_DIR = cros_build_lib.FromChrootPath(
      os.path.join(constants.CHROOT_SOURCE_ROOT, 'devserver', 'static'))
  DEVSERVER_PKG_DIR = os.path.join(constants.SOURCE_ROOT, 'src/platform/dev')
  STATEFUL_UPDATE_BIN = '/usr/bin/stateful_update'
  UPDATE_ENGINE_BIN = 'update_engine_client'
  UPDATE_CHECK_INTERVAL = 10
  # Root working directory on the device. This directory is in the
  # stateful partition and thus has enough space to store the payloads.
  DEVICE_BASE_DIR = '/mnt/stateful_partition/cros-flash'

  def __init__(self, ssh_hostname, ssh_port, image_path, stateful_update=True,
               rootfs_update=True, clobber_stateful=False, reboot=True,
               board=None, src_image=None, wipe=True, debug=False, yes=False):
    """Initializes RemoteDeviceUpdater"""
    if not stateful_update and not rootfs_update:
      cros_build_lib.Die('No update operation to perform. Use -h to see usage.')

    self.tempdir = tempfile.mkdtemp(prefix='cros-flash')
    self.ssh_hostname = ssh_hostname
    self.ssh_port = ssh_port
    self.image_path = image_path
    self.board = board
    self.src_image = src_image
    self.do_stateful_update = stateful_update
    self.do_rootfs_update = rootfs_update
    self.clobber_stateful = clobber_stateful
    self.reboot = reboot
    self.debug = debug
    # Do not wipe if debug is set.
    self.wipe = wipe and not debug
    self.yes = yes

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
    result = device.RunCommand([cls.UPDATE_ENGINE_BIN, '--status'])
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
    cmd = ['sh', os.path.join(device.work_dir,
                              os.path.basename(self.STATEFUL_UPDATE_BIN)), '-']
    if clobber:
      cmd.append('--stateful_change=clean')
      msg += ' with clobber enabled'

    logging.info('%s...', msg)

    device.PipeOverSSH(payload, cmd)
    logging.info('Payload decompressed to stateful partition.')

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
        self.DEVSERVER_PKG_DIR, src_dir,
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
    devserver_bin = os.path.join(src_dir, 'devserver.py')
    ds = ds_wrapper.RemoteDevServerWrapper(
        device, devserver_bin, static_dir=static_dir, log_dir=device.work_dir)

    logging.info('Updating rootfs partition')
    try:
      ds.Start()

      omaha_url = ds.GetDevServerURL(ip=remote_access.LOCALHOST_IP,
                                     port=ds.port,
                                     sub_dir='update/pregenerated')
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
      ds_log = ds.TailLog()
      if ds_log:
        logging.error(ds_log)
      raise
    finally:
      ds.Stop()
      device.CopyFromDevice(ds.log_filename,
                            os.path.join(tempdir, 'target_devserver.log'),
                            error_code_ok=True)
      device.CopyFromDevice('/var/log/update_engine.log', tempdir,
                            follow_symlinks=True,
                            error_code_ok=True)

  def GetUpdatePayloads(self, path, payload_dir, board=None, src_image=None,
                        timeout=60 * 15):
    """Launch devserver to get the update payloads.

    Args:
      path: The image or xbuddy path.
      payload_dir: The directory to store the payloads.
      board: The default board to use when |path| is None.
      src_image: Image used as the base to generate the delta payloads.
      timeout: Timeout for launching devserver (seconds).
    """
    static_dir = self.DEVSERVER_STATIC_DIR
    # SafeMakedirsNonroot has a side effect that 'chown' an existing
    # root-owned directory with a non-root user. This makes sure
    # we can write to static_dir later.
    osutils.SafeMakedirsNonRoot(static_dir)

    ds = ds_wrapper.DevServerWrapper(
        static_dir=static_dir, src_image=src_image, board=board)
    req = GenerateXbuddyRequest(path, static_dir)
    logging.info('Starting local devserver to generate/serve payloads...')
    try:
      ds.Start()
      url = ds.OpenURL(ds.GetDevServerURL(sub_dir=req), timeout=timeout)
      ds.DownloadFile(os.path.join(url, self.ROOTFS_FILENAME), payload_dir)
      ds.DownloadFile(os.path.join(url, self.STATEFUL_FILENAME), payload_dir)
    except ds_wrapper.DevServerException:
      ds_log = ds.TailLog()
      if ds_log:
        logging.error(ds_log)
      raise
    else:
      # If we're running in debug, also print out the log even if we didn't get
      # an exception.
      if self.debug:
        ds_log = ds.TailLog()
        if ds_log:
          logging.error(ds_log)
    finally:
      ds.Stop()
      if os.path.exists(ds.log_filename):
        shutil.copyfile(ds.log_filename,
                        os.path.join(payload_dir, 'local_devserver.log'))
      else:
        logging.warning('Could not find %s', ds.log_filename)

  def _CheckPayloads(self, payload_dir):
    """Checks that all update payloads exists in |payload_dir|."""
    filenames = []
    filenames += [self.ROOTFS_FILENAME] if self.do_rootfs_update else []
    filenames += [self.STATEFUL_FILENAME] if self.do_stateful_update else []
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

  def Run(self):
    """Performs remote device update."""
    old_root_dev, new_root_dev = None, None
    try:
      with remote_access.ChromiumOSDeviceHandler(
          self.ssh_hostname, port=self.ssh_port,
          base_dir=self.DEVICE_BASE_DIR) as device:

        board = cros_build_lib.GetBoard(device_board=device.board,
                                        override_board=self.board,
                                        force=self.yes)
        logging.info('Board is %s', board)

        # If the given path is a directory, we use the pre-generated
        # update payload(s) in the directory.
        if os.path.isdir(self.image_path):
          payload_dir = self.image_path
          logging.info('Using payloads in %s', payload_dir)
        else:
          # Launch a local devserver to generate/serve update payloads.
          payload_dir = self.tempdir
          self.GetUpdatePayloads(self.image_path, payload_dir,
                                 board=board,
                                 src_image=self.src_image)

        self._CheckPayloads(payload_dir)
        # Perform device updates.
        if self.do_rootfs_update:
          self.SetupRootfsUpdate(device)
          # Record the current root device. This must be done after
          # SetupRootfsUpdate because SetupRootfsUpdate may reboot the
          # device if there is a pending update, which changes the
          # root device.
          old_root_dev = self.GetRootDev(device)
          payload = os.path.join(payload_dir, self.ROOTFS_FILENAME)
          self.UpdateRootfs(device, payload, self.tempdir)
          logging.info('Rootfs update completed.')

        if self.do_stateful_update:
          payload = os.path.join(payload_dir, self.STATEFUL_FILENAME)
          self.UpdateStateful(device, payload, clobber=self.clobber_stateful)
          logging.info('Stateful update completed.')

        if self.reboot:
          logging.info('Rebooting device..')
          device.Reboot()

        if self.do_rootfs_update and self.reboot:
          new_root_dev = self.GetRootDev(device)
          self.Verify(old_root_dev, new_root_dev)

    except Exception:
      logging.error('Device update failed.')
      raise
    else:
      logging.info('Update performed successfully.')
    finally:
      self.Cleanup()


@cros.CommandDecorator('flash')
class FlashCommand(cros.CrosCommand):
  """Update the device with an image.

  This command updates the device with the image. It assumes that
  device is able to accept ssh connections.

  For rootfs partition update, this command may launch a devserver to
  generate payloads. As a side effect, it may create symlinks in
  static_dir/others used by the devserver.
  """

  EPILOG = """
To update the device with the latest locally built image:
  cros flash device latest
  cros flash device

To update the device with an xbuddy path:
  cros flash device xbuddy://{local, remote}/<board>/<version>

  Common xbuddy version aliases are 'latest' (alias for 'latest-stable')
  latest-{dev, beta, stable, canary}, and latest-official.

To update the device with a local image path:
  cros flash device /path/to/image.bin

There are certain constraints on the local image path:
  1. The image path has to be in your source tree.
  2. The image name should be one of the following:
  3. You should create a separate directory for each image as devserve
    writes temp files to it and reuses payloads it finds.

  For more information and known problems/fixes, please see:
  http://dev.chromium.org/chromium-os/build/cros-flash
"""
  # Override base class property to enable stats upload.
  upload_stats = True

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(FlashCommand, cls).AddParser(parser)
    parser.add_argument(
        'device', help='The hostname/IP[:port] address of the device.')
    parser.add_argument(
        'image', nargs='?', default='latest', help="Image to install; "
        "can be a local path or an xbuddy path "
        "(xbuddy://{local|remote}/board/version). Note any strings that do not "
        "map to a real file path will be converted to an xbuddy path i.e. "
        "latest, will map to xbuddy://latest if latest isn't a local file.")

    update = parser.add_argument_group('Advanced device update options')
    update.add_argument(
        '--board', default=None, help='The board to use. By default it is '
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
        help='Do not wipe the temporary working directory.')
    update.add_argument(
        '--no-stateful-update', action='store_false', dest='stateful_update',
        help='Do not update the stateful partition on the device.'
        'Default is always update.')
    update.add_argument(
        '--no-rootfs-update', action='store_false', dest='rootfs_update',
        help='Do not update the rootfs partition on the device. '
        'Default is always update.')
    update.add_argument(
        '--src-image', type='path',
        help='Local path to an image to be used as the base to generate '
        'payloads.')
    update.add_argument(
        '--clobber-stateful', action='store_true', default=False,
        help='Clobber stateful partition when performing update.')

  def __init__(self, options):
    """Initializes cros flash."""
    cros.CrosCommand.__init__(self, options)
    self.ssh_mode = False
    self.ssh_hostname = None
    self.ssh_port = None

  def _ParseDevice(self, device):
    """Parse |device| and set corresponding variables ."""
    # pylint: disable=E1101
    if urlparse.urlparse(device).scheme == '':
      # For backward compatibility, prepend ssh:// ourselves.
      device = 'ssh://%s' % device

    parsed = urlparse.urlparse(device)
    if parsed.scheme == 'ssh':
      self.ssh_mode = True
      self.ssh_hostname = parsed.hostname
      self.ssh_port = parsed.port
    else:
      cros_build_lib.Die('Does not support device %s' % device)

  # pylint: disable=E1101
  def Run(self):
    """Perfrom the cros flash command."""
    self.options.Freeze()
    self._ParseDevice(self.options.device)

    try:
      if self.ssh_mode:
        logging.info('Preparing to update the remote device %s',
                     self.options.device)
        updater = RemoteDeviceUpdater(
            self.ssh_hostname,
            self.ssh_port,
            self.options.image,
            board=self.options.board,
            src_image=self.options.src_image,
            rootfs_update=self.options.rootfs_update,
            stateful_update=self.options.stateful_update,
            clobber_stateful=self.options.clobber_stateful,
            reboot=self.options.reboot,
            wipe=self.options.wipe,
            debug=self.options.debug,
            yes=self.options.yes)

        # Perform device update.
        updater.Run()

    except (Exception, KeyboardInterrupt):
      logging.error('Cros Flash failed before completing.')
      raise
