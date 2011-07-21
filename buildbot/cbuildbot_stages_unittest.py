#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

import mox
import os
import shutil
import StringIO
import sys
import tempfile
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
from chromite.buildbot import cbuildbot
from chromite.buildbot import cbuildbot_config as config
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_stages as stages
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib as cros_lib


class AbstractStageTest(mox.MoxTestBase):
  """Base class for tests that test a particular build stage.

  Abstract base class that sets up the build config and options with some
  default values for testing BuilderStage and its derivatives.
  """

  TRACKING_BRANCH = 'ooga_booga'

  def ConstructStage(self):
    """Returns an instance of the stage to be tested.
    Implement in subclasses.
    """
    raise NotImplementedError, "return an instance of stage to be tested"

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-pre-flight-queue'
    self.build_config = config.config[self.bot_id]
    self.build_root = '/fake_root'

    self.url = 'fake_url'
    self.build_config['git_url'] = self.url

    # Use the cbuildbot parser to create properties and populate default values.
    parser = cbuildbot._CreateParser()
    (self.options, _) = parser.parse_args([])

    self.options.buildroot = self.build_root
    self.options.buildbot = True
    self.options.debug = False
    self.options.prebuilts = False
    self.options.clobber = False
    self.options.buildnumber = 1234
    self.overlay = os.path.join(self.build_root,
                                'src/third_party/chromiumos-overlay')
    stages.BuilderStage.overlays = [self.overlay]
    stages.BuilderStage.push_overlays = [self.overlay]

    stages.BuilderStage.SetTrackingBranch(self.TRACKING_BRANCH)

    self.mox.StubOutWithMock(os.path, 'isdir')

  def RunStage(self):
    """Creates and runs an instance of the stage to be tested.
    Requires ConstructStage() to be implemented.

    Raises:
      NotImplementedError: ConstructStage() was not implemented.
    """

    # Stage construction is usually done as late as possible because the tests
    # set up the build configuration and options used in constructing the stage.

    stage = self.ConstructStage()
    stage.Run()


class BuilderStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)

  def ConstructStage(self):
    return stages.BuilderStage(self.bot_id, self.options, self.build_config)

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    self.mox.StubOutWithMock(cros_lib, 'OldRunCommand')
    envvar = 'EXAMPLE'
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    cros_lib.OldRunCommand(mox.And(mox.IsA(list), mox.In(envvar)),
                           cwd='%s/src/scripts' % self.build_root,
                           redirect_stdout=True, enter_chroot=True,
                           error_ok=True).AndReturn('RESULT\n')
    self.mox.ReplayAll()
    stage = self.ConstructStage()
    board = self.build_config['board']
    result = stage._GetPortageEnvVar(envvar, board)
    self.mox.VerifyAll()
    self.assertEqual(result, 'RESULT')

  def testResolveOverlays(self):
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    for _ in range(3):
      output_obj = cros_lib.CommandResult()
      output_obj.output = 'public1 public2\n'
      cros_lib.RunCommand(
          mox.And(mox.IsA(list), mox.In('--noprivate')),
          print_cmd=False, redirect_stdout=True).AndReturn(output_obj)
      output_obj = cros_lib.CommandResult()
      output_obj.output = 'private1 private2\n'
      cros_lib.RunCommand(
          mox.And(mox.IsA(list), mox.In('--nopublic')),
          print_cmd=False, redirect_stdout=True).AndReturn(output_obj)
    self.mox.ReplayAll()
    stages.OVERLAY_LIST_CMD = '/bin/true'
    stage = self.ConstructStage()
    public_overlays = ['public1', 'public2', self.overlay]
    private_overlays = ['private1', 'private2']

    self.assertEqual(stage._ResolveOverlays('public'), public_overlays)
    self.assertEqual(stage._ResolveOverlays('private'), private_overlays)
    self.assertEqual(stage._ResolveOverlays('both'),
                     public_overlays + private_overlays)
    self.mox.VerifyAll()


class ManifestVersionedSyncStageTest(AbstractStageTest):
  """Tests the two (heavily related) stages ManifestVersionedSync, and
     ManifestVersionedSyncCompleted.
  """

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)

    self.tmpdir = tempfile.mkdtemp()
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'patch'

    self.build_config['manifest_version'] = self.manifest_version_url
    self.next_version = 'next_version'

    self.manager = manifest_version.BuildSpecsManager(
      self.tmpdir, self.source_repo, self.manifest_version_url, self.branch,
      self.build_name, self.incr_type, dry_run=True)

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager

  def tearDown(self):
    if os.path.exists(self.tmpdir): shutil.rmtree(self.tmpdir)

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""
    self.mox.StubOutWithMock(stages.ManifestVersionedSyncStage,
                             'InitializeManifestManager')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetNextBuildSpec')
    self.mox.StubOutWithMock(commands, 'ManifestCheckout')

    os.path.isdir(self.build_root + '/.repo').AndReturn(False)

    stages.ManifestVersionedSyncStage.InitializeManifestManager()
    self.manager.GetNextBuildSpec(force_version=None,
        latest=True).AndReturn(self.next_version)

    commands.ManifestCheckout(self.build_root,
                              self.TRACKING_BRANCH,
                              self.next_version,
                              self.url)

    os.path.isdir('/fake_root/src/third_party/'
                  'chromiumos-overlay').AndReturn(True)

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncStage(self.bot_id,
                                              self.options,
                                              self.build_config)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedSuccess(self):
    """Tests basic ManifestVersionedSyncStageCompleted on success"""

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager

    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    os.path.isdir(self.build_root + '/.repo').AndReturn(False)
    self.manager.UpdateStatus(success=True)

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.bot_id,
                                                        self.options,
                                                        self.build_config,
                                                        success=True)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedFailure(self):
    """Tests basic ManifestVersionedSyncStageCompleted on failure"""

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager

    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    os.path.isdir(self.build_root + '/.repo').AndReturn(False)
    self.manager.UpdateStatus(success=False)


    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.bot_id,
                                                        self.options,
                                                        self.build_config,
                                                        success=False)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedIncomplete(self):
    """Tests basic ManifestVersionedSyncStageCompleted on incomplete build."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    os.path.isdir(self.build_root + '/.repo').AndReturn(False)

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.bot_id,
                                                        self.options,
                                                        self.build_config,
                                                        success=False)
    stage.Run()
    self.mox.VerifyAll()


class LKGMCandidateSyncCompletionStage(AbstractStageTest):
  """Tests the two (heavily related) stages ManifestVersionedSync, and
     ManifestVersionedSyncCompleted.
  """

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)

    self.tmpdir = tempfile.mkdtemp()
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic-pre-flight-queue'
    self.build_type = constants.PFQ_TYPE

    self.build_config['manifest_version'] = True
    self.build_config['build_type'] = self.build_type

    self.manager = lkgm_manager.LKGMManager(
      self.tmpdir, self.source_repo, self.manifest_version_url, self.branch,
      self.build_name, self.build_type, dry_run=True)

  def ConstructStage(self):
    return stages.LKGMCandidateSyncCompletionStage(self.bot_id, self.options,
                                                   self.build_config,
                                                   success=True)

  def testGetImportantBuildsForMaster(self):
    test_config = {}
    test_config['test1'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
    }
    test_config['test2'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
    }
    test_config['test3'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': False,
    }
    test_config['test4'] = {
        'manifest_version': False,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
    }
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)

    self.mox.ReplayAll()
    stage = self.ConstructStage()
    important_configs = stage._GetImportantBuildersForMaster(test_config)
    self.mox.VerifyAll()

    self.assertTrue('test1' in important_configs)
    self.assertTrue('test2' in important_configs)
    self.assertFalse('test3' in important_configs)
    self.assertFalse('test4' in important_configs)


class BuildBoardTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)

  def ConstructStage(self):
    return stages.BuildBoardStage(self.bot_id, self.options, self.build_config)

  def testFullBuild(self):
    """Tests whether we correctly run make chroot and setup board for a full."""
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(os.path.join(self.build_root, '.repo')).AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(buildroot=self.build_root,
                        fast=True,
                        replace=True,
                        usepkg=False)
    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root,
                        board=self.build_config['board'],
                        fast=True,
                        usepkg=False,
                        latest_toolchain=False,
                        extra_env={},
                        profile=None)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFullBuildWithProfile(self):
    """Tests whether full builds add profile flag when requested."""
    self.bot_id = 'arm-tegra2_seaboard-tangent-private-release'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')
    self.mox.StubOutWithMock(commands, 'RunChrootUpgradeHooks')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(buildroot=self.build_root,
                        fast=True,
                        replace=True,
                        usepkg=False)
    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root,
                        board=self.build_config['board'],
                        fast=True,
                        usepkg=False,
                        latest_toolchain=False,
                        extra_env={},
                        profile=self.build_config['profile'])

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFullBuildWithOverriddenProfile(self):
    """Tests whether full builds add profile flag when requested."""
    self.bot_id = 'arm-tegra2_seaboard-tangent-private-release'
    self.options.profile = 'smock'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')
    self.mox.StubOutWithMock(commands, 'RunChrootUpgradeHooks')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(buildroot=self.build_root,
                        fast=True,
                        replace=True,
                        usepkg=False)
    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root,
                        board=self.build_config['board'],
                        fast=True,
                        usepkg=False,
                        latest_toolchain=False,
                        extra_env={},
                        profile='smock')

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()
    self.options.profile = None

  def testBinBuild(self):
    """Tests whether we skip un-necessary steps for a binary builder."""
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')
    self.mox.StubOutWithMock(commands, 'RunChrootUpgradeHooks')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self.build_config['board'])).AndReturn(True)

    commands.RunChrootUpgradeHooks(self.build_root);

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testBinBuildAfterClobber(self):
    """Tests whether we make chroot and board after a clobber."""
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(False)
    commands.MakeChroot(buildroot=self.build_root,
                        replace=self.build_config['chroot_replace'],
                        fast=self.build_config['fast'],
                        usepkg=self.build_config['usepkg_chroot'])

    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self.build_config['board'])).AndReturn(False)

    commands.SetupBoard(self.build_root,
                        board=self.build_config['board'],
                        fast=self.build_config['fast'],
                        usepkg=self.build_config['usepkg_setup_board'],
                        latest_toolchain=self.build_config['latest_toolchain'],
                        extra_env={},
                        profile=None)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class TestStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    self.fake_results_dir = '/tmp/fake_results_dir'

  def ConstructStage(self):
    return stages.TestStage(self.bot_id, self.options, self.build_config, None)

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id].copy()
    self.build_config['quick_unit'] = False
    self.build_config['quick_vm'] = False

    self.mox.StubOutWithMock(cros_lib, 'OldRunCommand')

    self.mox.StubOutWithMock(commands, 'RunUnitTests')
    self.mox.StubOutWithMock(commands, 'RunTestSuite')
    self.mox.StubOutWithMock(commands, 'ArchiveTestResults')
    self.mox.StubOutWithMock(stages.TestStage, '_CreateTestRoot')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    stages.TestStage._CreateTestRoot().AndReturn(self.fake_results_dir)
    commands.RunUnitTests(self.build_root, self.build_config['board'],
                          full=True)
    commands.RunTestSuite(self.build_root,
                          self.build_config['board'],
                          mox.IgnoreArg(),
                          os.path.join(self.fake_results_dir,
                                       'test_harness'),
                          full=True)
    commands.ArchiveTestResults(self.build_root, self.fake_results_dir, None,
                                False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testQuickTests(self):
    """Tests if quick unit and cros_au_test_harness tests are run correctly."""
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id].copy()
    self.build_config['quick_unit'] = True
    self.build_config['quick_vm'] = True

    self.mox.StubOutWithMock(commands, 'RunUnitTests')
    self.mox.StubOutWithMock(commands, 'RunTestSuite')
    self.mox.StubOutWithMock(commands, 'ArchiveTestResults')
    self.mox.StubOutWithMock(stages.TestStage, '_CreateTestRoot')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    stages.TestStage._CreateTestRoot().AndReturn(self.fake_results_dir)
    commands.RunUnitTests(self.build_root, self.build_config['board'],
                          full=False)
    commands.RunTestSuite(self.build_root,
                          self.build_config['board'],
                          mox.IgnoreArg(),
                          os.path.join(self.fake_results_dir,
                                       'test_harness'),
                          full=False)
    commands.ArchiveTestResults(self.build_root, self.fake_results_dir, None,
                                False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class TestHWStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)

  def ConstructStage(self):
    return stages.TestHWStage(self.bot_id, self.options, self.build_config)

  def testHWTest1(self):
    """Tests if remote tests commands are called
    when remoteip option is set."""
    self.bot_id = 'x86-generic-chrome-pre-flight-queue'
    self.build_config = config.config[self.bot_id].copy()
    ip = self.options.remote_ip = '1.1.1.1'

    self.mox.StubOutWithMock(cros_lib, 'OldRunCommand')
    self.mox.StubOutWithMock(commands, 'UpdateRemoteHW')
    self.mox.StubOutWithMock(commands, 'RemoteRunPyAuto')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    commands.UpdateRemoteHW(self.build_root,
                            self.build_config['board'],
                            mox.IgnoreArg(),
                            ip)
    commands.RemoteRunPyAuto(self.build_root,
                            self.build_config['board'],
                            ip,
                            internal_test=False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testHWTest2(self):
    """Tests if remote tests commands are called
    when remote_ip config is set."""
    self.bot_id = 'x86-generic-chrome-pre-flight-queue'
    self.build_config = config.config[self.bot_id].copy()
    ip = self.build_config['remote_ip'] = '1.1.1.1'

    self.mox.StubOutWithMock(cros_lib, 'OldRunCommand')
    self.mox.StubOutWithMock(commands, 'UpdateRemoteHW')
    self.mox.StubOutWithMock(commands, 'RemoteRunPyAuto')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    commands.UpdateRemoteHW(self.build_root,
                            self.build_config['board'],
                            mox.IgnoreArg(),
                            ip)
    commands.RemoteRunPyAuto(self.build_root,
                            self.build_config['board'],
                            ip,
                            internal_test=False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testHWTest3(self):
    """Tests if remote tests commands are called
    when both remoteip option and remote_ip config are set."""
    self.bot_id = 'x86-generic-chrome-pre-flight-queue'
    self.build_config = config.config[self.bot_id].copy()
    ip = self.options.remote_ip = '1.1.1.1'
    self.build_config['remote_ip'] = '2.2.2.2'

    self.mox.StubOutWithMock(cros_lib, 'OldRunCommand')
    self.mox.StubOutWithMock(commands, 'UpdateRemoteHW')
    self.mox.StubOutWithMock(commands, 'RemoteRunPyAuto')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    commands.UpdateRemoteHW(self.build_root,
                            self.build_config['board'],
                            mox.IgnoreArg(),
                            ip)
    commands.RemoteRunPyAuto(self.build_root,
                            self.build_config['board'],
                            ip,
                            internal_test=False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class UprevStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)

    # Disable most paths by default and selectively enable in tests

    self.options.chrome_rev = None
    self.build_config['uprev'] = False
    self.mox.StubOutWithMock(commands, 'MarkChromeAsStable')
    self.mox.StubOutWithMock(commands, 'UprevPackages')
    self.mox.StubOutWithMock(sys, 'exit')

  def ConstructStage(self):
    return stages.UprevStage(self.bot_id, self.options, self.build_config)

  def testChromeRevSuccess(self):
    """Case where MarkChromeAsStable returns an atom.  We shouldn't exit."""
    self.options.chrome_rev = 'tot'
    chrome_atom = 'chromeos-base/chromeos-chrome-12.0.719.0_alpha-r1'

    commands.MarkChromeAsStable(
        self.build_root,
        self.TRACKING_BRANCH,
        self.options.chrome_rev,
        self.build_config['board']).AndReturn(chrome_atom)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testChromeRevFoundNothing(self):
    """Verify we exit when MarkChromeAsStable doesn't return an atom."""
    self.options.chrome_rev = 'tot'

    commands.MarkChromeAsStable(
        self.build_root,
        self.TRACKING_BRANCH,
        self.options.chrome_rev,
        self.build_config['board'])

    sys.exit(0)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testBuildRev(self):
    """Uprevving the build without uprevving chrome."""
    self.build_config['uprev'] = True

    commands.UprevPackages(
        self.build_root,
        self.build_config['board'],
        [self.overlay])

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testNoRev(self):
    """No paths are enabled."""
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testUprevAll(self):
    """Uprev both Chrome and built packages."""
    self.build_config['uprev'] = True
    self.options.chrome_rev = 'tot'

    # Even if MarkChromeAsStable didn't find anything to rev,
    # if we rev the build then we don't exit

    commands.MarkChromeAsStable(
        self.build_root,
        self.TRACKING_BRANCH,
        self.options.chrome_rev,
        self.build_config['board']).AndReturn(None)

    commands.UprevPackages(
        self.build_root,
        self.build_config['board'],
        [self.overlay])

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class BuildTargetStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    latest_image_dir = self.build_root + '/src/build/images/x86-generic/latest'
    self.mox.StubOutWithMock(os, 'readlink')
    self.mox.StubOutWithMock(os, 'symlink')
    os.readlink(latest_image_dir).AndReturn('myimage')
    os.symlink('myimage', '%s-cbuildbot' % latest_image_dir)

    # Disable most paths by default and selectively enable in tests

    self.build_config['vm_tests'] = False
    self.build_config['build_type'] = constants.PFQ_TYPE
    self.build_config['usepkg_chroot'] = False
    self.build_config['fast'] = False

    self.options.prebuilts = True
    self.options.tests = False

    self.mox.StubOutWithMock(commands, 'Build')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.mox.StubOutWithMock(commands, 'BuildImage')
    self.mox.StubOutWithMock(commands, 'BuildVMImageForTesting')
    self.mox.StubOutWithMock(stages.BuilderStage, '_GetPortageEnvVar')

  def ConstructStage(self):
    return stages.BuildTargetStage(self.bot_id,
                                   self.options,
                                   self.build_config)

  def testAllConditionalPaths(self):
    """Enable all paths to get line coverage."""
    self.build_config['vm_tests'] = True
    self.options.tests = True
    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.build_config['usepkg_chroot'] = True
    self.build_config['usepkg_setup_board'] = True
    self.build_config['usepkg_build_packages'] = True
    self.build_config['fast'] = True
    self.build_config['useflags'] = ['ALPHA', 'BRAVO', 'CHARLIE']
    self.build_config['skip_toolchain_update'] = False

    proper_env = {'USE' : ' '.join(self.build_config['useflags'])}

    commands.Build(self.build_root,
                   self.build_config['board'],
                   build_autotest=True,
                   usepkg=True,
                   fast=True,
                   skip_toolchain_update=False,
                   extra_env=proper_env)

    commands.BuildImage(self.build_root,
                        self.build_config['board'],
                        True,
                        extra_env=proper_env)
    commands.BuildVMImageForTesting(self.build_root, self.build_config['board'],
                                    extra_env=proper_env)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFalseBuildArgs(self):
    """Make sure our logic for Build arguments can toggle to false."""
    self.build_config['useflags'] = None

    commands.Build(self.build_root,
                   self.build_config['board'],
                   build_autotest=mox.IgnoreArg(),
                   fast=mox.IgnoreArg(),
                   usepkg=mox.IgnoreArg(),
                   skip_toolchain_update=mox.IgnoreArg(),
                   extra_env={})
    commands.BuildImage(self.build_root,
                        self.build_config['board'],
                        False,
                        extra_env={})

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFalseTestArg(self):
    """Make sure our logic for build test arg can toggle to false."""
    self.build_config['vm_tests'] = False
    self.options.tests = True
    self.options.hw_tests = True
    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.build_config['usepkg_chroot'] = True
    self.build_config['usepkg_setup_board'] = True
    self.build_config['usepkg_build_packages'] = True
    self.build_config['fast'] = True
    self.build_config['useflags'] = ['ALPHA', 'BRAVO', 'CHARLIE']
    self.build_config['skip_toolchain_update'] = False

    proper_env = {'USE' : ' '.join(self.build_config['useflags'])}

    commands.Build(self.build_root,
                   self.build_config['board'],
                   build_autotest=True,
                   usepkg=True,
                   fast=True,
                   skip_toolchain_update=False,
                   extra_env=proper_env)

    commands.BuildImage(self.build_root,
                        self.build_config['board'],
                        True,
                        extra_env=proper_env)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

class ArchiveStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    self.mox.StubOutWithMock(os, 'readlink')
    latest_image_dir = self.build_root + '/src/build/images/x86-generic/latest'
    cbuildbot_link = '%s-cbuildbot' % latest_image_dir
    os.readlink(cbuildbot_link).AndReturn('myimage')

    self._build_config = self.build_config.copy()
    self._build_config['upload_symbols'] = True
    self._build_config['push_image'] = True

  def ConstructStage(self):
    return stages.ArchiveStage(self.bot_id, self.options, self._build_config)

  def testArchive(self):
    """Simple did-it-run test."""
    self.mox.StubOutWithMock(commands, 'LegacyArchiveBuild')
    self.mox.StubOutWithMock(commands, 'UploadSymbols')
    self.mox.StubOutWithMock(commands, 'PushImages')

    commands.LegacyArchiveBuild(
        self.build_root, self.bot_id, self._build_config,
        mox.IgnoreArg(), 'myimage-b1234', mox.IgnoreArg(), self.options.debug)

    commands.UploadSymbols(self.build_root,
                           board=self._build_config['board'],
                           official=self._build_config['chromeos_official'])

    commands.PushImages(self.build_root,
                        board=self._build_config['board'],
                        branch_name='master',
                        archive_dir=mox.IgnoreArg())

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testTrybotArchive(self):
    """Test archiving to a test directory with Trybots."""
    self.mox.StubOutWithMock(commands, 'LegacyArchiveBuild')
    self.mox.StubOutWithMock(commands, 'UploadSymbols')
    self.mox.StubOutWithMock(commands, 'PushImages')
    self.mox.StubOutWithMock(shutil, 'rmtree')

    self.options.buildbot = False
    shutil.rmtree(mox.Regex(r'^%s' % self.build_root), ignore_errors=True)
    commands.LegacyArchiveBuild(
        self.build_root, self.bot_id, self._build_config,
        mox.IgnoreArg(), 'myimage-b1234', mox.Regex(r'^%s' % self.build_root),
        self.options.debug)

    commands.UploadSymbols(self.build_root,
                           board=self._build_config['board'],
                           official=self._build_config['chromeos_official'])

    commands.PushImages(self.build_root,
                        board=self._build_config['board'],
                        branch_name='master',
                        archive_dir=mox.IgnoreArg())

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class UploadPrebuiltsStageTest(AbstractStageTest):
  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    self.options.chrome_rev = 'tot'
    self.options.prebuilts = True
    self.mox.StubOutWithMock(stages.UploadPrebuiltsStage, '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')

  def RunStage(self):
    """Creates and runs an instance of the stage to be tested.
    Requires ConstructStage() to be implemented.

    Raises:
      NotImplementedError: ConstructStage() was not implemented.
    """

    # Stage construction is usually done as late as possible because the tests
    # set up the build configuration and options used in constructing the stage.

    stage = self.ConstructStage()
    stage._PerformStage()

  def ConstructStage(self):
    return stages.UploadPrebuiltsStage(self.bot_id,
                                       self.options,
                                       self.build_config)

  def ConstructBinhosts(self):
    for board in (self.build_config['board'], None):
      binhost = 'http://binhost/?board=' + str(board)
      stages.UploadPrebuiltsStage._GetPortageEnvVar(stages._PORTAGE_BINHOST,
          board).AndReturn(binhost)

  def testChromeUpload(self):
    """Test uploading of prebuilts for chrome build."""
    self.build_config['build_type'] = constants.CHROME_PFQ_TYPE

    self.ConstructBinhosts()
    commands.UploadPrebuilts(
        self.build_root, self.build_config['board'],
        self.build_config['overlays'],
        self.build_config['build_type'],
        self.options.chrome_rev,
        self.options.buildnumber,
        mox.IgnoreArg()).MultipleTimes(mox.IgnoreArg())

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testPreflightUpload(self):
    """Test uploading of prebuilts for preflight build."""
    self.build_config['build_type'] = constants.PFQ_TYPE

    self.ConstructBinhosts()
    commands.UploadPrebuilts(
        self.build_root, self.build_config['board'],
        self.build_config['overlays'],
        self.build_config['build_type'],
        self.options.chrome_rev,
        self.options.buildnumber,
        mox.IgnoreArg()).MultipleTimes(mox.IgnoreArg())

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class PublishUprevChangesStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)

    # Disable most paths by default and selectively enable in tests

    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.options.chrome_rev = constants.CHROME_REV_TOT
    self.options.prebuilts = True
    self.mox.StubOutWithMock(stages.PublishUprevChangesStage,
                             '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.mox.StubOutWithMock(commands, 'UprevPush')

  def ConstructStage(self):
    return stages.PublishUprevChangesStage(self.bot_id,
                                           self.options,
                                           self.build_config)

  def testPush(self):
    """Test values for PublishUprevChanges."""
    commands.UprevPush(
        self.build_root,
        self.build_config['board'],
        [self.overlay],
        self.options.debug)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class BuildStagesResultsTest(unittest.TestCase):

  def setUp(self):
    unittest.TestCase.setUp(self)
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-pre-flight-queue'
    self.build_config = config.config[self.bot_id]
    self.build_root = '/fake_root'
    self.url = 'fake_url'

    # Create a class to hold
    class Options:
      pass

    self.options = Options()
    self.options.buildroot = self.build_root
    self.options.debug = False
    self.options.prebuilts = False
    self.options.clobber = False
    self.options.url = self.url
    self.options.buildnumber = 1234

    self.failException = Exception("FailStage needs to fail.")

  def _runStages(self):
    """Run a couple of stages so we can capture the results"""

    # Save off our self where FailStage._PerformStage can find it.
    outer_self = self

    class PassStage(stages.BuilderStage):
      """PassStage always works"""
      pass

    class Pass2Stage(stages.BuilderStage):
      """Pass2Stage always works"""
      pass

    class FailStage(stages.BuilderStage):
      """FailStage always throws an exception"""

      def _PerformStage(self):
        """Throw the exception to make us fail."""
        raise outer_self.failException

    # Run two pass stages, and one fail stage.
    PassStage(self.bot_id, self.options, self.build_config).Run()
    Pass2Stage(self.bot_id, self.options, self.build_config).Run()

    self.assertRaises(
      stages.BuildException,
      FailStage(self.bot_id, self.options, self.build_config).Run)

  def _verifyRunResults(self, expectedResults):

    actualResults = stages.Results.Get()

    # Break out the asserts to be per item to make debugging easier
    self.assertEqual(len(expectedResults), len(actualResults))
    for i in xrange(len(expectedResults)):
      name, result, description, runtime = actualResults[i]

      if result not in (stages.Results.SUCCESS,
                         stages.Results.SKIPPED):
        self.assertTrue(isinstance(description, str))

      # Skipped stages take no time, all other test stages take a little time
      if result == stages.Results.SKIPPED:
        self.assertEqual(runtime, 0)
      else:
        self.assertTrue(runtime >= 0 and runtime < 2.0)

      self.assertEqual(expectedResults[i], (name, result))

  def testRunStages(self):
    """Run some stages and verify the captured results"""

    stages.Results.Clear()
    self.assertEqual(stages.Results.Get(), [])

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', stages.Results.SUCCESS),
        ('Pass2', stages.Results.SUCCESS),
        ('Fail', self.failException)]

    self._verifyRunResults(expectedResults)

  def testSuccessTest(self):
    """Run some stages and verify the captured results"""

    stages.Results.Clear()
    stages.Results.Record('Pass', stages.Results.SKIPPED)

    self.assertTrue(stages.Results.Success())

    stages.Results.Record('Fail', self.failException, time=1)

    self.assertFalse(stages.Results.Success())

    stages.Results.Record('Pass2', stages.Results.SUCCESS)

    self.assertFalse(stages.Results.Success())

  def testStagesReportSuccess(self):
    """Tests Stage reporting."""

    stages.BuilderStage.archive_url = None
    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    stages.Results.Clear()
    stages.Results.Record('Pass', stages.Results.SKIPPED, time=1)
    stages.Results.Record('Pass2', stages.Results.SUCCESS, time=2)
    stages.Results.Record('Fail', self.failException, time=3)
    stages.Results.Record(
        'FailRunCommand',
        cros_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            ['/bin/false', '/nosuchdir'], error_code=2), time=4)
    stages.Results.Record(
        'FailOldRunCommand',
        cros_lib.RunCommandException(
            'Command "[\'/bin/false\', \'/nosuchdir\']" failed.\n',
            ['/bin/false', '/nosuchdir']), time=5)

    results = StringIO.StringIO()

    stages.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** Pass previously completed\n"
        "************************************************************\n"
        "** PASS Pass2 (0:00:02)\n"
        "************************************************************\n"
        "** FAIL Fail (0:00:03) with Exception\n"
        "************************************************************\n"
        "** FAIL FailRunCommand (0:00:04) in /bin/false\n"
        "************************************************************\n"
        "** FAIL FailOldRunCommand (0:00:05) in /bin/false\n"
        "************************************************************\n")

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(len(expectedLines)):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportError(self):
    """Tests Stage reporting with exceptions."""

    stages.BuilderStage.archive_url = None
    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    stages.Results.Clear()
    stages.Results.Record('Pass', stages.Results.SKIPPED, time=1)
    stages.Results.Record('Pass2', stages.Results.SUCCESS, time=2)
    stages.Results.Record('Fail', self.failException,
                                       'failException Msg\nLine 2', time=3)
    stages.Results.Record(
        'FailRunCommand',
        cros_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            ['/bin/false', '/nosuchdir'], error_code=2),
        'FailRunCommand msg', time=4)
    stages.Results.Record(
        'FailOldRunCommand',
        cros_lib.RunCommandException(
            'Command "[\'/bin/false\', \'/nosuchdir\']" failed.\n',
            ['/bin/false', '/nosuchdir']),
        'FailOldRunCommand msg', time=5)

    results = StringIO.StringIO()

    stages.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** Pass previously completed\n"
        "************************************************************\n"
        "** PASS Pass2 (0:00:02)\n"
        "************************************************************\n"
        "** FAIL Fail (0:00:03) with Exception\n"
        "************************************************************\n"
        "** FAIL FailRunCommand (0:00:04) in /bin/false\n"
        "************************************************************\n"
        "** FAIL FailOldRunCommand (0:00:05) in /bin/false\n"
        "************************************************************\n"
        "\n"
        "Build failed with:\n"
        "\n"
        "failException Msg\n"
        "Line 2")

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(len(expectedLines)):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportReleaseTag(self):
    """Tests Release Tag entry in stages report."""

    manifest_manager = mox.MockAnything()
    manifest_manager.current_version = "release_tag_string"
    stages.ManifestVersionedSyncStage.manifest_manager = manifest_manager

    archive_url = 'result_url'

    # Store off a known set of results and generate a report
    stages.Results.Clear()
    stages.Results.Record('Pass', stages.Results.SUCCESS, time=1)

    results = StringIO.StringIO()

    stages.Results.Report(results, archive_url)

    expectedResults = (
        "************************************************************\n"
        "** RELEASE VERSION: release_tag_string\n"
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Pass (0:00:01)\n"
        "************************************************************\n"
        "** BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n"
        "**  result_url\n"
        "************************************************************\n")

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(len(expectedLines)):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testSaveCompletedStages(self):
    """Tests that we can save out completed stages."""

    # Run this again to make sure we have the expected results stored
    stages.Results.Clear()
    stages.Results.Record('Pass', stages.Results.SUCCESS)
    stages.Results.Record('Fail', self.failException)
    stages.Results.Record('Pass2', stages.Results.SUCCESS)

    saveFile = StringIO.StringIO()
    stages.Results.SaveCompletedStages(saveFile)
    self.assertEqual(saveFile.getvalue(), 'Pass\n')

  def testRestoreCompletedStages(self):
    """Tests that we can read in completed stages."""

    stages.Results.Clear()
    stages.Results.RestoreCompletedStages(
        StringIO.StringIO('Pass\n'))

    self.assertEqual(stages.Results.GetPrevious(), ['Pass'])

  def testRunAfterRestore(self):
    """Tests that we skip previously completed stages."""

    # Fake stages.Results.RestoreCompletedStages
    stages.Results.Clear()
    stages.Results.RestoreCompletedStages(
        StringIO.StringIO('Pass\n'))

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', stages.Results.SKIPPED),
        ('Pass2', stages.Results.SUCCESS),
        ('Fail', self.failException)]

    self._verifyRunResults(expectedResults)


if __name__ == '__main__':
  unittest.main()
