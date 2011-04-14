#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for manifest_version. Needs to be run inside of chroot for mox."""

import __builtin__
import mox
import os
import shutil
import sys
import unittest

import chromite.buildbot.manifest_version as manifest_version
import chromite.lib.cros_build_lib as cros_lib

class ManifestVersionTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

    self.tmp_dir = '/foo'
    self.source_repo = 'ssh://source/repo'
    self.manifest_repo = 'ssh://manifest/repo'
    self.board = 'board-name'
    self.build_version='build-version'
    self.version_file='version-file.sh'

    self.working_dir = os.path.join(self.tmp_dir, 'working')
    self.ver_manifests = os.path.basename(self.manifest_repo)
    self.ver_manifests_dir = os.path.join(self.working_dir, 'repo')

  def testUpdateStatusSuccess(self):
    """Test if we can archive the latest results dir to Google Storage."""
    #$TOOLS/mvp.py --build-name=$BOARD --status=pass \
    #                               --build-version=$RELEASETAG


    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(manifest_version, 'SetStatus')

    os.path.exists(self.ver_manifests_dir).AndReturn(True)

    cros_lib.RunCommand(['git', 'checkout', 'master'],
                        cwd=self.ver_manifests_dir)

    manifest_version.SetStatus(self.build_version,
                               'pass',
                               self.working_dir,
                               self.ver_manifests,
                               self.board,
                               True)

    self.mox.ReplayAll()

    manifest_version.UpdateStatus(tmp_dir=self.tmp_dir,
                                  manifest_repo=self.manifest_repo,
                                  build_name=self.board,
                                  build_version=self.build_version,
                                  success=True,
                                  dry_run=True,
                                  retries=0)

    self.mox.VerifyAll()

  def testUpdateStatusFailure(self):
    """Test if we can archive the latest results dir to Google Storage."""
    #$TOOLS/mvp.py --build-name=$BOARD --status=pass \
    #                               --build-version=$RELEASETAG


    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(manifest_version, 'SetStatus')

    os.path.exists(self.ver_manifests_dir).AndReturn(True)

    cros_lib.RunCommand(['git', 'checkout', 'master'],
                        cwd=self.ver_manifests_dir)

    manifest_version.SetStatus(self.build_version,
                               'fail',
                               self.working_dir,
                               self.ver_manifests,
                               self.board,
                               True)

    self.mox.ReplayAll()

    manifest_version.UpdateStatus(tmp_dir=self.tmp_dir,
                                  manifest_repo=self.manifest_repo,
                                  build_name=self.board,
                                  build_version=self.build_version,
                                  success=False,
                                  dry_run=True,
                                  retries=0)

    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
