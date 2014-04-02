# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros deploy: Deploy the packages onto the target device."""

import os
import logging
import urlparse

from chromite import cros
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import remote_access


@cros.CommandDecorator('deploy')
class DeployCommand(cros.CrosCommand):
  """Deploy the requested packages to the target device.

  This command assumes the requested packages are already built in the
  chroot. This command needs to run inside the chroot for inspecting
  the installed packages.

  Note: If the rootfs on your device is read-only, this command
  remounts it as read-write. If the rootfs verification is enabled on
  your device, this command disables it.
  """

  EPILOG = """
To deploy packages:
  cros deploy device power_manager cherrypy
  cros deploy device /path/to/package

To uninstall packages:
  cros deploy --unmerge cherrypy

For more information of cros build usage:
  cros build -h
"""

  DEVICE_BASE_DIR = '/usr/local/tmp/cros-deploy'
  # This is defined in src/platform/dev/builder.py
  STRIPPED_PACKAGES_DIR = 'stripped-packages'

  # Override base class property to enable stats upload.
  upload_stats = True

  def __init__(self, options):
    """Initializes DeployCommand."""
    cros.CrosCommand.__init__(self, options)
    self.emerge = True
    self.strip = True
    self.clean_binpkg = True
    self.ssh_hostname = None
    self.ssh_port = None
    self.ssh_username = None
    self.ssh_private_key = None
    # The installation root of packages.
    self.root = None

  @classmethod
  def AddParser(cls, parser):
    """Add a parser."""
    super(cls, DeployCommand).AddParser(parser)
    parser.add_argument(
        'device', help='IP[:port] address of the target device.')
    parser.add_argument(
        'packages', help='Packages to install. You can specify '
        '[category/]package[:slot] or the path to the binary package.',
        nargs='+')
    parser.add_argument(
        '--board', default=None, help='The board to use. By default it is '
        'automatically detected. You can override the detected board with '
        'this option.')
    parser.add_argument(
        '--no-strip', dest='strip', action='store_false', default=True,
        help='Do not run strip_package to filter out preset paths in the '
        'package. Stripping removes debug symbol files and reduces the size '
        'of the package significantly. Defaults to always strip.')
    parser.add_argument(
        '--unmerge',  dest='emerge', action='store_false', default=True,
        help='Unmerge requested packages.')
    parser.add_argument(
        '--root', default='/',
        help="Package installation root, e.g. '/' or '/usr/local'"
        "(default: '/').")
    parser.add_argument(
        '--no-clean-binpkg', dest='clean_binpkg', action='store_false',
        default=True, help='Do not clean outdated binary packages. '
        ' Defaults to always clean.')
    parser.add_argument(
        '--emerge-args', default=None,
        help='Extra arguments to pass to emerge.')
    parser.add_argument(
        '--private-key', type='path', default=None,
        help='SSH identify file (private key).')

  def GetLatestPackage(self, board, pkg):
    """Returns the path to the latest |pkg| for |board|."""
    sysroot = cros_build_lib.GetSysroot(board=board)
    matches = portage_utilities.FindPackageNameMatches(
        pkg, board=board)
    if not matches:
      raise ValueError('Package %s is not installed!' % pkg)

    idx = 0
    if len(matches) > 1:
      # Ask user to pick among multiple matches.
      idx = cros_build_lib.GetChoice(
          'Multiple matches found for %s: ' % pkg,
          [os.path.join(x.category, x.pv) for x in matches])

    cpv = matches[idx]
    packages_dir = None
    if self.strip:
      try:
        cros_build_lib.RunCommand(
            ['strip_package', '--board', board,
             os.path.join(cpv.category, '%s' % (cpv.pv))])
        packages_dir = self.STRIPPED_PACKAGES_DIR
      except cros_build_lib.RunCommandError:
        logging.error('Cannot strip package %s', pkg)
        raise

    return portage_utilities.GetBinaryPackagePath(
        cpv.category, cpv.package, cpv.version, sysroot=sysroot,
        packages_dir=packages_dir)

  def _Emerge(self, device, board, pkg, root, extra_args=None):
    """Copies |pkg| to |device| and emerges it.

    Args:
      device: A ChromiumOSDevice object.
      board: The board to use for retrieving |pkg|.
      pkg: A package name.
      root: The installation root of |pkg|.
      extra_args: Extra arguments to pass to emerge.
    """
    if os.path.isfile(pkg):
      latest_pkg = pkg
    else:
      latest_pkg = self.GetLatestPackage(board, pkg)

    if not latest_pkg:
      cros_build_lib.Die('Missing package %s.' % pkg)

    pkgroot = os.path.join(device.work_dir, 'packages')
    pkg_name = os.path.basename(latest_pkg)
    pkg_dirname = os.path.basename(os.path.dirname(latest_pkg))
    pkg_dir = os.path.join(pkgroot, pkg_dirname)
    device.RunCommand(['mkdir', '-p', pkg_dir], remote_sudo=True)

    logging.info('Copying %s to device...', latest_pkg)
    device.CopyToDevice(latest_pkg, pkg_dir, remote_sudo=True)

    portage_tmpdir = os.path.join(device.work_dir, 'portage-tmp')
    device.RunCommand(['mkdir', '-p', portage_tmpdir], remote_sudo=True)
    logging.info('Use portage temp dir %s', portage_tmpdir)

    logging.info('Installing %s...', latest_pkg)
    pkg_path = os.path.join(pkg_dir, pkg_name)

    # We set PORTAGE_CONFIGROOT to '/usr/local' because by default all
    # chromeos-base packages will be skipped due to the configuration
    # in /etc/protage/make.profile/package.provided. However, there is
    # a known bug that /usr/local/etc/portage is not setup properly
    # (crbug.com/312041). This does not affect `cros deploy` because
    # we do not use the preset PKGDIR.
    extra_env = {
        'FEATURES': '-sandbox',
        'PKGDIR': pkgroot,
        'PORTAGE_CONFIGROOT': '/usr/local',
        'PORTAGE_TMPDIR': portage_tmpdir,
        'PORTDIR': device.work_dir,
        'CONFIG_PROTECT': '-*',
    }
    cmd = ['emerge', '--usepkg', pkg_path]
    cmd.append('--root=%s' % root)
    if extra_args:
      cmd.append(extra_args)

    try:
      # Always showing the emerge output for clarity.
      device.RunCommand(cmd, extra_env=extra_env, remote_sudo=True,
                        capture_output=False, debug_level=logging.INFO)
    except Exception:
      logging.error('Failed to emerge package %s', pkg)
      raise
    else:
      logging.info('%s has been installed.', pkg)
    finally:
      # Free up the space for other packages.
      device.RunCommand(['rm', '-rf', portage_tmpdir, pkg_dir],
                        error_code_ok=True, remote_sudo=True)

  def _Unmerge(self, device, pkg, root):
    """Unmerges |pkg| on |device|.

    Args:
      device: A RemoteDevice object.
      pkg: A package name.
      root: The installation root of |pkg|.
    """
    logging.info('Unmerging %s...', pkg)
    cmd = ['qmerge', '--yes']
    # Check if qmerge is available on the device. If not, use emerge.
    if device.RunCommand(
        ['qmerge', '--version'], error_code_ok=True).returncode != 0:
      cmd = ['emerge']

    cmd.extend(['--unmerge', pkg, '--root=%s' % root])
    try:
      # Always showing the qmerge/emerge output for clarity.
      device.RunCommand(cmd, capture_output=False, remote_sudo=True,
                        debug_level=logging.INFO)
    except Exception:
      logging.error('Failed to unmerge package %s', pkg)
      raise
    else:
      logging.info('%s has been uninstalled.', pkg)

  def _IsPathWritable(self, device, path):
    """Returns True if |path| on |device| is writable."""
    tmp_file = os.path.join(path, 'tmp.cros_flash')
    result = device.RunCommand(['touch', tmp_file], remote_sudo=True,
                               error_code_ok=True, capture_output=True)

    if result.returncode != 0:
      return False

    device.RunCommand(['rm', tmp_file], error_code_ok=True, remote_sudo=True)

    return True

  def _ReadOptions(self):
    """Processes options and set variables."""
    self.emerge = self.options.emerge
    self.strip = self.options.strip
    self.clean_binpkg = self.options.clean_binpkg
    self.root = self.options.root
    device = self.options.device
    # pylint: disable=E1101
    if urlparse.urlparse(device).scheme == '':
      # For backward compatibility, prepend ssh:// ourselves.
      device = 'ssh://%s' % device

    parsed = urlparse.urlparse(device)

    if parsed.scheme == 'ssh':
      self.ssh_hostname = parsed.hostname
      self.ssh_username = parsed.username
      self.ssh_port = parsed.port
      self.ssh_private_key = self.options.private_key
    else:
      cros_build_lib.Die('Does not support device %s' % self.options.device)

  def Run(self):
    """Run cros deploy."""
    cros_build_lib.AssertInsideChroot()
    self._ReadOptions()

    try:
      with remote_access.ChromiumOSDeviceHandler(
          self.ssh_hostname, port=self.ssh_port, username=self.ssh_username,
          private_key=self.ssh_private_key,
          base_dir=self.DEVICE_BASE_DIR) as device:
        board = cros_build_lib.GetBoard(device_board=device.board,
                                        override_board=self.options.board)
        logging.info('Board is %s', board)

        if self.clean_binpkg:
          logging.info('Cleaning outdated binary packages for %s', board)
          portage_utilities.CleanOutdatedBinaryPackages(board)

        if not self._IsPathWritable(device, self.root):
          # Only remounts rootfs if the given root is not writable.
          if not device.MountRootfsReadWrite():
            cros_build_lib.Die('Cannot remount rootfs as read-write. Exiting.')

        for pkg in self.options.packages:
          if self.emerge:
            self._Emerge(device, board, pkg, self.root,
                         extra_args=self.options.emerge_args)
          else:
            self._Unmerge(device, pkg, self.root)

    except Exception:
      logging.error('Cros Deploy terminated before completing!')
      raise
    else:
      logging.info('All packages are processed.')
