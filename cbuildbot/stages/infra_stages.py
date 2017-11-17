# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing infra build stages."""

from __future__ import print_function

import os
import shutil

from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cipd
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import path_util


_GO_BINDIR = '/usr/bin'
_GO_PACKAGES = {
    'lucifer': (
        'lucifer_run_job',
        'lucifer_watcher',
    ),
}
_CRED_FILE = ('/creds/service_accounts/'
              'service-account-chromeos-cipd-uploader.json')


class EmergeInfraGoBinariesStage(generic_stages.BuilderStage):
  """Emerge Chromium OS Go binary packages."""

  def PerformStage(self):
    """Build infra Go packages."""
    self._EmergePackages()

  def _EmergePackages(self):
    cmd = ['emerge', '--deep']
    cmd.extend(_GO_PACKAGES)
    commands.RunBuildScript(self._build_root, cmd,
                            sudo=True, enter_chroot=True)


class PackageInfraGoBinariesStage(generic_stages.BuilderStage,
                                  generic_stages.ArchivingStageMixin):
  """Make CIPD packages for Go binaries."""

  def PerformStage(self):
    """Build infra Go packages."""
    self._PreparePackagesDir()
    for package, binaries in _GO_PACKAGES.items():
      package_path = _GetPackagePath(self.archive_path, package)
      with osutils.TempDir() as staging_dir:
        self._StagePackage(staging_dir, package, binaries)
        self._BuildCIPDPackage(package_path, package, staging_dir)
      logging.info('Uploading %s package artifact', package)
      self.UploadArtifact(package_path, archive=False)

  def _PreparePackagesDir(self):
    self._PrepareArchiveDir()
    packages_dir = _GetPackageDir(self.archive_path)
    osutils.SafeMakedirs(packages_dir, 0o775)

  def _PrepareArchiveDir(self):
    # Make sure local archive directory is prepared, if it was not already.
    if not os.path.exists(self.archive_path):
      self.archive.SetupArchivePath()

  def _StagePackage(self, staging_dir, package, binaries):
    """Stage binaries for packaging."""
    logging.info('Staging %s', package)
    binary_paths = [os.path.join(_GO_BINDIR, binary)
                    for binary in binaries]
    _StageChrootFilesIntoDir(staging_dir, binary_paths)

  def _BuildCIPDPackage(self, package_path, package, staging_dir):
    """Build CIPD package."""
    logging.info('Building CIPD package %s', package)
    cipd.BuildPackage(
        cipd_path=cipd.GetCIPDFromCache(),
        package='chromiumos/infra/%s' % package,
        in_dir=staging_dir,
        outfile=package_path,
    )


class RegisterInfraGoPackagesStage(generic_stages.BuilderStage,
                                   generic_stages.ArchivingStageMixin):
  """Upload infra Go binaries."""

  def PerformStage(self):
    """Upload infra Go binaries."""
    if self._run.options.debug:
      logging.info('Skipping CIPD package upload as we are in debug mode')
      return
    for package in _GO_PACKAGES:
      self._RegisterPackage(package)

  def _RegisterPackage(self, package):
    """Register CIPD package."""
    logging.info('Registering CIPD package %s', package)
    cipd.RegisterPackage(
        cipd_path=cipd.GetCIPDFromCache(),
        package_file=_GetPackagePath(self.archive_path, package),
        tags={'version': self._VersionString()},
        refs=['latest'],
        cred_path=_CRED_FILE,
    )

  def _VersionString(self):
    return self._run.attrs.version_info.VersionString()


class TestPuppetSpecsStage(generic_stages.BuilderStage):
  """Run Puppet RSpec tests."""

  def PerformStage(self):
    """Build infra Go packages."""
    # TODO(ayatane): Put this into the SDK?
    commands.RunBuildScript(
        self._build_root,
        ['emerge', 'ruby'],
        sudo=True,
        enter_chroot=True)
    commands.RunBuildScript(
        self._build_root,
        ['bash', '-c',
         'cd ../../chromeos-admin/puppet && make -j -O check GEM=gem19'],
        enter_chroot=True)


def _StageChrootFilesIntoDir(target_path, paths):
  """Install chroot files into a staging directory.

  Args:
    target_path: Path to the staging directory
    paths: An iterable of absolute paths inside the chroot
  """
  for path in paths:
    chroot_path = path_util.FromChrootPath(path)
    install_path = os.path.join(target_path, os.path.relpath(path, '/'))
    install_parent = os.path.dirname(install_path)
    osutils.SafeMakedirs(install_parent, 0o775)
    shutil.copyfile(chroot_path, install_path)
    shutil.copymode(chroot_path, install_path)


def _GetPackageDir(archive_path):
  """Get package directory."""
  return os.path.join(archive_path, 'infra_go_packages')


def _GetPackagePath(archive_path, package):
  """Get package path."""
  return os.path.join(_GetPackageDir(archive_path), '%s.cipd' % package)
