# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing infra build stages."""

from __future__ import print_function

import os
import tarfile

from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import osutils


_GO_BINDIR = '/usr/bin'
_GO_PACKAGES = (
    'lucifer',
)
_GO_BINARIES = (
    'job_shepherd',
)
_GO_TARBALL_NAME = 'go_binaries.tar'
_GO_UPLOAD_PATH = 'gs://chromeos-infra/go-binaries'


class BuildInfraGoBinariesStage(generic_stages.BuilderStage):
  """Build Chromium OS infra packages."""

  def PerformStage(self):
    """Build infra Go packages."""
    cmd = ['emerge', '--deep']
    cmd.extend(_GO_PACKAGES)
    commands.RunBuildScript(self._build_root, cmd, sudo=True)


class UploadInfraGoBinariesStage(generic_stages.BuilderStage,
                                 generic_stages.ArchivingStageMixin):
  """Upload infra Go binaries."""

  def PerformStage(self):
    """Upload infra Go binaries."""
    with osutils.TempDir() as tempdir:
      tarball_path = os.path.join(tempdir, _GO_TARBALL_NAME)
      self._CreateInfraGoTarball(tarball_path)
      # Upload to chromeos-image-archive also to leave build artifacts.
      self.UploadArtifact(tarball_path)
      commands.UploadArchivedFile(
          archive_dir=os.path.dirname(tarball_path),
          upload_urls=[_GO_UPLOAD_PATH],
          filename=os.path.basename(tarball_path),
          debug=True,
      )

  def _CreateInfraGoTarball(self, tarball_path):
    """Create infra Go binary tarball."""
    tarball = tarfile.open(tarball_path, 'w')
    for binary in _GO_BINARIES:
      binary_path = os.path.join(_GO_BINDIR, binary)
      archive_path = os.path.join('bin', binary)
      tarball.add(binary_path, arcpath=archive_path)
    tarball.close()
