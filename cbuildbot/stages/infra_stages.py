# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing infra build stages."""

from __future__ import print_function

import os
import tarfile

from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import path_util


_GO_BINDIR = '/usr/bin'
_GO_PACKAGES = (
    'lucifer',
)
_GO_BINARIES = (
    'job_shepherd',
)
_GO_TARBALL_NAME = 'go_binaries-%(version)s.tar'
_GO_UPLOAD_PATH = 'gs://chromeos-infra/go-binaries'


class BuildInfraGoBinariesStage(generic_stages.BuilderStage):
  """Build Chromium OS infra packages."""

  def PerformStage(self):
    """Build infra Go packages."""
    cmd = ['emerge', '--deep']
    cmd.extend(_GO_PACKAGES)
    commands.RunBuildScript(self._build_root, cmd,
                            sudo=True, enter_chroot=True)


class UploadInfraGoBinariesStage(generic_stages.BuilderStage,
                                 generic_stages.ArchivingStageMixin):
  """Upload infra Go binaries."""

  def PerformStage(self):
    """Upload infra Go binaries."""
    with osutils.TempDir() as tempdir:
      tarball_name = self._TarballName()
      tarball_path = os.path.join(tempdir, tarball_name)
      logging.debug('Using %s for tarball name', tarball_name)
      logging.info('Creating binaries tarball')
      self._CreateInfraGoTarball(tarball_path)
      # Upload to chromeos-image-archive also to leave build artifacts.
      logging.info('Uploading standard build artifacts')
      self.UploadArtifact(tarball_path)
      logging.info('Uploading binaries for use')
      commands.UploadArchivedFile(
          archive_dir=os.path.dirname(tarball_path),
          upload_urls=[_GO_UPLOAD_PATH],
          filename=os.path.basename(tarball_path),
          debug=False,
      )

  def _VersionString(self):
    return self._run.attrs.version_info.VersionString()

  def _TarballName(self):
    return _GO_TARBALL_NAME % {
        'version': self._VersionString(),
    }

  def _CreateInfraGoTarball(self, tarball_path):
    """Create infra Go binary tarball."""
    tarball = tarfile.open(tarball_path, 'w')
    for binary in _GO_BINARIES:
      binary_path = path_util.FromChrootPath(os.path.join(_GO_BINDIR, binary))
      archive_path = os.path.join('bin', binary)
      tarball.add(binary_path, arcname=archive_path)
    tarball.close()
