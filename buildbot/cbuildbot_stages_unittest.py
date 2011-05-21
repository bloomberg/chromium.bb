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
import chromite.buildbot.cbuildbot_config as config
import chromite.buildbot.cbuildbot_commands as commands
import chromite.buildbot.cbuildbot_stages as stages
import chromite.buildbot.manifest_version as manifest_version
import chromite.lib.cros_build_lib as cros_lib


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

    self.options = self.mox.CreateMockAnything()
    self.options.buildroot = self.build_root
    self.options.debug = False
    self.options.prebuilts = False
    self.options.clobber = False
    self.options.buildnumber = 1234
    self.overlay = os.path.join(self.build_root,
                                'src/third_party/chromiumos-overlay')
    stages.BuilderStage.rev_overlays = [self.overlay]
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
    result = stage._GetPortageEnvVar(envvar)
    self.mox.VerifyAll()
    self.assertEqual(result, 'RESULT')

  def testResolveOverlays(self):
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    for _ in range(3):
      output_obj = cros_lib.CommandResult()
      output_obj.output = 'public1 public2\n'
      cros_lib.RunCommand(mox.And(mox.IsA(list), mox.In('--noprivate')),
                          redirect_stdout=True).AndReturn(output_obj)
      output_obj = cros_lib.CommandResult()
      output_obj.output = 'private1 private2\n'
      cros_lib.RunCommand(mox.And(mox.IsA(list), mox.In('--nopublic')), \
                          redirect_stdout=True).AndReturn(output_obj)
    self.mox.ReplayAll()
    stage = self.ConstructStage()
    public_overlays = ['public1', 'public2', self.overlay]
    private_overlays = ['private1', 'private2']

    self.assertEqual(stage._ResolveOverlays('public'), public_overlays)
    self.assertEqual(stage._ResolveOverlays('private'), private_overlays)
    self.assertEqual(stage._ResolveOverlays('both'),
                     public_overlays + private_overlays)
    self.mox.VerifyAll()


class SyncStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    self.mox.StubOutWithMock(commands, 'PreFlightRinse')

  def ConstructStage(self):
    return stages.SyncStage(self.bot_id, self.options, self.build_config)

  def testFullSync(self):
    """Tests whether we can perform a full sync with a missing .repo folder."""
    self.mox.StubOutWithMock(commands, 'FullCheckout')

    os.path.isdir(self.build_root + '/.repo').AndReturn(False)
    os.path.isdir(self.build_root + '/.repo').AndReturn(False)
    commands.FullCheckout(self.build_root, self.TRACKING_BRANCH,
                          url=self.url)
    os.path.isdir(self.overlay).AndReturn(True)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testIncrementalSync(self):
    """Tests whether we can perform a standard incremental sync."""
    self.mox.StubOutWithMock(commands, 'IncrementalCheckout')
    self.mox.StubOutWithMock(stages.BuilderStage, '_GetPortageEnvVar')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    commands.PreFlightRinse(self.build_root, self.build_config['board'],
                            self.TRACKING_BRANCH,
                            [self.overlay])
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    stages.BuilderStage._GetPortageEnvVar(stages._FULL_BINHOST)
    commands.IncrementalCheckout(self.build_root)
    os.path.isdir(self.overlay).AndReturn(True)

    self.mox.ReplayAll()
    self.RunStage()
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
    self.version_file = 'version-file.sh'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'patch'

    self.build_config['manifest_version'] = self.manifest_version_url
    self.next_version = 'next_version'

    self.manager = manifest_version.BuildSpecsManager(
      self.tmpdir, self.source_repo, self.manifest_version_url, self.branch,
      self.build_name, self.incr_type, dry_run=True)

  def tearDown(self):
    shutil.rmtree(self.tmpdir)

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""

    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetNextBuildSpec')
    self.mox.StubOutWithMock(commands, 'ManifestCheckout')

    os.path.isdir(self.build_root + '/.repo').AndReturn(False)
    self.manager.GetNextBuildSpec(stages.VERSION_FILE,
        latest=True).AndReturn(self.next_version)

    commands.ManifestCheckout(self.build_root,
                              self.TRACKING_BRANCH,
                              self.next_version,
                              url=self.manifest_version_url)

    os.path.isdir('/fake_root/src/'
                  'third_party/chromiumos-overlay').AndReturn(True)

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


class BuildBoardTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)

  def ConstructStage(self):
    return stages.BuildBoardStage(self.bot_id, self.options, self.build_config)

  def testFullBuild(self):
    """Tests whether correctly run make chroot and setup board for a full."""
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(buildroot=self.build_root,
                        fast=True,
                        replace=True,
                        usepkg=False)
    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root,
                        board=self.build_config['board'],
                        fast=True,
                        usepkg=False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testBinBuild(self):
    """Tests whether we skip un-necessary steps for a binary builder."""
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(True)

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
                        usepkg=self.build_config['usepkg'])

    os.path.isdir(os.path.join(self.build_root, 'chroot/build',
                               self.build_config['board'])).AndReturn(False)
    commands.SetupBoard(self.build_root,
                        board=self.build_config['board'],
                        fast=self.build_config['fast'],
                        usepkg=self.build_config['usepkg'])

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class TestStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    self.fake_results_dir = '/tmp/fake_results_dir'

  def ConstructStage(self):
    return stages.TestStage(self.bot_id, self.options, self.build_config)

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
    commands.RunUnitTests(self.build_root, full=True)
    commands.RunTestSuite(self.build_root,
                          self.build_config['board'],
                          os.path.join(self.fake_results_dir,
                                       'test_harness'),
                          full=True)
    commands.ArchiveTestResults(self.build_root, self.fake_results_dir)

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
    commands.RunUnitTests(self.build_root, full=False)
    commands.RunTestSuite(self.build_root,
                            self.build_config['board'],
                            os.path.join(self.fake_results_dir,
                                         'test_harness'),
                            full=False)
    commands.ArchiveTestResults(self.build_root, self.fake_results_dir)

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
        self.build_root, self.TRACKING_BRANCH,
        self.build_config['board'], [self.overlay])

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
        self.build_root, self.TRACKING_BRANCH,
        self.build_config['board'], [self.overlay])

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class BuildTargetStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)

    # Disable most paths by default and selectively enable in tests

    self.build_config['vm_tests'] = False
    self.build_config['build_type'] = 'binary'
    self.build_config['usepkg'] = False
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
    self.build_config['build_type'] = 'full'
    self.build_config['usepkg'] = True
    self.build_config['fast'] = True
    self.build_config['useflags'] = ['ALPHA', 'BRAVO', 'CHARLIE']
    proper_env = {'USE' : ' '.join(self.build_config['useflags'])}

    stages.BuilderStage._GetPortageEnvVar('FULL_BINHOST').AndReturn('new.com')
    stages.BuilderStage.old_binhost = 'old.com'

    commands.Build(self.build_root,
                   build_autotest=True,
                   emptytree=True,
                   usepkg=True,
                   fast=True,
                   extra_env=proper_env)

    commands.UploadPrebuilts(
        self.build_root, self.build_config['board'],
        self.build_config['rev_overlays'], [],
        self.build_config['build_type'], False)

    commands.BuildImage(self.build_root, extra_env=proper_env)
    commands.BuildVMImageForTesting(self.build_root, extra_env=proper_env)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFalseBuildArgs1(self):
    """Make sure our logic for Build arguments can toggle to false."""
    stages.BuilderStage._GetPortageEnvVar('FULL_BINHOST').AndReturn('new.com')
    stages.BuilderStage.old_binhost = 'new.com'
    self.build_config['useflags'] = None

    commands.Build(self.build_root,
                   build_autotest=mox.IgnoreArg(),
                   emptytree=mox.IgnoreArg(),
                   fast=mox.IgnoreArg(),
                   usepkg=mox.IgnoreArg(),
                   extra_env={})
    commands.BuildImage(self.build_root, extra_env={})

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFalseBuildArgs2(self):
    """Verify emptytree flag is false when there is no old binhost."""
    stages.BuilderStage._GetPortageEnvVar('FULL_BINHOST').AndReturn('new.com')
    stages.BuilderStage.old_binhost = False
    self.build_config['useflags'] = ['BAKER']
    proper_env = {'USE' : ' '.join(self.build_config['useflags'])}

    commands.Build(self.build_root,
                   build_autotest=mox.IgnoreArg(),
                   emptytree=False,
                   fast=mox.IgnoreArg(),
                   usepkg=mox.IgnoreArg(),
                   extra_env=proper_env)

    commands.BuildImage(self.build_root, extra_env=proper_env)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class ArchiveStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)

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
        self.options.buildnumber, None,
        self.options.debug).AndReturn((None, '/archive/dir'))

    commands.UploadSymbols(self.build_root,
                           board=self._build_config['board'],
                           official=self._build_config['chromeos_official'])

    commands.PushImages(self.build_root,
                        board=self._build_config['board'],
                        branch_name='master',
                        archive_dir='/archive/dir')

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class PushChangesStageTest(AbstractStageTest):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    AbstractStageTest.setUp(self)
    os.path.isdir(self.build_root + '/.repo').AndReturn(True)

    # Disable most paths by default and selectively enable in tests

    self.build_config['build_type'] = 'full'
    self.options.chrome_rev = 'tot'
    self.options.prebuilts = True
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.mox.StubOutWithMock(commands, 'UprevPush')

  def ConstructStage(self):
    return stages.PushChangesStage(self.bot_id,
                                   self.options,
                                   self.build_config)

  def testChromePush(self):
    """Test uploading of prebuilts for chrome build."""
    self.build_config['build_type'] = 'chrome'

    commands.UploadPrebuilts(
        self.build_root, self.build_config['board'],
        self.build_config['rev_overlays'], mox.IsA(list),
        self.build_config['build_type'],
        self.options.chrome_rev)

    commands.UprevPush(
        self.build_root, self.TRACKING_BRANCH,
        self.build_config['board'], [self.overlay],
        self.options.debug)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testPreflightPush(self):
    """Test uploading of prebuilts for preflight build."""
    self.build_config['build_type'] = 'binary'

    commands.UploadPrebuilts(
        self.build_root, self.build_config['board'],
        self.build_config['rev_overlays'], mox.IsA(list),
        self.build_config['build_type'],
        self.options.chrome_rev)

    commands.UprevPush(
        self.build_root, self.TRACKING_BRANCH,
        self.build_config['board'], [self.overlay],
        self.options.debug)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFullPush(self):
    """Shouldn't upload prebuilts for a full build."""
    self.build_config['build_type'] = 'full'

    commands.UprevPush(
        self.build_root, self.TRACKING_BRANCH,
        self.build_config['board'], [self.overlay],
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

    class FailStage(stages.BuilderStage):
      """FailStage always throws an exception"""

      def _PerformStage(self):
        """Throw the exception to make us fail."""
        raise outer_self.failException

    # Run two stages
    PassStage(self.bot_id, self.options, self.build_config).Run()

    self.assertRaises(
        Exception,
        FailStage(self.bot_id, self.options, self.build_config).Run)

  def testRunStages(self):
    """Run some stages and verify the captured results"""

    stages.BuilderStage.Results.Clear()
    self.assertEqual(stages.BuilderStage.Results.Get(), [])

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', stages.BuilderStage.Results.SUCCESS),
        ('Fail', self.failException)]

    actualResults = stages.BuilderStage.Results.Get()

    # Break out the asserts to be per item to make debugging easier
    self.assertEqual(len(expectedResults), len(actualResults))
    for i in xrange(len(expectedResults)):
      self.assertEqual(expectedResults[i], actualResults[i])

  def testStagesReportSuccess(self):
    """Tests Stage reporting."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    stages.BuilderStage.Results.Clear()
    stages.BuilderStage.Results.Record('Pass',
                                       stages.BuilderStage.Results.SKIPPED)
    stages.BuilderStage.Results.Record('Pass2',
                                       stages.BuilderStage.Results.SUCCESS)
    stages.BuilderStage.Results.Record('Fail', self.failException)
    stages.BuilderStage.Results.Record(
        'FailRunCommand',
        cros_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            ['/bin/false', '/nosuchdir']))
    stages.BuilderStage.Results.Record(
        'FailOldRunCommand',
        cros_lib.RunCommandException(
            'Command "[\'/bin/false\', \'/nosuchdir\']" failed.\n',
            ['/bin/false', '/nosuchdir']))

    results = StringIO.StringIO()

    stages.BuilderStage.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** Pass previously completed\n"
        "************************************************************\n"
        "** Pass2\n"
        "************************************************************\n"
        "** Fail failed with Exception\n"
        "************************************************************\n"
        "** FailRunCommand failed in /bin/false\n"
        "************************************************************\n"
        "** FailOldRunCommand failed in /bin/false\n"
        "************************************************************\n")

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(len(expectedLines)):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportError(self):
    """Tests Stage reporting with exceptions."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    stages.BuilderStage.Results.Clear()
    stages.BuilderStage.Results.Record('Pass',
                                       stages.BuilderStage.Results.SKIPPED)
    stages.BuilderStage.Results.Record('Pass2',
                                       stages.BuilderStage.Results.SUCCESS)
    stages.BuilderStage.Results.Record('Fail', self.failException)
    stages.BuilderStage.Results.Record(
        'FailRunCommand',
        cros_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            ['/bin/false', '/nosuchdir']))
    stages.BuilderStage.Results.Record(
        'FailOldRunCommand',
        cros_lib.RunCommandException(
            'Command "[\'/bin/false\', \'/nosuchdir\']" failed.\n',
            ['/bin/false', '/nosuchdir']))

    results = StringIO.StringIO()

    stages.BuilderStage.Results.Report(results, 'Exception:\nMessage\n')

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** Pass previously completed\n"
        "************************************************************\n"
        "** Pass2\n"
        "************************************************************\n"
        "** Fail failed with Exception\n"
        "************************************************************\n"
        "** FailRunCommand failed in /bin/false\n"
        "************************************************************\n"
        "** FailOldRunCommand failed in /bin/false\n"
        "************************************************************\n"
        "\n"
        "Build failed with:\n"
        "\n"
        "Exception:\n"
        "Message\n")

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

    # Store off a known set of results and generate a report
    stages.BuilderStage.Results.Clear()
    stages.BuilderStage.Results.Record('Pass',
                                       stages.BuilderStage.Results.SUCCESS)

    results = StringIO.StringIO()

    stages.BuilderStage.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** RELEASETAG: release_tag_string\n"
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** Pass\n"
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
    stages.BuilderStage.Results.Clear()
    stages.BuilderStage.Results.Record('Pass',
                                       stages.BuilderStage.Results.SUCCESS)

    stages.BuilderStage.Results.Record('Fail',
                                       self.failException)

    stages.BuilderStage.Results.Record('Pass2',
                                       stages.BuilderStage.Results.SUCCESS)

    saveFile = StringIO.StringIO()
    stages.BuilderStage.Results.SaveCompletedStages(saveFile)
    self.assertEqual(saveFile.getvalue(), 'Pass\n')

  def testRestoreCompletedStages(self):
    """Tests that we can read in completed stages."""

    stages.BuilderStage.Results.Clear()
    stages.BuilderStage.Results.RestoreCompletedStages(
        StringIO.StringIO('Pass\n'))

    self.assertEqual(stages.BuilderStage.Results.GetPrevious(), ['Pass'])

  def testRunAfterRestore(self):
    """Tests that we skip previously completed stages."""

    # Fake stages.BuilderStage.Results.RestoreCompletedStages
    stages.BuilderStage.Results.Clear()
    stages.BuilderStage.Results.RestoreCompletedStages(
        StringIO.StringIO('Pass\n'))

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', stages.BuilderStage.Results.SKIPPED),
        ('Fail', self.failException)]

    actualResults = stages.BuilderStage.Results.Get()

    # Break out the asserts to be per item to make debugging easier
    self.assertEqual(len(expectedResults), len(actualResults))
    for i in xrange(len(expectedResults)):
      self.assertEqual(expectedResults[i], actualResults[i])


if __name__ == '__main__':
  unittest.main()
