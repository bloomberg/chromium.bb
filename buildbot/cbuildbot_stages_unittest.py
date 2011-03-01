#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

import mox
import os
import sys
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
import chromite.buildbot.cbuildbot_config as config
import chromite.buildbot.cbuildbot_commands as commands
import chromite.buildbot.cbuildbot_stages as stages
import chromite.lib.cros_build_lib as cros_lib


class BuilderStageTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-pre-flight-queue'
    self.build_config = config.config[self.bot_id]
    self.build_root = '/fake_root'
    self.url = 'fake_url'

    self.options = self.mox.CreateMockAnything()
    self.options.buildroot = self.build_root
    self.options.debug = False
    self.options.prebuilts = False
    self.options.tracking_branch = 'ooga_booga'
    self.options.clobber = False
    self.options.url = self.url
    self.overlay = os.path.join(self.build_root,
                                'src/third_party/chromiumos-overlay')
    stages.BuilderStage.rev_overlays = [self.overlay]
    stages.BuilderStage.push_overlays = [self.overlay]

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    self.mox.StubOutWithMock(cros_lib, 'OldRunCommand')
    envvar = 'EXAMPLE'
    cros_lib.OldRunCommand(mox.And(mox.IsA(list), mox.In(envvar)),
                           cwd='%s/src/scripts' % self.build_root,
                           redirect_stdout=True, enter_chroot=True,
                           error_ok=True).AndReturn('RESULT\n')
    self.mox.ReplayAll()
    stage = stages.BuilderStage(self.bot_id, self.options, self.build_config)
    result = stage._GetPortageEnvVar(envvar)
    self.mox.VerifyAll()
    self.assertEqual(result, 'RESULT')


class SyncStageTest(BuilderStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    BuilderStageTest.setUp(self)
    self.mox.StubOutWithMock(commands, 'PreFlightRinse')
    self.mox.StubOutWithMock(os.path, 'isdir')

  def testFullSync(self):
    """Tests whether we can perform a full sync with a missing .repo folder."""
    self.mox.StubOutWithMock(commands, 'FullCheckout')

    commands.PreFlightRinse(self.build_root, self.build_config['board'],
                            self.options.tracking_branch, [self.overlay])
    os.path.isdir(self.build_root + '/.repo').AndReturn(False)
    commands.FullCheckout(self.build_root, self.options.tracking_branch,
                          url=self.url)
    os.path.isdir(self.overlay).AndReturn(True)

    self.mox.ReplayAll()
    stage = stages.SyncStage(self.bot_id, self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()

  def testIncrementalSync(self):
    """Tests whether we can perform a standard incremental sync."""
    self.mox.StubOutWithMock(commands, 'IncrementalCheckout')
    self.mox.StubOutWithMock(stages.BuilderStage, '_GetPortageEnvVar')

    commands.PreFlightRinse(self.build_root, self.build_config['board'],
                            self.options.tracking_branch, [self.overlay])
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    stages.BuilderStage._GetPortageEnvVar(stages._FULL_BINHOST)
    commands.IncrementalCheckout(self.build_root)
    os.path.isdir(self.overlay).AndReturn(True)

    self.mox.ReplayAll()
    stage = stages.SyncStage(self.bot_id, self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()


class BuildBoardTest(BuilderStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    BuilderStageTest.setUp(self)
    self.mox.StubOutWithMock(os.path, 'isdir')

  def testFullBuild(self):
    """Tests whether correctly run make chroot and setup board for a full."""
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(self.build_root, self.build_config['chroot_replace'])
    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root, board=self.build_config['board'])

    self.mox.ReplayAll()
    stage = stages.BuildBoardStage(self.bot_id, self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()

  def testBinBuild(self):
    """Tests whether we skip un-necessary steps for a binary builder."""
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(True)

    self.mox.ReplayAll()
    stage = stages.BuildBoardStage(self.bot_id, self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()

  def testBinBuildAfterClobber(self):
    """Tests whether we make chroot and board after a clobber."""
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(False)
    commands.MakeChroot(self.build_root, self.build_config['chroot_replace'])
    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root, board=self.build_config['board'])


    self.mox.ReplayAll()
    stage = stages.BuildBoardStage(self.bot_id, self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()


class BuildBoardTest(BuilderStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    BuilderStageTest.setUp(self)
    self.mox.StubOutWithMock(os.path, 'isdir')

  def testFullBuild(self):
    """Tests whether correctly run make chroot and setup board for a full."""
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(self.build_root, self.build_config['chroot_replace'])
    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root, board=self.build_config['board'])

    self.mox.ReplayAll()
    stage = stages.BuildBoardStage(self.bot_id, self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()

  def testBinBuild(self):
    """Tests whether we skip un-necessary steps for a binary builder."""
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(True)

    self.mox.ReplayAll()
    stage = stages.BuildBoardStage(self.bot_id, self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()

  def testBinBuildAfterClobber(self):
    """Tests whether we make chroot and board after a clobber."""
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(False)
    commands.MakeChroot(self.build_root, self.build_config['chroot_replace'])
    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root, board=self.build_config['board'])


    self.mox.ReplayAll()
    stage = stages.BuildBoardStage(self.bot_id, self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
