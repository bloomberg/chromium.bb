# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for manipulating ChromeOS images."""

from __future__ import print_function

import glob
import os
import re

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import retry_util
from chromite.lib import signing


# security_check: pass_config mapping.
_SECURITY_CHECKS = {
    'no_nonrelease_files': True,
    'sane_lsb-release': True,
    'secure_kernelparams': True,
    'not_ASAN': False,
}


class Error(Exception):
  """Base image_lib error class."""


class LoopbackError(Error):
  """An exception raised when something went wrong setting up a loopback"""


class LoopbackPartitions(object):
  """Loopback mount a file and provide access to its partitions.

  This class can be used as a context manager with the "with" statement, or
  individual instances of it can be created which will clean themselves up
  when garbage collected or when explicitly closed, ala the tempfile module.

  In either case, the same arguments should be passed to init.
  """

  def __init__(self, path, destination=None, part_ids=None, mount_opts=('ro',)):
    """Initialize.

    Args:
      path: Path to the backing file.
      destination: Base path to mount partitions.
      part_ids: Mount these partitions at context manager entry.
      mount_opts: Use these mount_opts for mounting |part_ids|.
    """
    self.path = path
    self.destination = destination
    self.dev = None
    self.part_ids = part_ids
    self.mount_opts = mount_opts
    self.parts = {}
    self._destination_created = False
    self._gpt_table = cros_build_lib.GetImageDiskPartitionInfo(path)
    # Set of _gpt_table elements currently mounted.
    self._mounted = set()
    # Set of dirs that need to be removed in close().
    self._to_be_rmdir = set()
    # Set of symlinks created.
    self._symlinks = set()

    try:
      cmd = ['losetup', '--show', '-f', self.path]
      ret = cros_build_lib.SudoRunCommand(
          cmd, print_cmd=False, capture_output=True)
      self.dev = ret.output.strip()
      cmd = ['partx', '-d', self.dev]
      cros_build_lib.SudoRunCommand(cmd, quiet=True, error_code_ok=True)
      cmd = ['partx', '-a', self.dev]
      ret = cros_build_lib.SudoRunCommand(
          cmd, print_cmd=False, error_code_ok=True)
      if ret.returncode:
        logging.warning('Adding partitions failed; dumping log & retrying')
        cros_build_lib.RunCommand(['sync'])
        cros_build_lib.RunCommand(['dmesg'])
        cmd = ['partx', '-u', self.dev]
        cros_build_lib.SudoRunCommand(cmd, print_cmd=False)

      self.parts = {}
      part_devs = glob.glob(self.dev + 'p*')
      if not part_devs:
        logging.warning("Didn't find partition devices nodes for %s.",
                        self.path)
        return

      for part in part_devs:
        number = int(re.search(r'p(\d+)$', part).group(1))
        self.parts[number] = part

    except:
      self.close()
      raise

  def GetPartitionDevName(self, part_id):
    """Return the loopback device for a partition.

    Args:
      part_id: partition name (str) or number (int)

    Returns:
      String with name of loopback device (e.g. '/dev/loop3p2').  If there are
      multiple partitions that match part_id, then the first one from the
      partition table is returned.
    """
    for part in self._gpt_table:
      if part_id == part.name or part_id == part.number:
        return '%sp%d' % (self.dev, part.number)
    raise KeyError(repr(part_id))

  def _GetMountPointAndSymlink(self, part):
    """Return tuple of mount point and symlink for a given PartitionInfo.

    Args:
      part: A PartitionInfo object.

    Returns:
      (mount_point, symlink) tuple.
    """
    dest_number = os.path.join(self.destination, 'dir-%d' % part.number)
    dest_label = os.path.join(self.destination, 'dir-%s' % part.name)
    return (dest_number, dest_label)

  def Mount(self, part_ids, mount_opts=('ro',)):
    """Mount the given part_ids in subdirectories of the given destination.

    Args:
      part_ids: list of partition names (str) or numbers (int)
      mount_opts: list of mount options to be applied for these partitions.

    Returns:
      List of mountpoint paths.
    """
    ret = []
    for part_id in part_ids:
      for part in self._gpt_table:
        if part_id == part.name or part_id == part.number:
          ret.append(self._Mount(part, mount_opts))
          break
      else:
        raise KeyError(repr(part_id))
    return ret

  def Unmount(self, part_ids):
    """Mount the given part_ids in subdirectories of the given destination.

    Args:
      part_ids: list of partition names (str) or numbers (int)
      mount_opts: list of mount options to be applied for these partitions.
    """
    for part_id in part_ids:
      for part in self._gpt_table:
        if part_id == part.name or part_id == part.number:
          self._Unmount(part)
          break
      else:
        raise KeyError(repr(part_id))

  def _IsExt2(self, part_id, offset=0):
    """Is the given partition an ext2 file system?"""
    dev = self.GetPartitionDevName(part_id)
    magic_ofs = offset + 0x438
    ret = cros_build_lib.SudoRunCommand(
        ['dd', 'if=%s' % dev, 'skip=%d' % magic_ofs,
         'conv=notrunc', 'count=2', 'bs=1'],
        capture_output=True, error_code_ok=True)
    if ret.returncode:
      return False
    return ret.output == b'\x53\xef'

  def EnableRwMount(self, part_id, offset=0):
    """Enable RW mounts of the specified partition."""
    dev = self.GetPartitionDevName(part_id)
    if not self._IsExt2(part_id, offset):
      logging.error('EnableRwMount called on non-ext2 fs: %s %s',
                    part_id, offset)
      return
    ro_compat_ofs = offset + 0x464 + 3
    logging.info('Enabling RW mount writing 0x00 to %d', ro_compat_ofs)
    cros_build_lib.SudoRunCommand(
        ['dd', 'of=%s' % dev, 'seek=%d' % ro_compat_ofs,
         'conv=notrunc', 'count=1', 'bs=1'],
        input=b'\0', redirect_stderr=True)

  def DisableRwMount(self, part_id, offset=0):
    """Disable RW mounts of the specified partition."""
    dev = self.GetPartitionDevName(part_id)
    if not self._IsExt2(part_id, offset):
      logging.error('DisableRwMount called on non-ext2 fs: %s %s',
                    part_id, offset)
      return
    ro_compat_ofs = offset + 0x464 + 3
    logging.info('Disabling RW mount writing 0xff to %d', ro_compat_ofs)
    cros_build_lib.SudoRunCommand(
        ['dd', 'of=%s' % dev, 'seek=%d' % ro_compat_ofs,
         'conv=notrunc', 'count=1', 'bs=1'],
        input=b'\xff', redirect_stderr=True)

  def _Mount(self, part, mount_opts):
    if not self.destination:
      self.destination = osutils.TempDir().tempdir
      self._destination_created = True

    dest_number, dest_label = self._GetMountPointAndSymlink(part)
    if part in self._mounted and 'remount' not in mount_opts:
      return dest_number

    osutils.MountDir(self.GetPartitionDevName(part.number), dest_number,
                     makedirs=True, skip_mtab=False, sudo=True,
                     mount_opts=mount_opts)
    self._mounted.add(part)

    osutils.SafeSymlink(os.path.basename(dest_number), dest_label)
    self._symlinks.add(dest_label)

    return dest_number

  def _Unmount(self, part):
    """Unmount a partition that was mounted by _Mount."""
    dest_number, _ = self._GetMountPointAndSymlink(part)
    # Due to crosbug/358933, the RmDir call might fail. So we skip the cleanup.
    osutils.UmountDir(dest_number, cleanup=False)
    self._mounted.remove(part)
    self._to_be_rmdir.add(dest_number)

  def close(self):
    if self.dev:
      for part in list(self._mounted):
        self._Unmount(part)

      # We still need to remove some directories, since _Unmount did not.
      for link in self._symlinks:
        osutils.SafeUnlink(link)
      self._symlinks = set()
      for path in self._to_be_rmdir:
        retry_util.RetryException(cros_build_lib.RunCommandError, 60,
                                  osutils.RmDir, path, sudo=True, sleep=1)
      self._to_be_rmdir = set()
      cmd = ['partx', '-d', self.dev]
      cros_build_lib.SudoRunCommand(cmd, quiet=True, error_code_ok=True)
      cros_build_lib.SudoRunCommand(['losetup', '--detach', self.dev])
      self.dev = None
      self.parts = {}
      self._gpt_table = None
      if self._destination_created:
        self.destination = None
        self._destination_created = False

  def __enter__(self):
    if self.part_ids:
      self.Mount(self.part_ids, self.mount_opts)
    return self

  def __exit__(self, exc_type, exc, tb):
    self.close()

  def __del__(self):
    self.close()


def WriteLsbRelease(sysroot, fields):
  """Writes out the /etc/lsb-release file into the given sysroot.

  Args:
    sysroot: The sysroot to write the lsb-release file to.
    fields: A dictionary of all the fields and values to write.
  """
  content = '\n'.join('%s=%s' % (k, v) for k, v in fields.items()) + '\n'

  path = os.path.join(sysroot, constants.LSB_RELEASE_PATH.lstrip('/'))

  if os.path.exists(path):
    # The file has already been pre-populated with some fields.  Since
    # osutils.WriteFile(..) doesn't support appending with sudo, read in the
    # content and prepend it to the new content to write.
    # TODO(stevefung): Remove this appending, once all writing to the
    #   /etc/lsb-release file has been removed from ebuilds and consolidated
    #  to the buid tools.
    content = osutils.ReadFile(path) + content

  osutils.WriteFile(path, content, mode='w', makedirs=True, sudo=True)
  cros_build_lib.SudoRunCommand([
      'setfattr', '-n', 'security.selinux', '-v',
      'u:object_r:cros_conf_file:s0', path])


def GetLatestImageLink(board, force_chroot=False):
  """Get the path for the `latest` image symlink for the given board.

  Args:
    board (str): The name of the board.
    force_chroot (bool): Get the path as if we are inside the chroot, whether
      or not we actually are.

  Returns:
    str - The `latest` image symlink path.
  """
  base = constants.CHROOT_SOURCE_ROOT if force_chroot else constants.SOURCE_ROOT
  return os.path.join(base, 'src/build/images', board, 'latest')


class ImageDoesNotExistError(Error):
  """When the provided or implied image path does not exist."""


class SecurityConfigDirectoryError(Error):
  """The SecurityTestConfig directory does not exist."""


class SecurityTestArgumentError(Error):
  """Invalid SecurityTest argument error."""


class VbootCheckoutError(Error):
  """Error checking out the stable vboot source."""


def SecurityTest(board=None, image=None, baselines=None, vboot_hash=None):
  """Image security tests.

  Args:
    board: str - The board whose image should be tested. Used when |image| is
      not provided or is a basename. Defaults to the default board.
    image: str - The path to an image that should be tested, or the basename of
      the desired image in the |board|'s build directory.
    baselines: str - The path to a directory containing the baseline configs.
    vboot_hash: str - The commit hash to checkout for the vboot_reference clone.

  Returns:
    bool - True on success, False on failure.

  Raises:
    SecurityTestArgumentError when one or more arguments are not valid.
    VbootCheckoutError when the vboot_reference repository cannot be cloned or
        the |vboot_hash| cannot be checked out.
  """
  if not cros_build_lib.IsInsideChroot():
    cmd = ['security_test_image']
    if board:
      cmd += ['--board', board]
    if image:
      cmd += ['--image', image]
    if baselines:
      cmd += ['--baselines', baselines]
    if vboot_hash:
      cmd += ['--vboot-hash', vboot_hash]
    result = cros_build_lib.RunCommand(cmd, enter_chroot=True,
                                       error_code_ok=True)
    return not result.returncode
  else:
    try:
      image = BuildImagePath(board, image)
    except ImageDoesNotExistError as e:
      raise SecurityTestArgumentError(e.message)
    logging.info('Using %s', image)

    if not baselines:
      baselines = signing.SECURITY_BASELINES_DIR
      if not os.path.exists(baselines):
        if not os.path.exists(signing.CROS_SIGNING_BASE_DIR):
          logging.warning('Skipping security tests with public manifest.')
          return True
        else:
          raise SecurityTestArgumentError(
              'Could not locate security baselines from %s with private '
              'manifest.' % baselines)
    logging.info('Loading baselines from %s', baselines)

    if not vboot_hash:
      vboot_hash = signing.GetDefaultVbootStableHash()
      if not vboot_hash:
        raise SecurityTestArgumentError(
            'Could not detect vboot_stable_hash in %s.'
            % signing.CROS_SIGNING_CONFIG)
    logging.info('Using vboot_reference.git rev %s', vboot_hash)

    with osutils.TempDir() as tempdir:
      config = SecurityTestConfig(image, baselines, vboot_hash, tempdir)
      failures = sum(config.RunCheck(check, with_config)
                     for check, with_config in _SECURITY_CHECKS.items())

    if failures:
      logging.error('%s tests failed', failures)
    else:
      logging.info('All tests passed.')

    return not failures


def BuildImagePath(board, image):
  """Build a fully qualified path to the image.

  Args:
    board: str - The name of the board whose image is being tested when an
      image path is not specified.
    image: str - The path to an image (in which case |image| is simply returned)
      or the basename of the image file to use. When |image| is a basename, the
      |board| build directory is always used to find it.
  """
  # Prefer an image path if provided.
  if image and os.sep in image:
    if os.path.exists(image):
      return image
    else:
      raise ImageDoesNotExistError(
          'The provided image does not exist: %s' % image)

  # We have no image or a basename only, so we need the board to build out the
  # full path to an image file.
  if not board:
    board = cros_build_lib.GetDefaultBoard()

  if not board:
    if image:
      raise ImageDoesNotExistError(
          '|image| must be a full path or used with |board|.')
    else:
      raise ImageDoesNotExistError(
          'Either |image| or |board| must be provided.')

  # Build out the full path using the board's build path.
  image_file = image or 'recovery_image.bin'
  image = os.path.join(GetLatestImageLink(board), image_file)

  if not os.path.exists(image):
    raise ImageDoesNotExistError('Image does not exist: %s' % image)

  return image


class SecurityTestConfig(object):
  """Hold configurations and do related setup."""

  _VBOOT_SRC = os.path.join(constants.SOURCE_ROOT,
                            'src/platform/vboot_reference/.git')
  _VBOOT_CHECKS_REL_DIR = 'scripts/image_signing'

  def __init__(self, image, baselines, vboot_hash, directory):
    """SecurityTest run configuration.

    Args:
      image: str - Path to an image.
      baselines: str - Path to the security baselines.
      vboot_hash: str - Commit hash for the vboot_reference.
      directory: str - The directory to use for the vboot_reference checkout.
        Usually a temporary directory.
    """
    self.image = image
    self.baselines = baselines
    self.vboot_hash = vboot_hash
    self.directory = directory
    self._repo_dir = os.path.join(self.directory, 'vboot_source')
    self._checks_dir = os.path.join(self._repo_dir, self._VBOOT_CHECKS_REL_DIR)
    self._checked_out = False

  def RunCheck(self, check, pass_config):
    """Run the given check.

    Args:
      check: str - A config.vboot_dir/ensure_|check|.sh check name.
      pass_config: bool - Whether the check has a corresponding
          `ensure_|check|.config` file to pass.

    Returns:
      bool - True on success, False on failure.

    Raises:
      SecurityConfigDirectoryError if the directory does not exist.
      VbootCheckoutError if the vboot reference repo could not be cloned or the
        vboot_hash could not be checked out.
    """
    self._VbootCheckout()

    cmd = [os.path.join(self._checks_dir, 'ensure_%s.sh' % check), self.image]
    if pass_config:
      cmd.append(os.path.join(self.baselines, 'ensure_%s.config' % check))

    try:
      self._RunCommand(cmd)
    except cros_build_lib.RunCommandError as e:
      logging.error('%s test failed: %s', check, e.message)
      return False
    else:
      return True

  def _VbootCheckout(self):
    """Clone the vboot reference repo and checkout the vboot stable hash."""
    if not os.path.exists(self.directory):
      raise SecurityConfigDirectoryError('The directory does not exist.')

    if not self._checked_out:
      try:
        git.Clone(self._repo_dir, self._VBOOT_SRC, reference=self._VBOOT_SRC)
      except cros_build_lib.RunCommandError as e:
        raise VbootCheckoutError('Failed cloning repo from %s: %s' %
                                 (self._VBOOT_SRC, e.message))
      try:
        cros_build_lib.RunCommand(['git', 'checkout', '-q', self.vboot_hash],
                                  cwd=self._repo_dir)
      except cros_build_lib.RunCommandError as e:
        raise VbootCheckoutError('Failed checking out %s from %s: %s' %
                                 (self.vboot_hash, self._VBOOT_SRC, e.message))
      self._checked_out = True

  def _RunCommand(self, cmd, *args, **kwargs):
    """Run a command ensuring the signing bin directory is included in PATH."""
    extra_env = {'PATH': '%s:%s' % (signing.CROS_SIGNING_BIN_DIR,
                                    os.environ['PATH'])}
    kwargs['extra_env'] = extra_env.update(kwargs.get('extra_env', {}))
    return cros_build_lib.RunCommand(cmd, *args, **kwargs)
