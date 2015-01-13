# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing SDK stages."""

from __future__ import print_function

import glob
import json
import logging
import os

from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import perf_uploader
from chromite.lib import portage_util
from chromite.lib import retry_util


def SdkPerfPath(buildroot):
  """Return the path to the perf file for sdk stages."""
  return os.path.join(buildroot, constants.DEFAULT_CHROOT_DIR, 'tmp',
                      'cros-sdk.perf')


class SDKBuildToolchainsStage(generic_stages.BuilderStage):
  """Stage that builds all the cross-compilers we care about"""

  def PerformStage(self):
    chroot_location = os.path.join(self._build_root,
                                   constants.DEFAULT_CHROOT_DIR)

    # Build the toolchains first.  Since we're building & installing the
    # compilers, need to run as root.
    self.CrosSetupToolchains(['--nousepkg'], sudo=True)

    # Create toolchain packages.
    self.CreateRedistributableToolchains(chroot_location)

  def CrosSetupToolchains(self, cmd_args, **kwargs):
    """Wrapper around cros_setup_toolchains to simplify things."""
    commands.RunBuildScript(self._build_root,
                            ['cros_setup_toolchains'] + list(cmd_args),
                            chromite_cmd=True, enter_chroot=True, **kwargs)

  def CreateRedistributableToolchains(self, chroot_location):
    """Create the toolchain packages"""
    osutils.RmDir(os.path.join(chroot_location,
                               constants.SDK_TOOLCHAINS_OUTPUT),
                  ignore_missing=True)

    # We need to run this as root because the tool creates hard links to root
    # owned files and our bots enable security features which disallow that.
    # Specifically, these features cause problems:
    #  /proc/sys/kernel/yama/protected_nonaccess_hardlinks
    #  /proc/sys/fs/protected_hardlinks
    self.CrosSetupToolchains([
        '--create-packages',
        '--output-dir', os.path.join('/', constants.SDK_TOOLCHAINS_OUTPUT),
    ], sudo=True)


class SDKPackageStage(generic_stages.BuilderStage):
  """Stage that performs preparing and packaging SDK files"""

  # Version of the Manifest file being generated. Should be incremented for
  # Major format changes.
  MANIFEST_VERSION = '1'
  _EXCLUDED_PATHS = ('usr/lib/debug', constants.AUTOTEST_BUILD_PATH,
                     'packages', 'tmp')

  def __init__(self, builder_run, version=None, **kwargs):
    self.sdk_version = version
    super(SDKPackageStage, self).__init__(builder_run, **kwargs)

  def PerformStage(self):
    tarball_name = 'built-sdk.tar.xz'
    tarball_location = os.path.join(self._build_root, tarball_name)
    chroot_location = os.path.join(self._build_root,
                                   constants.DEFAULT_CHROOT_DIR)
    board_location = os.path.join(chroot_location, 'build/amd64-host')
    manifest_location = os.path.join(self._build_root,
                                     '%s.Manifest' % tarball_name)

    # Create a tarball of the latest SDK.
    self.CreateSDKTarball(chroot_location, board_location, tarball_location)

    # Create a package manifest for the tarball.
    self.CreateManifestFromSDK(board_location, manifest_location)

    # Make sure the regular user has the permission to read.
    cmd = ['chmod', 'a+r', tarball_location]
    cros_build_lib.SudoRunCommand(cmd, cwd=board_location)

    self.SendPerfValues(tarball_location)

  def CreateSDKTarball(self, _chroot, sdk_path, dest_tarball):
    """Creates an SDK tarball from a given source chroot.

    Args:
      chroot: A chroot used for finding compression tool.
      sdk_path: Path to the root of newly generated SDK image.
      dest_tarball: Path of the tarball that should be created.
    """
    # TODO(zbehan): We cannot use xz from the chroot unless it's
    # statically linked.
    extra_args = ['--exclude=%s/*' % path for path in self._EXCLUDED_PATHS]
    # Options for maximum compression.
    extra_env = {'XZ_OPT': '-e9'}
    cros_build_lib.CreateTarball(
        dest_tarball, sdk_path, sudo=True, extra_args=extra_args,
        debug_level=logging.INFO, extra_env=extra_env)

  def CreateManifestFromSDK(self, sdk_path, dest_manifest):
    """Creates a manifest from a given source chroot.

    Args:
      sdk_path: Path to the root of the SDK to describe.
      dest_manifest: Path to the manifest that should be generated.
    """
    cros_build_lib.Info('Generating manifest for new sdk')
    package_data = {}
    for key, version in portage_util.ListInstalledPackages(sdk_path):
      package_data.setdefault(key, []).append((version, {}))
    self._WriteManifest(package_data, dest_manifest)

  def _WriteManifest(self, data, manifest):
    """Encode manifest into a json file."""
    json_input = dict(version=self.MANIFEST_VERSION, packages=data)
    osutils.WriteFile(manifest, json.dumps(json_input))

  @staticmethod
  def _SendPerfValues(buildroot, sdk_tarball, buildbot_uri_log, version,
                      platform_name):
    """Generate & upload perf data for the build"""
    perf_path = SdkPerfPath(buildroot)
    test_name = 'sdk'
    units = 'bytes'

    # Make sure the file doesn't contain previous data.
    osutils.SafeUnlink(perf_path)

    common_kwargs = {
        'higher_is_better': False,
        'graph': 'cros-sdk-size',
        'stdio_uri': buildbot_uri_log,
    }

    sdk_size = os.path.getsize(sdk_tarball)
    perf_uploader.OutputPerfValue(perf_path, 'base', sdk_size, units,
                                  **common_kwargs)

    for tarball in glob.glob(os.path.join(
        buildroot, constants.DEFAULT_CHROOT_DIR,
        constants.SDK_TOOLCHAINS_OUTPUT, '*.tar.*')):
      name = os.path.basename(tarball).rsplit('.', 2)[0]
      size = os.path.getsize(tarball)
      perf_uploader.OutputPerfValue(perf_path, name, size, units,
                                    **common_kwargs)
      perf_uploader.OutputPerfValue(perf_path, 'base_plus_%s' % name,
                                    sdk_size + size, units, **common_kwargs)

    # Due to limitations in the perf dashboard, we have to create an integer
    # based on the current timestamp.  This field only accepts integers, and
    # the perf dashboard accepts this or CrOS+Chrome official versions.
    revision = int(version.replace('.', ''))
    perf_values = perf_uploader.LoadPerfValues(perf_path)
    retry_util.RetryException(perf_uploader.PerfUploadingError, 3,
                              perf_uploader.UploadPerfValues,
                              perf_values, platform_name, test_name,
                              revision=revision)

  def SendPerfValues(self, sdk_tarball):
    """Generate & upload perf data for the build"""
    self._SendPerfValues(self._build_root, sdk_tarball,
                         self.ConstructDashboardURL(), self.sdk_version,
                         self._run.bot_id)


class SDKTestStage(generic_stages.BuilderStage):
  """Stage that performs testing an SDK created in a previous stage"""

  option_name = 'tests'

  def PerformStage(self):
    new_chroot_dir = 'new-sdk-chroot'
    tarball_location = os.path.join(self._build_root, 'built-sdk.tar.xz')
    new_chroot_args = ['--chroot', new_chroot_dir]
    if self._run.options.chrome_root:
      new_chroot_args += ['--chrome_root', self._run.options.chrome_root]

    # Build a new SDK using the provided tarball.
    chroot_args = new_chroot_args + ['--download', '--replace', '--nousepkg',
                                     '--url', 'file://' + tarball_location]
    cros_build_lib.RunCommand(
        [], cwd=self._build_root, enter_chroot=True, chroot_args=chroot_args,
        extra_env=self._portage_extra_env)

    # Inject the toolchain binpkgs from the previous sdk build.  On end user
    # systems, they'd be fetched from the binpkg mirror, but we don't have one
    # set up for this local build.
    pkgdir = os.path.join('var', 'lib', 'portage', 'pkgs')
    old_pkgdir = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR,
                              pkgdir)
    new_pkgdir = os.path.join(self._build_root, new_chroot_dir, pkgdir)
    osutils.SafeMakedirs(new_pkgdir, sudo=True)
    cros_build_lib.SudoRunCommand(
        ['cp', '-r'] + glob.glob(os.path.join(old_pkgdir, 'cross-*')) +
        [new_pkgdir])

    # Now install those toolchains in the new chroot.  We skip the chroot
    # upgrade below which means we need to install the toolchain manually.
    cmd = ['cros_setup_toolchains', '--targets=boards',
           '--include-boards=%s' % ','.join(self._boards)]
    commands.RunBuildScript(self._build_root, cmd, chromite_cmd=True,
                            enter_chroot=True, sudo=True,
                            chroot_args=new_chroot_args,
                            extra_env=self._portage_extra_env)

    # Build all the boards with the new sdk.
    for board in self._boards:
      cros_build_lib.PrintBuildbotStepText(board)
      cmd = ['./setup_board', '--board', board, '--skip_chroot_upgrade']
      cros_build_lib.RunCommand(
          cmd, cwd=self._build_root, enter_chroot=True,
          chroot_args=new_chroot_args, extra_env=self._portage_extra_env)
      cmd = ['./build_packages', '--board', board, '--nousepkg',
             '--skip_chroot_upgrade']
      cros_build_lib.RunCommand(cmd, cwd=self._build_root, enter_chroot=True,
                                chroot_args=new_chroot_args,
                                extra_env=self._portage_extra_env)
