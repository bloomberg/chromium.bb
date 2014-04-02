#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

import contextlib
import copy
import cPickle
import itertools
import json
import mox
import os
import signal
import StringIO
import sys
import tempfile
import time
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_config as config
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import cbuildbot_run
from chromite.buildbot import cbuildbot_stages as stages
from chromite.buildbot import lab_status
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import manifest_version_unittest
from chromite.buildbot import portage_utilities
from chromite.buildbot import repository
from chromite.buildbot import validation_pool
from chromite.lib import alerts
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import git_unittest
from chromite.lib import gob_util
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.lib import timeout_util
from chromite.scripts import cbuildbot

try:
  # pylint: disable=F0401
  from crostools.lib import gspaths
  from crostools.lib import paygen_build_lib
  CROSTOOLS_AVAILABLE = True
except ImportError:
  CROSTOOLS_AVAILABLE = False


# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


MANIFEST_CONTENTS = """\
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote fetch="https://chromium.googlesource.com" name="cros" \
review="chromium-review.googlesource.com"/>

  <default remote="cros" revision="refs/heads/master" sync-j="8"/>

  <project groups="minilayout,buildtools" name="chromiumos/chromite" \
path="chromite" revision="refs/heads/special-branch"/>
  <project name="chromiumos/special" path="src/special-new" \
revision="new-special-branch"/>
  <project name="chromiumos/special" path="src/special-old" \
revision="old-special-branch"/>
</manifest>"""

CHROMITE_REVISION = "fb46d34d7cd4b9c167b74f494f2a99b68df50b18"
SPECIAL_REVISION1 = "7bc42f093d644eeaf1c77fab60883881843c3c65"
SPECIAL_REVISION2 = "6270eb3b4f78d9bffec77df50f374f5aae72b370"

VERSIONED_MANIFEST_CONTENTS = """\
<?xml version="1.0" encoding="UTF-8"?>
<manifest revision="fe72f0912776fa4596505e236e39286fb217961b">
  <remote fetch="https://chrome-internal.googlesource.com" name="chrome"/>
  <remote fetch="https://chromium.googlesource.com/" name="chromium"/>
  <remote fetch="https://chromium.googlesource.com" name="cros" \
review="chromium-review.googlesource.com"/>
  <remote fetch="https://chrome-internal.googlesource.com" name="cros-internal" \
review="https://chrome-internal-review.googlesource.com"/>
  <remote fetch="https://special.googlesource.com/" name="special" \
review="https://special.googlesource.com/"/>

  <default remote="cros" revision="refs/heads/master" sync-j="8"/>

  <project name="chromeos/manifest-internal" path="manifest-internal" \
remote="cros-internal" revision="fe72f0912776fa4596505e236e39286fb217961b" \
upstream="refs/heads/master"/>
  <project groups="minilayout,buildtools" name="chromiumos/chromite" \
path="chromite" revision="%(chromite_revision)s" \
upstream="refs/heads/master"/>
  <project name="chromiumos/manifest" path="manifest" \
revision="f24b69176b16bf9153f53883c0cc752df8e07d8b" \
upstream="refs/heads/master"/>
  <project groups="minilayout" name="chromiumos/overlays/chromiumos-overlay" \
path="src/third_party/chromiumos-overlay" \
revision="3ac713c65b5d18585e606a0ee740385c8ec83e44" \
upstream="refs/heads/master"/>
  <project name="chromiumos/special" path="src/special-new" \
revision="%(special_revision1)s" \
upstream="new-special-branch"/>
  <project name="chromiumos/special" path="src/special-old" \
revision="%(special_revision2)s" \
upstream="old-special-branch"/>
</manifest>""" % dict(chromite_revision=CHROMITE_REVISION,
                      special_revision1=SPECIAL_REVISION1,
                      special_revision2=SPECIAL_REVISION2)


DEFAULT_CHROME_BRANCH = '27'


class BuilderRunMock(partial_mock.PartialMock):
  """Partial mock for BuilderRun class."""

  TARGET = 'chromite.buildbot.cbuildbot_run._BuilderRunBase'
  ATTRS = ('GetVersionInfo', )
  VERSION = '3333.1.0'

  def GetVersionInfo(self, _build_root):
    return manifest_version.VersionInfo(
        version_string=self.VERSION, chrome_branch=DEFAULT_CHROME_BRANCH)


# pylint: disable=E1111,E1120,W0212,R0901,R0904
class StageTest(cros_test_lib.MoxTempDirTestCase,
                cros_test_lib.MockOutputTestCase):
  """Test running a single stage in isolation."""

  TARGET_MANIFEST_BRANCH = 'ooga_booga'
  BUILDROOT = 'buildroot'

  # Subclass should override this to default to a different build config
  # for its tests.
  BOT_ID = 'x86-generic-paladin'

  # Subclasses can override this.  If non-None, value is inserted into
  # self.run.attrs.release_tag.
  RELEASE_TAG = None

  def setUp(self):
    # Prepare a fake build root in self.tempdir, save at self.build_root.
    self.build_root = os.path.join(self.tempdir, self.BUILDROOT)
    osutils.SafeMakedirs(os.path.join(self.build_root, '.repo'))

    self._manager = parallel.Manager()
    self._manager.__enter__()

    # These are here to make pylint happy.  Values filled in by _Prepare.
    self.bot_id = None
    self._current_board = None
    self._boards = None

  def _Prepare(self, bot_id=None, extra_config=None, cmd_args=None,
               extra_cmd_args=None):
    """Prepare a BuilderRun at self.run for this test.

    This method must allow being called more than once.  Subclasses can
    override this method, but those subclass methods should also call this one.

    The idea is that all test preparation that falls out from the choice of
    build config and cbuildbot options should go in _Prepare.

    This will populate the following attributes on self:
      run: A BuilderRun object.
      bot_id: The bot id (name) that was used from config.config.
      self._boards: Same as self.run.config.boards.  TODO(mtennant): remove.
      self._current_board: First board in list, if there is one.

    Args:
      bot_id: Name of build config to use, defaults to self.BOT_ID.
      extra_config: Dict used to add to the build config for the given
        bot_id.  Example: {'push_image': True}.
      cmd_args: List to override the default cbuildbot command args.
      extra_cmd_args: List to add to default cbuildbot command args.  This
        is a good way to adjust an options value for your test.
        Example: ['branch-name', 'some-branch-name'] will effectively cause
        self.run.options.branch_name to be set to 'some-branch-name'.
    """
    # Use cbuildbot parser to create options object and populate default values.
    parser = cbuildbot._CreateParser()
    if not cmd_args:
      # Fill in default command args.
      cmd_args = [
          '-r', self.build_root, '--buildbot', '--noprebuilts',
          '--buildnumber', '1234',
          '--branch', self.TARGET_MANIFEST_BRANCH,
      ]
    if extra_cmd_args:
      cmd_args += extra_cmd_args
    (options, args) = parser.parse_args(cmd_args)

    # The bot_id can either be specified as arg to _Prepare method or in the
    # cmd_args (as cbuildbot normally accepts it from command line).
    if args:
      self.bot_id = args[0]
      if bot_id:
        # This means bot_id was specified as _Prepare arg and in cmd_args.
        # Make sure they are the same.
        self.assertEquals(self.bot_id, bot_id)
    else:
      self.bot_id = bot_id or self.BOT_ID
      args = [self.bot_id]
    cbuildbot._FinishParsing(options, args)

    # Populate build_config corresponding to self.bot_id.
    build_config = copy.deepcopy(config.config[self.bot_id])
    build_config['manifest_repo_url'] = 'fake_url'
    if extra_config:
      build_config.update(extra_config)
    if options.remote_trybot:
      build_config = config.OverrideConfigForTrybot(build_config, options)

    self._boards = build_config['boards']
    self._current_board = self._boards[0] if self._boards else None

    # Some preliminary sanity checks.
    self.assertEquals(options.buildroot, self.build_root)

    # Construct a real BuilderRun using options and build_config.
    self.run = cbuildbot_run.BuilderRun(options, build_config, self._manager)

    if self.RELEASE_TAG is not None:
      self.run.attrs.release_tag = self.RELEASE_TAG

    portage_utilities._OVERLAY_LIST_CMD = '/bin/true'

  def tearDown(self):
    # Mimic exiting with statement for self._manager.
    self._manager.__exit__(None, None, None)

  def AutoPatch(self, to_patch):
    """Patch a list of objects with autospec=True.

    Args:
      to_patch: A list of tuples in the form (target, attr) to patch.  Will be
      directly passed to mock.patch.object.
    """
    for item in to_patch:
      self.PatchObject(*item, autospec=True)

  def GetHWTestSuite(self):
    """Get the HW test suite for the current bot."""
    hw_tests = self.run.config['hw_tests']
    if not hw_tests:
      # TODO(milleral): Add HWTests back to lumpy-chrome-perf.
      raise unittest.SkipTest('Missing HWTest for %s' % (self.bot_id,))

    return hw_tests[0]


class AbstractStageTest(StageTest):
  """Base class for tests that test a particular build stage.

  Abstract base class that sets up the build config and options with some
  default values for testing BuilderStage and its derivatives.
  """

  def ConstructStage(self):
    """Returns an instance of the stage to be tested.
    Implement in subclasses.
    """
    raise NotImplementedError(self, "ConstructStage: Implement in your test")

  def RunStage(self):
    """Creates and runs an instance of the stage to be tested.
    Requires ConstructStage() to be implemented.

    Raises:
      NotImplementedError: ConstructStage() was not implemented.
    """

    # Stage construction is usually done as late as possible because the tests
    # set up the build configuration and options used in constructing the stage.
    results_lib.Results.Clear()
    stage = self.ConstructStage()
    stage.Run()
    self.assertTrue(results_lib.Results.BuildSucceededSoFar())


def patch(*args, **kwargs):
  """Convenience wrapper for mock.patch.object.

  Sets autospec=True by default.
  """
  kwargs.setdefault('autospec', True)
  return mock.patch.object(*args, **kwargs)


@contextlib.contextmanager
def patches(*args):
  """Context manager for a list of patch objects."""
  with cros_build_lib.ContextManagerStack() as stack:
    for arg in args:
      stack.Add(lambda: arg)
    yield


class BuilderStageTest(AbstractStageTest):
  """Tests for BuilderStage class."""

  def setUp(self):
    self._Prepare()

  def ConstructStage(self):
    return bs.BuilderStage(self.run)

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    envvar = 'EXAMPLE'
    obj = cros_test_lib.EasyAttr(output='RESULT\n')
    cros_build_lib.RunCommand(mox.And(mox.IsA(list), mox.In(envvar)),
                              cwd='%s/src/scripts' % self.build_root,
                              redirect_stdout=True, enter_chroot=True,
                              error_code_ok=True).AndReturn(obj)
    self.mox.ReplayAll()

    stage = self.ConstructStage()
    board = self._current_board
    result = stage._GetPortageEnvVar(envvar, board)
    self.mox.VerifyAll()

    self.assertEqual(result, 'RESULT')

  def testStageNamePrefixSmoke(self):
    """Basic test for the StageNamePrefix() function."""
    stage = self.ConstructStage()
    self.assertEqual(stage.StageNamePrefix(), 'Builder')

  def testGetStageNamesSmoke(self):
    """Basic test for the GetStageNames() function."""
    stage = self.ConstructStage()
    self.assertEqual(stage.GetStageNames(), ['Builder'])

  def testConstructDashboardURLSmoke(self):
    """Basic test for the ConstructDashboardURL() function."""
    stage = self.ConstructStage()

    exp_url = ('http://build.chromium.org/p/chromiumos/builders/'
               'x86-generic-paladin/builds/1234')
    self.assertEqual(stage.ConstructDashboardURL(), exp_url)

    stage_name = 'Archive'
    exp_url = '%s/steps/%s/logs/stdio' % (exp_url, stage_name)
    self.assertEqual(stage.ConstructDashboardURL(stage=stage_name), exp_url)

  def test_ExtractOverlaysSmoke(self):
    """Basic test for the _ExtractOverlays() function."""
    stage = self.ConstructStage()
    self.assertEqual(stage._ExtractOverlays(), ([], []))

  def test_PrintSmoke(self):
    """Basic test for the _Print() function."""
    stage = self.ConstructStage()
    with self.OutputCapturer():
      stage._Print('hi there')
    self.AssertOutputContainsLine('hi there', check_stderr=True)

  def test_PrintLoudlySmoke(self):
    """Basic test for the _PrintLoudly() function."""
    stage = self.ConstructStage()
    with self.OutputCapturer():
      stage._PrintLoudly('hi there')
    self.AssertOutputContainsLine(r'\*{10}', check_stderr=True)
    self.AssertOutputContainsLine('hi there', check_stderr=True)

  def testRunSmoke(self):
    """Basic passing test for the Run() function."""
    stage = self.ConstructStage()
    with self.OutputCapturer():
      stage.Run()

  def _RunCapture(self, stage):
    """Helper method to run Run() with captured output."""
    output = self.OutputCapturer()
    output.StartCapturing()
    try:
      stage.Run()
    finally:
      output.StopCapturing()

  def testRunException(self):
    """Verify stage exceptions are handled."""
    class TestError(Exception):
      """Unique test exception"""

    perform_mock = self.PatchObject(bs.BuilderStage, 'PerformStage')
    perform_mock.side_effect = TestError('fail!')

    stage = self.ConstructStage()
    results_lib.Results.Clear()
    self.assertRaises(results_lib.StepFailure, self._RunCapture, stage)

    results = results_lib.Results.Get()[0]
    self.assertTrue(isinstance(results.result, TestError))
    self.assertEqual(str(results.result), 'fail!')

  def testHandleExceptionException(self):
    """Verify exceptions in HandleException handlers are themselves handled."""
    class TestError(Exception):
      """Unique test exception"""

    class BadStage(bs.BuilderStage):
      """Stage that throws an exception when PerformStage is called."""

      handled_exceptions = []

      def PerformStage(self):
        raise TestError('first fail')

      def _HandleStageException(self, exc_info):
        self.handled_exceptions.append(str(exc_info[1]))
        raise TestError('nested')

    stage = BadStage(self.run)
    results_lib.Results.Clear()
    self.assertRaises(results_lib.StepFailure, self._RunCapture, stage)

    # Verify the results tracked the original exception.
    results = results_lib.Results.Get()[0]
    self.assertTrue(isinstance(results.result, TestError))
    self.assertEqual(str(results.result), 'first fail')

    self.assertEqual(stage.handled_exceptions, ['first fail'])


class ManifestVersionedSyncStageTest(AbstractStageTest):
  """Tests the two (heavily related) stages ManifestVersionedSync, and
     ManifestVersionedSyncCompleted.
  """
  # pylint: disable=W0223

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'branch'
    self.next_version = 'next_version'
    self.sync_stage = None

    repo = repository.RepoRepository(
      self.source_repo, self.tempdir, self.branch)
    self.manager = manifest_version.BuildSpecsManager(
      repo, self.manifest_version_url, self.build_name, self.incr_type,
      force=False, branch=self.branch, dry_run=True)

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(ManifestVersionedSyncStageTest, self)._Prepare(bot_id, **kwargs)

    self.run.config['manifest_version'] = self.manifest_version_url
    self.sync_stage = stages.ManifestVersionedSyncStage(self.run)
    self.sync_stage.manifest_manager = self.manager
    self.run.attrs.manifest_manager = self.manager

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""
    self.mox.StubOutWithMock(stages.ManifestVersionedSyncStage,
                             'Initialize')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetNextBuildSpec')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetLatestPassingSpec')
    self.mox.StubOutWithMock(stages.SyncStage, 'ManifestCheckout')

    stages.ManifestVersionedSyncStage.Initialize()
    self.manager.GetNextBuildSpec(
        dashboard_url=self.sync_stage.ConstructDashboardURL()
        ).AndReturn(self.next_version)
    self.manager.GetLatestPassingSpec().AndReturn(None)

    stages.SyncStage.ManifestCheckout(self.next_version)

    self.mox.ReplayAll()
    self.sync_stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedSuccess(self):
    """Tests basic ManifestVersionedSyncStageCompleted on success"""
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    self.manager.UpdateStatus(message=None, success=True,
                              dashboard_url=mox.IgnoreArg())

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.run,
                                                        self.sync_stage,
                                                        success=True)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedFailure(self):
    """Tests basic ManifestVersionedSyncStageCompleted on failure"""
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    self.manager.UpdateStatus(message=None, success=False,
                              dashboard_url=mox.IgnoreArg())


    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.run,
                                                        self.sync_stage,
                                                        success=False)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedIncomplete(self):
    """Tests basic ManifestVersionedSyncStageCompleted on incomplete build."""
    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.run,
                                                        self.sync_stage,
                                                        success=False)
    stage.Run()
    self.mox.VerifyAll()


class CommitQueueCompletionStageTest(cros_test_lib.TestCase):
  """Test partial functionality of CommitQueueCompletionStage."""

  def testSanityDetection(self):
    """Test the _WasBuildSane function."""
    sanity_slaves = ['sanity_1', 'sanity_2', 'sanity_3']

    passed = manifest_version.BuilderStatus(
        manifest_version.BuilderStatus.STATUS_PASSED, '')
    failed = manifest_version.BuilderStatus(
        manifest_version.BuilderStatus.STATUS_FAILED, '')
    missing = manifest_version.BuilderStatus(
        manifest_version.BuilderStatus.STATUS_MISSING, '')

    # If any sanity builder failed, build was not sane.
    slave_statuses = {'builder_a': passed,
                      'sanity_1' : missing,
                      'sanity_2' : passed,
                      'sanity_3' : failed}
    self.assertFalse(
        stages.CommitQueueCompletionStage._WasBuildSane(sanity_slaves,
                                                        slave_statuses))

    # If some sanity builders did not report a status but the others passed,
    # then build was sane.
    slave_statuses = {'builder_a': passed,
                      'sanity_1' : missing,
                      'sanity_2' : passed}

    self.assertTrue(
        stages.CommitQueueCompletionStage._WasBuildSane(sanity_slaves,
                                                        slave_statuses))

    # If all sanity builders passed, build was sane.
    slave_statuses = {'builder_a': failed,
                      'sanity_1' : passed,
                      'sanity_2' : passed,
                      'sanity_3' : passed}
    self.assertTrue(
        stages.CommitQueueCompletionStage._WasBuildSane(sanity_slaves,
                                                        slave_statuses))


class MasterSlaveSyncCompletionStage(AbstractStageTest):
  """Tests the two (heavily related) stages ManifestVersionedSync, and
     ManifestVersionedSyncCompleted.
  """
  BOT_ID = 'x86-generic-paladin'

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_type = constants.PFQ_TYPE

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(MasterSlaveSyncCompletionStage, self)._Prepare(bot_id, **kwargs)

    self.run.config['manifest_version'] = True
    self.run.config['build_type'] = self.build_type
    self.run.config['master'] = True

  def ConstructStage(self):
    sync_stage = stages.MasterSlaveSyncStage(self.run)
    return stages.MasterSlaveSyncCompletionStage(self.run, sync_stage,
                                                 success=True)

  def _GetTestConfig(self):
    test_config = {}
    test_config['test1'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': False,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
        'master': False,
    }
    test_config['test2'] = {
        'manifest_version': False,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
        'master': False,
    }
    test_config['test3'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'both',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': True,
        'master': False,
    }
    test_config['test4'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'both',
        'important': True,
        'chrome_rev': None,
        'branch': True,
        'internal': True,
        'master': False,
    }
    test_config['test5'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
        'master': False,
    }
    return test_config

  def testGetSlavesForMaster(self):
    """Tests that we get the slaves for a fake unified master configuration."""
    orig_config = stages.cbuildbot_config.config
    try:
      stages.cbuildbot_config.config = test_config = self._GetTestConfig()

      self.mox.ReplayAll()

      stage = self.ConstructStage()
      p = stage._GetSlaveConfigs()
      self.mox.VerifyAll()

      self.assertTrue(test_config['test3'] in p)
      self.assertTrue(test_config['test5'] in p)
      self.assertFalse(test_config['test1'] in p)
      self.assertFalse(test_config['test2'] in p)
      self.assertFalse(test_config['test4'] in p)

    finally:
      stages.cbuildbot_config.config = orig_config

  def testIsFailureFatal(self):
    """Tests the correctness of the _IsFailureFatal method"""
    stage = self.ConstructStage()

    # Test behavior when there are no sanity check builders
    self.assertFalse(stage._IsFailureFatal(set(), set(), set()))
    self.assertTrue(stage._IsFailureFatal(set(['test3']), set(), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(['test5']), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(), set(['test1'])))

    # Test behavior where there is a sanity check builder
    stage._run.config.sanity_check_slaves = ['sanity']
    self.assertTrue(stage._IsFailureFatal(set(['test5']), set(['sanity']),
                                          set()))
    self.assertFalse(stage._IsFailureFatal(set(), set(['sanity']), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(['sanity']),
                                          set(['test1'])))
    self.assertFalse(stage._IsFailureFatal(set(), set(),
                                           set(['sanity'])))

  def testAnnotateFailingBuilders(self):
    """Tests that _AnnotateFailingBuilders is free of syntax errors."""
    stage = self.ConstructStage()

    failing = {'a'}
    inflight = {}
    status = manifest_version.BuilderStatus('failed', 'message', 'url')
    statuses = {'a' : status}
    no_stat = set()
    stage._AnnotateFailingBuilders(failing, inflight, no_stat, statuses)

  def testExceptionHandler(self):
    """Verify _HandleStageException is sane."""
    stage = self.ConstructStage()
    e = ValueError('foo')
    try:
      raise e
    except ValueError:
      ret = stage._HandleStageException(sys.exc_info())
      self.assertTrue(isinstance(ret, tuple))
      self.assertEqual(len(ret), 3)
      self.assertEqual(ret[0], e)


# pylint: disable=W0223
class RunCommandAbstractStageTest(AbstractStageTest,
                                  cros_build_lib_unittest.RunCommandTestCase):
  """Base test class for testing a stage and mocking RunCommand."""

  FULL_BOT_ID = 'x86-generic-full'
  BIN_BOT_ID = 'x86-generic-paladin'

  def _Prepare(self, bot_id, **kwargs):
    super(RunCommandAbstractStageTest, self)._Prepare(bot_id, **kwargs)

  def _PrepareFull(self, **kwargs):
    self._Prepare(self.FULL_BOT_ID, **kwargs)

  def _PrepareBin(self, **kwargs):
    self._Prepare(self.BIN_BOT_ID, **kwargs)

  def _Run(self, dir_exists):
    """Helper for running the build."""
    with patch(os.path, 'isdir', return_value=dir_exists):
      self.RunStage()


class InitSDKTest(RunCommandAbstractStageTest):
  """Test building the SDK"""

  def setUp(self):
    self.PatchObject(cros_build_lib, 'GetChrootVersion', return_value='12')

  def ConstructStage(self):
    return stages.InitSDKStage(self.run)

  def testFullBuildWithExistingChroot(self):
    """Tests whether we create chroots for full builds."""
    self._PrepareFull()
    self._Run(dir_exists=True)
    self.assertCommandContains(['cros_sdk'])

  def testBinBuildWithMissingChroot(self):
    """Tests whether we create chroots when needed."""
    self._PrepareBin()
    # Do not force chroot replacement in build config.
    self.run._config.chroot_replace = False
    self._Run(dir_exists=False)
    self.assertCommandContains(['cros_sdk'])

  def testFullBuildWithMissingChroot(self):
    """Tests whether we create chroots when needed."""
    self._PrepareFull()
    self._Run(dir_exists=True)
    self.assertCommandContains(['cros_sdk'])

  def testFullBuildWithNoSDK(self):
    """Tests whether the --nosdk option works."""
    self._PrepareFull(extra_cmd_args=['--nosdk'])
    self._Run(dir_exists=False)
    self.assertCommandContains(['cros_sdk', '--bootstrap'])

  def testBinBuildWithExistingChroot(self):
    """Tests whether the --nosdk option works."""
    self._PrepareFull(extra_cmd_args=['--nosdk'])
    # Do not force chroot replacement in build config.
    self.run._config.chroot_replace = False
    self._Run(dir_exists=True)
    self.assertCommandContains(['cros_sdk'], expected=False)


class SetupBoardTest(RunCommandAbstractStageTest):
  """Test building the board"""

  def ConstructStage(self):
    return stages.SetupBoardStage(self.run, self._current_board)

  def _RunFull(self, dir_exists=False):
    """Helper for testing a full builder."""
    self._Run(dir_exists)
    cmd = ['./setup_board', '--board=%s' % self._current_board, '--nousepkg']
    self.assertCommandContains(cmd, expected=not dir_exists)
    cmd = ['./setup_board', '--skip_chroot_upgrade']
    self.assertCommandContains(cmd, expected=False)

  def testFullBuildWithProfile(self):
    """Tests whether full builds add profile flag when requested."""
    self._PrepareFull(extra_config={'profile': 'foo'})
    self._RunFull(dir_exists=False)
    self.assertCommandContains(['./setup_board', '--profile=foo'])

  def testFullBuildWithOverriddenProfile(self):
    """Tests whether full builds add overridden profile flag when requested."""
    self._PrepareFull(extra_cmd_args=['--profile', 'smock'])
    self._RunFull(dir_exists=False)
    self.assertCommandContains(['./setup_board', '--profile=smock'])

  def testFullBuildWithLatestToolchain(self):
    """Tests whether we use --nousepkg for creating the board"""
    self._PrepareFull()
    self._RunFull(dir_exists=False)

  def _RunBin(self, dir_exists):
    """Helper for testing a binary builder."""
    self._Run(dir_exists)
    self.assertCommandContains(['./setup_board'])
    cmd = ['./setup_board', '--nousepkg']
    self.assertCommandContains(cmd, expected=self.run.options.latest_toolchain)
    cmd = ['./setup_board', '--skip_chroot_upgrade']
    self.assertCommandContains(cmd, expected=False)

  def testBinBuildWithBoard(self):
    """Tests whether we don't create the board when it's there."""
    self._PrepareBin()
    self._RunBin(dir_exists=True)

  def testBinBuildWithMissingBoard(self):
    """Tests whether we create the board when it's missing."""
    self._PrepareBin()
    self._RunBin(dir_exists=False)

  def testBinBuildWithLatestToolchain(self):
    """Tests whether we use --nousepkg for creating the board."""
    self._PrepareBin()
    self._RunBin(dir_exists=False)

  def testSDKBuild(self):
    """Tests whether we use --skip_chroot_upgrade for SDK builds."""
    extra_config = {'build_type': constants.CHROOT_BUILDER_TYPE}
    self._PrepareFull(extra_config=extra_config)
    self._Run(dir_exists=False)
    self.assertCommandContains(['./setup_board', '--skip_chroot_upgrade'])


class SDKStageTest(AbstractStageTest):
  """Tests SDK package and Manifest creation."""
  fake_packages = [('cat1/package', '1'), ('cat1/package', '2'),
                   ('cat2/package', '3'), ('cat2/package', '4')]
  fake_json_data = {}
  fake_chroot = None

  def setUp(self):
    # Replace SudoRunCommand, since we don't care about sudo.
    self._OriginalSudoRunCommand = cros_build_lib.SudoRunCommand
    cros_build_lib.SudoRunCommand = cros_build_lib.RunCommand

    # Prepare a fake chroot.
    self.fake_chroot = os.path.join(self.build_root, 'chroot/build/amd64-host')
    osutils.SafeMakedirs(self.fake_chroot)
    osutils.Touch(os.path.join(self.fake_chroot, 'file'))
    for package, v in self.fake_packages:
      cpv = portage_utilities.SplitCPV('%s-%s' % (package, v))
      key = '%s/%s' % (cpv.category, cpv.package)
      self.fake_json_data.setdefault(key, []).append([v, {}])

  def tearDown(self):
    cros_build_lib.SudoRunCommand = self._OriginalSudoRunCommand

  def ConstructStage(self):
    return stages.SDKPackageStage(self.run)

  def testTarballCreation(self):
    """Tests whether we package the tarball and correctly create a Manifest."""
    self._Prepare('chromiumos-sdk')
    fake_tarball = os.path.join(self.build_root, 'built-sdk.tar.xz')
    fake_manifest = os.path.join(self.build_root,
                                 'built-sdk.tar.xz.Manifest')
    self.mox.StubOutWithMock(portage_utilities, 'ListInstalledPackages')
    self.mox.StubOutWithMock(stages.SDKPackageStage,
                             'CreateRedistributableToolchains')

    portage_utilities.ListInstalledPackages(self.fake_chroot).AndReturn(
        self.fake_packages)
    # This code has its own unit tests, so no need to go testing it here.
    stages.SDKPackageStage.CreateRedistributableToolchains(mox.IgnoreArg())

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

    # Check tarball for the correct contents.
    output = cros_build_lib.RunCommand(
        ['tar', '-I', 'xz', '-tvf', fake_tarball],
        capture_output=True).output.splitlines()
    # First line is './', use it as an anchor, count the chars, and strip as
    # much from all other lines.
    stripchars = len(output[0]) - 1
    tar_lines = [x[stripchars:] for x in output]
    # TODO(ferringb): replace with assertIn.
    self.assertFalse('/build/amd64-host/' in tar_lines)
    self.assertTrue('/file' in tar_lines)
    # Verify manifest contents.
    real_json_data = json.loads(osutils.ReadFile(fake_manifest))
    self.assertEqual(real_json_data['packages'],
                     self.fake_json_data)


class VMTestStageTest(AbstractStageTest):
  """Tests for the VMTest stage."""

  BOT_ID = 'x86-generic-full'
  RELEASE_TAG = ''

  def setUp(self):
    for cmd in ('RunTestSuite', 'CreateTestRoot', 'GenerateStackTraces',
                'ArchiveFile', 'ArchiveTestResults', 'UploadArchivedFile',
                'RunDevModeTest', 'RunCrosVMTest', 'ListFailedTests',
                'GetTestResultsDir', 'BuildAndArchiveTestResultsTarball'):
      self.PatchObject(commands, cmd, autospec=True)

    self.PatchObject(osutils, 'RmDir', autospec=True)
    self.PatchObject(os.path, 'isdir', autospec=True)
    self.PatchObject(os, 'listdir', autospec=True)
    self.StartPatcher(BuilderRunMock())

    self._Prepare()

    # Simulate breakpad symbols being ready.
    board_runattrs = self.run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('breakpad_symbols_generated', True)

  def ConstructStage(self):
    self.run.GetArchive().SetupArchivePath()
    return stages.VMTestStage(self.run, self._current_board)

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self.run.config['vm_tests'] = constants.FULL_AU_TEST_TYPE
    self.RunStage()

  def testQuickTests(self):
    """Tests if quick unit and cros_au_test_harness tests are run correctly."""
    self.run.config['vm_tests'] = constants.SIMPLE_AU_TEST_TYPE
    self.RunStage()


class UnitTestStageTest(AbstractStageTest):
  """Tests for the UnitTest stage."""

  BOT_ID = 'x86-generic-full'

  def setUp(self):
    self.mox.StubOutWithMock(commands, 'RunUnitTests')
    self.mox.StubOutWithMock(commands, 'TestAuZip')

    self._Prepare()

  def ConstructStage(self):
    return stages.UnitTestStage(self.run, self._current_board)

  def testQuickTests(self):
    self.mox.StubOutWithMock(os.path, 'exists')
    self.run.config['quick_unit'] = True
    commands.RunUnitTests(self.build_root, self._current_board, full=False,
                          blacklist=[], extra_env=mox.IgnoreArg())
    image_dir = os.path.join(self.build_root,
                             'src/build/images/x86-generic/latest-cbuildbot')
    os.path.exists(os.path.join(image_dir,
                                'au-generator.zip')).AndReturn(True)
    commands.TestAuZip(self.build_root, image_dir)
    self.mox.ReplayAll()

    self.RunStage()
    self.mox.VerifyAll()

  def testQuickTestsAuGeneratorZipMissing(self):
    self.mox.StubOutWithMock(os.path, 'exists')
    self.run.config['quick_unit'] = True
    commands.RunUnitTests(self.build_root, self._current_board, full=False,
                          blacklist=[], extra_env=mox.IgnoreArg())
    image_dir = os.path.join(self.build_root,
                             'src/build/images/x86-generic/latest-cbuildbot')
    os.path.exists(os.path.join(image_dir,
                                'au-generator.zip')).AndReturn(False)
    self.mox.ReplayAll()

    self.RunStage()
    self.mox.VerifyAll()

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self.mox.StubOutWithMock(os.path, 'exists')
    self.run.config['quick_unit'] = False
    commands.RunUnitTests(self.build_root, self._current_board, full=True,
                          blacklist=[], extra_env=mox.IgnoreArg())
    image_dir = os.path.join(self.build_root,
                             'src/build/images/x86-generic/latest-cbuildbot')
    os.path.exists(os.path.join(image_dir,
                                'au-generator.zip')).AndReturn(True)
    commands.TestAuZip(self.build_root, image_dir)
    self.mox.ReplayAll()

    self.RunStage()
    self.mox.VerifyAll()


class HWTestStageTest(AbstractStageTest):
  """Tests for the HWTest stage."""

  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = ''

  def setUp(self):
    self.StartPatcher(BuilderRunMock())

    self.mox.StubOutWithMock(lab_status, 'CheckLabStatus')
    self.mox.StubOutWithMock(commands, 'HaveCQHWTestsBeenAborted')
    self.mox.StubOutWithMock(commands, 'RunHWTestSuite')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotStepWarnings')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotStepFailure')
    self.mox.StubOutWithMock(cros_build_lib, 'Warning')
    self.mox.StubOutWithMock(cros_build_lib, 'Error')

    self.suite_config = None
    self.suite = None

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(HWTestStageTest, self)._Prepare(bot_id, **kwargs)

    self.run.options.log_dir = '/b/cbuild/mylogdir'

    self.suite_config = self.GetHWTestSuite()
    self.suite = self.suite_config.suite

  def ConstructStage(self):
    self.run.GetArchive().SetupArchivePath()
    return stages.HWTestStage(self.run, self._current_board, self.suite_config)

  def _RunHWTestSuite(self, debug=False, returncode=0, fails=False,
                      timeout=False):
    """Pretend to run the HWTest suite to assist with tests.

    Args:
      debug: Whether the HWTest suite should be run in debug mode.
      returncode: The return value of the HWTest command.
      fails: Whether the command as a whole should fail.
      timeout: Whether the the hw tests should time out.
    """
    if config.IsCQType(self.run.config.build_type):
      version = self.run.GetVersion()
      for _ in range(1 + int(fails)):
        commands.HaveCQHWTestsBeenAborted(version).AndReturn(False)

    lab_status.CheckLabStatus(mox.IgnoreArg())
    m = commands.RunHWTestSuite(mox.IgnoreArg(),
                                self.suite,
                                self._current_board, mox.IgnoreArg(),
                                mox.IgnoreArg(), mox.IgnoreArg(), True,
                                mox.IgnoreArg(), mox.IgnoreArg(), debug)

    # Raise an exception if the user wanted the command to fail.
    if timeout:
      m.AndRaise(timeout_util.TimeoutError('Timed out'))
      cros_build_lib.PrintBuildbotStepFailure()
      cros_build_lib.Error(mox.IgnoreArg())
    elif returncode != 0:
      result = cros_build_lib.CommandResult(cmd='run_hw_tests',
                                            returncode=returncode)
      m.AndRaise(cros_build_lib.RunCommandError('HWTests failed', result))

      # Make sure failures are logged correctly.
      if fails:
        cros_build_lib.PrintBuildbotStepFailure()
        cros_build_lib.Error(mox.IgnoreArg())
      else:
        cros_build_lib.Warning(mox.IgnoreArg())
        cros_build_lib.PrintBuildbotStepWarnings()
        cros_build_lib.Warning(mox.IgnoreArg())

    self.mox.ReplayAll()
    if fails or timeout:
      self.assertRaises(results_lib.StepFailure, self.RunStage)
    else:
      self.RunStage()
    self.mox.VerifyAll()

  def testRemoteTrybotWithHWTest(self):
    """Test remote trybot with hw test enabled"""
    cmd_args = ['--remote-trybot', '-r', self.build_root, '--hwtest']
    self._Prepare(cmd_args=cmd_args)
    self._RunHWTestSuite()

  def testRemoteTrybotNoHWTest(self):
    """Test remote trybot with no hw test"""
    cmd_args = ['--remote-trybot', '-r', self.build_root]
    self._Prepare(cmd_args=cmd_args)
    self._RunHWTestSuite(debug=True)

  def testWithSuite(self):
    """Test if run correctly with a test suite."""
    self._RunHWTestSuite()

  def testWithTimeout(self):
    """Test if run correctly with a critical timeout."""
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(timeout=True)

  def testWithSuiteWithInfrastructureFailure(self):
    """Tests that we warn correctly if we get a returncode of 2."""
    self._RunHWTestSuite(returncode=2)

  def testWithSuiteWithFatalFailure(self):
    """Tests that we fail if we get a returncode of 1."""
    self._RunHWTestSuite(returncode=1, fails=True)

  def testSendPerfResults(self):
    """Tests that we can send perf results back correctly."""
    self._Prepare('lumpy-chrome-perf')
    self.suite = 'perf_v2'

    self.mox.StubOutWithMock(stages.HWTestStage, '_PrintFile')

    with gs_unittest.GSContextMock() as gs_mock:
      gs_mock.SetDefaultCmdResult()
      self._RunHWTestSuite()

  def testHandleLabDownAsWarning(self):
    """Test that buildbot warn when lab is down."""
    cros_build_lib.Warning(mox.IgnoreArg())
    check_lab = lab_status.CheckLabStatus(mox.IgnoreArg())
    check_lab.AndRaise(lab_status.LabIsDownException('Lab is not up.'))
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning(mox.IgnoreArg())
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class SignerResultsStageTest(AbstractStageTest):
  """Test the SignerResultsStage."""

  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = '0.0.1'

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self._Prepare()

    self.signer_result = """
      { "status": { "status": "passed" }, "board": "link",
      "keyset": "link-mp-v4", "type": "recovery", "channel": "stable" }
      """

    self.insns_urls_per_channel = {
        'chan1': ['chan1_uri1', 'chan1_uri2'],
        'chan2': ['chan2_uri1'],
    }


  def ConstructStage(self):
    archive_stage = stages.ArchiveStage(self.run, self._current_board)
    stage = stages.SignerResultsStage(
        self.run, self._current_board, archive_stage)

    return stage

  def testPerformStageSuccess(self):
    """Test that SignerResultsStage works when signing works."""
    results = ['chan1_uri1.json', 'chan1_uri2.json', 'chan2_uri1.json']

    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.signer_result

      stage = self.ConstructStage()
      stage.archive_stage._push_image_status_queue.put(
          self.insns_urls_per_channel)

      stage.PerformStage()
      for result in results:
        mock_gs_ctx.Cat.assert_any_call(result)

      self.assertEqual(stage.archive_stage.WaitForChannelSigning(), 'chan1')
      self.assertEqual(stage.archive_stage.WaitForChannelSigning(), 'chan2')
      self.assertEqual(stage.archive_stage.WaitForChannelSigning(),
                       stages.SignerResultsStage.FINISHED)

  def testPerformStageSuccessNothingSigned(self):
    """Test that SignerResultsStage passes when there are no signed images."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.signer_result

      stage = self.ConstructStage()
      stage.archive_stage._push_image_status_queue.put({})

      stage.PerformStage()
      self.assertFalse(mock_gs_ctx.Cat.called)
      self.assertEqual(stage.archive_stage.WaitForChannelSigning(),
                       stages.SignerResultsStage.FINISHED)

  def testPerformStageFailure(self):
    """Test that SignerResultsStage errors when the signers report an error."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = """
          { "status": { "status": "failed" }, "board": "link",
            "keyset": "link-mp-v4", "type": "recovery", "channel": "stable" }
          """
      stage = self.ConstructStage()
      stage.archive_stage._push_image_status_queue.put({
          'chan1': ['chan1_uri1'],
      })
      self.assertRaises(stages.SignerFailure, stage.PerformStage)

  def testPerformStageMalformedJson(self):
    """Test that SignerResultsStage errors when invalid Json is received.."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = "{"

      stage = self.ConstructStage()
      stage.archive_stage._push_image_status_queue.put({
          'chan1': ['chan1_uri1'],
      })
      self.assertRaises(stages.MalformedResultsException, stage.PerformStage)

  def testPerformStageTimeout(self):
    """Test that SignerResultsStage reports timeouts correctly."""
    with patch(stages.timeout_util, 'WaitForSuccess') as mock_wait:
      mock_wait.side_effect = timeout_util.TimeoutError

      stage = self.ConstructStage()
      stage.archive_stage._push_image_status_queue.put({
          'chan1': ['chan1_uri1'],
      })
      self.assertRaises(stages.SignerResultsTimeout, stage.PerformStage)

  def testCheckForResultsSuccess(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.signer_result

      stage = self.ConstructStage()
      self.assertTrue(
          stage._CheckForResults(mock_gs_ctx,
          self.insns_urls_per_channel))

  def testCheckForResultsSuccessNoChannels(self):
    """Test that SignerResultsStage works when there is nothing to check for."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value

      stage = self.ConstructStage()

      # Ensure we find that we are ready if there are no channels to look for.
      self.assertTrue(stage._CheckForResults(mock_gs_ctx, {}))

      # Ensure we didn't contact GS while checking for no channels.
      self.assertFalse(mock_gs_ctx.Cat.called)

  def testCheckForResultsPartialComplete(self):
    """Verify _CheckForResults handles partial signing results."""
    def catChan2Success(url):
      if url.startswith('chan2'):
        result = mock.Mock()
        result.output = self.signer_result
        return result
      else:
        raise stages.gs.GSNoSuchKey()

    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.side_effect = catChan2Success

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
          self.insns_urls_per_channel))
      self.assertEqual(stage.signing_results, {
          'chan1': {},
          'chan2': {
              'chan2_uri1.json': {
                  'board': 'link',
                  'channel': 'stable',
                  'keyset': 'link-mp-v4',
                  'status': {'status': 'passed'},
                  'type': 'recovery'
              }
          }
      })

  def testCheckForResultsUnexpectedJson(self):
    """Verify _CheckForResults handles unexpected Json values."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = "{}"

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
          self.insns_urls_per_channel))
      self.assertEqual(stage.signing_results, {
          'chan1': {}, 'chan2': {}
      })

  def testCheckForResultsNoResult(self):
    """Verify _CheckForResults handles missing signer results."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.side_effect = stages.gs.GSNoSuchKey

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
          self.insns_urls_per_channel))
      self.assertEqual(stage.signing_results, {
          'chan1': {}, 'chan2': {}
      })


class PaygenStageTest(StageTest):
  """Test the PaygenStageStage."""

  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = '0.0.1'

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self._Prepare()

  def ConstructStage(self):
    archive_stage = stages.ArchiveStage(self.run, self._current_board)
    return stages.PaygenStage(self.run, self._current_board, archive_stage)

  def testPerformStageSuccess(self):
    """Test that SignerResultsStage works when signing works."""

    with patch(stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()
      stage.archive_stage.AnnounceChannelSigned('stable')
      stage.archive_stage.AnnounceChannelSigned('beta')
      stage.archive_stage.AnnounceChannelSigned(
          stages.SignerResultsStage.FINISHED)
      stage.PerformStage()

      # Verify that we queue up work
      queue.put.assert_any_call(('stable', 'x86-mario', '0.0.1', False, True))
      queue.put.assert_any_call(('beta', 'x86-mario', '0.0.1', False, True))

  def testPerformStageNoChannels(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()
      stage.archive_stage.AnnounceChannelSigned(
          stages.SignerResultsStage.FINISHED)
      stage.PerformStage()

      # Ensure no work was queued up.
      self.assertFalse(queue.put.called)

  def testPerformSigningFailed(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()
      stage.archive_stage.AnnounceChannelSigned(None)

      self.assertRaises(stages.PaygenSigningRequirementsError,
                        stage.PerformStage)

      # Ensure no work was queued up.
      self.assertFalse(queue.put.called)

  def testPerformTrybot(self):
    """Test the PerformStage alternate behavior for trybot runs."""
    with patch(stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      # The stage is constructed differently for trybots, so don't use
      # ConstructStage.
      stage = stages.PaygenStage(self.run, self._current_board,
                                 archive_stage=None, channels=['foo', 'bar'])
      stage.PerformStage()

      # Notice that we didn't put anything in _wait_for_channel_signing, but
      # still got results right away.
      queue.put.assert_any_call(('foo', 'x86-mario', '0.0.1', False, True))
      queue.put.assert_any_call(('bar', 'x86-mario', '0.0.1', False, True))

  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testRunPaygenInProcess(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(paygen_build_lib, 'CreatePayloads') as create_payloads:
      # Call the method under test.
      stage = self.ConstructStage()
      stage._RunPaygenInProcess('foo', 'foo-board', 'foo-version', False, True)

      # Ensure arguments are properly converted and passed along.
      create_payloads.assert_called_with(gspaths.Build(version='foo-version',
                                                       board='foo-board',
                                                       channel='foo-channel'),
                                         dry_run=False,
                                         work_dir=mock.ANY,
                                         run_parallel=True,
                                         run_on_builder=True,
                                         skip_test_payloads=False,
                                         skip_autotest=False)

  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testRunPaygenInProcessComplex(self):
    """Test that SignerResultsStage with arguments that are more unusual."""
    with patch(paygen_build_lib, 'CreatePayloads') as create_payloads:
      # Call the method under test.
      # Use release tools channel naming, and a board name including a variant.
      stage = self.ConstructStage()
      stage._RunPaygenInProcess('foo-channel', 'foo-board_variant',
                                'foo-version', True, False)

      # Ensure arguments are properly converted and passed along.
      create_payloads.assert_called_with(
          gspaths.Build(version='foo-version',
                        board='foo-board-variant',
                        channel='foo-channel'),
          dry_run=True,
          work_dir=mock.ANY,
          run_parallel=True,
          run_on_builder=True,
          skip_test_payloads=True,
          skip_autotest=True)


class AUTestStageTest(AbstractStageTest,
                      cros_build_lib_unittest.RunCommandTestCase):
  """Test only custom methods in AUTestStageTest."""
  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = '0.0.1'

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self.PatchObject(commands, 'ArchiveFile', autospec=True,
                     return_value='foo.txt')
    self.PatchObject(commands, 'HaveCQHWTestsBeenAborted', autospec=True,
                     return_value=False)
    self.PatchObject(lab_status, 'CheckLabStatus', autospec=True)

    self.archive_stage = None
    self.suite_config = None
    self.suite = None

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(AUTestStageTest, self)._Prepare(bot_id, **kwargs)

    self.run.GetArchive().SetupArchivePath()
    self.archive_stage = stages.ArchiveStage(self.run, self._current_board)
    self.suite_config = self.GetHWTestSuite()
    self.suite = self.suite_config.suite

  def ConstructStage(self):
    return stages.AUTestStage(self.run, self._current_board, self.suite_config)

  def testPerformStage(self):
    """Tests that we correctly generate a tarball and archive it."""
    stage = self.ConstructStage()
    stage.PerformStage()
    cmd = ['site_utils/autoupdate/full_release_test.py', '--npo', '--dump',
           '--archive_url', self.archive_stage.upload_url,
           self.archive_stage.release_tag, self._current_board]
    self.assertCommandContains(cmd)
    self.assertCommandContains([commands._AUTOTEST_RPC_CLIENT, self.suite])


class UprevStageTest(AbstractStageTest):
  """Tests for the UprevStage class."""

  def setUp(self):
    self.mox.StubOutWithMock(commands, 'UprevPackages')

    self._Prepare()

  def ConstructStage(self):
    return stages.UprevStage(self.run)

  def testBuildRev(self):
    """Uprevving the build without uprevving chrome."""
    self.run.config['uprev'] = True
    commands.UprevPackages(self.build_root, self._boards, [], enter_chroot=True)
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testNoRev(self):
    """No paths are enabled."""
    self.run.config['uprev'] = False
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class ArchivingMock(partial_mock.PartialMock):
  """Partial mock for ArchivingStage."""

  TARGET = 'chromite.buildbot.cbuildbot_stages.ArchivingStage'
  ATTRS = ('UploadArtifact',)

  def UploadArtifact(self, *args, **kwargs):
    with patch(commands, 'ArchiveFile', return_value='foo.txt'):
      with patch(commands, 'UploadArchivedFile'):
        self.backup['UploadArtifact'](*args, **kwargs)


class BuildPackagesStageTest(AbstractStageTest):
  """Tests BuildPackagesStage."""

  def setUp(self):
    self._release_tag = None

    self.StartPatcher(BuilderRunMock())

  def ConstructStage(self):
    self.run.attrs.release_tag = self._release_tag
    return stages.BuildPackagesStage(self.run, self._current_board)

  @contextlib.contextmanager
  def RunStageWithConfig(self):
    """Run the given config"""
    try:
      with cros_build_lib_unittest.RunCommandMock() as rc:
        rc.SetDefaultCmdResult()
        with cros_test_lib.OutputCapturer():
          with cros_test_lib.LoggingCapturer():
            self.RunStage()

        yield rc

    except AssertionError as ex:
      msg = '%s failed the following test:\n%s' % (self.bot_id, ex)
      raise AssertionError(msg)

  def RunTestsWithBotId(self, bot_id, options_tests=True):
    """Test with the config for the specified bot_id."""
    self._Prepare(bot_id)
    self.run.options.tests = options_tests

    with self.RunStageWithConfig() as rc:
      cfg = self.run.config
      rc.assertCommandContains(['./build_packages'])
      rc.assertCommandContains(['./build_packages', '--skip_chroot_upgrade'])
      rc.assertCommandContains(['./build_packages', '--nousepkg'],
                               expected=not cfg['usepkg_build_packages'])
      build_tests = cfg['build_tests'] and self.run.options.tests
      rc.assertCommandContains(['./build_packages', '--nowithautotest'],
                               expected=not build_tests)

  def testAllConfigs(self):
    """Test all major configurations"""
    task = self.RunTestsWithBotId
    with parallel.BackgroundTaskRunner(task) as queue:
      # Loop through all major configuration types and pick one from each.
      for bot_type in config.CONFIG_TYPE_DUMP_ORDER:
        for bot_id in config.config:
          if bot_id.endswith(bot_type):
            # Skip any config without a board, since those configs do not
            # build packages.
            cfg = config.config[bot_id]
            if cfg.boards:
              queue.put([bot_id])
              break

  def testNoTests(self):
    """Test that self.options.tests = False works."""
    self.RunTestsWithBotId('x86-generic-paladin', options_tests=False)


class BuildImageStageMock(ArchivingMock):
  """Partial mock for BuildImageStage."""

  TARGET = 'chromite.buildbot.cbuildbot_stages.BuildImageStage'
  ATTRS = ArchivingMock.ATTRS + ('_BuildAutotestTarballs', '_BuildImages',
                                 '_GenerateAuZip')

  def _BuildAutotestTarballs(self, *args, **kwargs):
    with patches(
        patch(commands, 'BuildTarball'),
        patch(commands, 'FindFilesWithPattern', return_value=['foo.txt'])):
      self.backup['_BuildAutotestTarballs'](*args, **kwargs)

  def _BuildImages(self, *args, **kwargs):
    with patches(
        patch(os, 'symlink'),
        patch(os, 'readlink', return_value='foo.txt')):
      self.backup['_BuildImages'](*args, **kwargs)

  def _GenerateAuZip(self, *args, **kwargs):
    with patch(git, 'ReinterpretPathForChroot', return_value='/chroot/path'):
      self.backup['_GenerateAuZip'](*args, **kwargs)


class BuildImageStageTest(BuildPackagesStageTest):
  """Tests BuildImageStage."""

  def setUp(self):
    self.StartPatcher(BuildImageStageMock())

  def ConstructStage(self):
    return stages.BuildImageStage(self.run, self._current_board)

  def RunTestsWithReleaseConfig(self, release_tag):
    self._release_tag = release_tag

    with parallel_unittest.ParallelMock():
      with self.RunStageWithConfig() as rc:
        cfg = self.run.config
        cmd = ['./build_image', '--version=%s' % (self._release_tag or '')]
        rc.assertCommandContains(cmd, expected=cfg['images'])
        rc.assertCommandContains(['./image_to_vm.sh'],
                                 expected=cfg['vm_tests'])
        hw = cfg['upload_hw_test_artifacts']
        canary = (cfg['build_type'] == constants.CANARY_TYPE)
        rc.assertCommandContains(['--full_payload'], expected=hw and not canary)
        rc.assertCommandContains(['--nplus1'], expected=hw and canary)
        cmd = ['./build_library/generate_au_zip.py', '-o', '/chroot/path']
        rc.assertCommandContains(cmd, expected=cfg['images'])

  def RunTestsWithBotId(self, bot_id, options_tests=True):
    """Test with the config for the specified bot_id."""
    release_tag = '0.0.1'
    self._Prepare(bot_id)
    self.run.options.tests = options_tests
    self.run.attrs.release_tag = release_tag

    task = self.RunTestsWithReleaseConfig
    steps = [lambda: task(tag) for tag in (None, release_tag)]
    parallel.RunParallelSteps(steps)


class ArchivingStageTest(AbstractStageTest):
  """Excerise ArchivingStage functionality."""
  RELEASE_TAG = ''

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self.StartPatcher(ArchivingMock())

    self._Prepare()

  def ConstructStage(self):
    self.run.GetArchive().SetupArchivePath()
    archive_stage = stages.ArchiveStage(self.run, self._current_board)
    return stages.ArchivingStage(self.run, self._current_board, archive_stage)

  def testMetadataJson(self):
    """Test that the json metadata is built correctly"""
    # First add some results to make sure we can handle various types.
    results_lib.Results.Clear()
    results_lib.Results.Record('Sync', results_lib.Results.SUCCESS, time=1)
    results_lib.Results.Record('Build', results_lib.Results.SUCCESS, time=2)
    results_lib.Results.Record('Test', FailStage.FAIL_EXCEPTION, time=3)
    results_lib.Results.Record('SignerTests', results_lib.Results.SKIPPED)

    # Now run the code.
    stage = self.ConstructStage()
    stage.UploadMetadata(stage='tests')

    # Now check the results.
    json_file = os.path.join(
        stage.archive_path,
        constants.METADATA_STAGE_JSON % { 'stage': 'tests' } )
    json_data = json.loads(osutils.ReadFile(json_file))

    important_keys = (
        'boards',
        'bot-config',
        'metadata-version',
        'results',
        'sdk-version',
        'toolchain-tuple',
        'toolchain-url',
        'version',
    )
    for key in important_keys:
      self.assertTrue(key in json_data)

    self.assertEquals(json_data['boards'], ['x86-generic'])
    self.assertEquals(json_data['bot-config'], 'x86-generic-paladin')
    self.assertEquals(json_data['version']['full'], stage.version)
    self.assertEquals(json_data['metadata-version'], '2')

    results_passed = ('Sync', 'Build',)
    results_failed = ('Test',)
    results_skipped = ('SignerTests',)
    for result in json_data['results']:
      if result['name'] in results_passed:
        self.assertEquals(result['status'], constants.FINAL_STATUS_PASSED)
      elif result['name'] in results_failed:
        self.assertEquals(result['status'], constants.FINAL_STATUS_FAILED)
      elif result['name'] in results_skipped:
        self.assertEquals(result['status'], constants.FINAL_STATUS_PASSED)
        self.assertTrue('skipped' in result['summary'].lower())

    # The buildtools manifest doesn't have any overlays. In this case, we can't
    # find any toolchains.
    overlays = portage_utilities.FindOverlays(
        constants.BOTH_OVERLAYS, board=None, buildroot=self.build_root)
    overlay_tuples = ['i686-pc-linux-gnu', 'arm-none-eabi']
    self.assertEquals(json_data['toolchain-tuple'],
                      overlay_tuples if overlays else [])


class ArchiveStageTest(AbstractStageTest):
  """Exercise ArchiveStage functionality."""
  RELEASE_TAG = ''
  VERSION = '3333.1.0'

  def _PatchDependencies(self):
    """Patch dependencies of ArchiveStage.PerformStage()."""
    to_patch = [
        (parallel, 'RunParallelSteps'), (commands, 'PushImages'),
        (commands, 'RemoveOldArchives'), (commands, 'UploadArchivedFile')]
    self.AutoPatch(to_patch)

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self._PatchDependencies()

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    extra_config = {'upload_symbols': True, 'push_image': True}
    super(ArchiveStageTest, self)._Prepare(bot_id, extra_config=extra_config,
                                           **kwargs)

  def ConstructStage(self):
    self.run.GetArchive().SetupArchivePath()
    return stages.ArchiveStage(self.run, self._current_board)

  def testArchive(self):
    """Simple did-it-run test."""
    # TODO(davidjames): Test the individual archive steps as well.
    self.RunStage()
    # pylint: disable=E1101
    self.assertTrue(commands.RemoveOldArchives.called)

  # TODO(build): This test is not actually testing anything real.  It confirms
  # that PushImages is not called, but the mock for RunParallelSteps already
  # prevents PushImages from being called, regardless of whether this is a
  # trybot flow.
  def testNoPushImagesForRemoteTrybot(self):
    """Test that remote trybot overrides work to disable push images."""
    self._Prepare('x86-mario-release',
                  cmd_args=['--remote-trybot', '-r', self.build_root,
                            '--buildnumber=1234'])
    self.RunStage()
    # pylint: disable=E1101
    self.assertEquals(commands.PushImages.call_count, 0)

  def ConstructStageForArchiveStep(self):
    """Stage construction for archive steps."""
    stage = self.ConstructStage()
    self.PatchObject(stage._upload_queue, 'put', autospec=True)
    self.PatchObject(git, 'ReinterpretPathForChroot', return_value='',
                     autospec=True)
    return stage

  def testBuildAndArchiveDeltaSysroot(self):
    """Test tarball is added to upload queue."""
    stage = self.ConstructStageForArchiveStep()
    with cros_build_lib_unittest.RunCommandMock() as rc:
      rc.SetDefaultCmdResult()
      stage.BuildAndArchiveDeltaSysroot()
    stage._upload_queue.put.assert_called_with([constants.DELTA_SYSROOT_TAR])

  def testBuildAndArchiveDeltaSysrootFailure(self):
    """Test tarball not added to upload queue on command exception."""
    stage = self.ConstructStageForArchiveStep()
    with cros_build_lib_unittest.RunCommandMock() as rc:
      rc.AddCmdResult(partial_mock.In('generate_delta_sysroot'), returncode=1,
                      error='generate_delta_sysroot: error')
      self.assertRaises2(cros_build_lib.RunCommandError,
                        stage.BuildAndArchiveDeltaSysroot)
    self.assertFalse(stage._upload_queue.put.called)


class UploadPrebuiltsStageTest(RunCommandAbstractStageTest):
  """Tests for the UploadPrebuilts stage."""

  CMD = './upload_prebuilts'
  RELEASE_TAG = ''

  def setUp(self):
    self.StartPatcher(BuilderRunMock())

  def _Prepare(self, bot_id=None, **kwargs):
    super(UploadPrebuiltsStageTest, self)._Prepare(bot_id, **kwargs)

    self.run.options.prebuilts = True

  def ConstructStage(self):
    return stages.UploadPrebuiltsStage(self.run,
                                       self.run.config.boards[-1])

  def _VerifyBoardMap(self, bot_id, count, board_map, public_args=None,
                      private_args=None):
    """Verify that the prebuilts are uploaded for the specified bot.

    Args:
      bot_id: Bot to upload prebuilts for.
      count: Number of assert checks that should be performed.
      board_map: Map from slave boards to whether the bot is public.
      public_args: List of extra arguments for public boards.
      private_args: List of extra arguments for private boards.
    """
    self._Prepare(bot_id)
    self.RunStage()
    public_prefix = [self.CMD] + (public_args or [])
    private_prefix = [self.CMD] + (private_args or [])
    for board, public in board_map.iteritems():
      if public or public_args:
        public_cmd = public_prefix + ['--slave-board', board]
        self.assertCommandContains(public_cmd, expected=public)
        count -= 1
      private_cmd = private_prefix + ['--slave-board', board, '--private']
      self.assertCommandContains(private_cmd, expected=not public)
      count -= 1
    if board_map:
      self.assertCommandContains([self.CMD, '--set-version',
                                  self.run.GetVersion()])
      count -= 1
    self.assertEqual(count, 0,
        'Number of asserts performed does not match (%d remaining)' % count)

  def testFullPrebuiltsUpload(self):
    """Test uploading of full builder prebuilts."""
    self._VerifyBoardMap('x86-generic-full', 0, {})
    self.assertCommandContains([self.CMD, '--git-sync'])

  def testIncorrectCount(self):
    """Test that _VerifyBoardMap asserts when the count is wrong."""
    self.assertRaises(AssertionError, self._VerifyBoardMap, 'x86-generic-full',
                      1, {})

  def testChromeUpload(self):
    """Test uploading of prebuilts for chrome build."""
    board_map = {'amd64-generic': True, 'daisy': True,
                 'x86-alex': False, 'lumpy': False}
    self._VerifyBoardMap('x86-generic-chromium-pfq', 9, board_map,
                         public_args=['--board', 'x86-generic'])


class MasterUploadPrebuiltsStageTest(RunCommandAbstractStageTest):
  """Tests for the MasterUploadPrebuilts stage."""

  CMD = './upload_prebuilts'
  RELEASE_TAG = '1234.5.6'
  VERSION = 'R%s-%s' % (DEFAULT_CHROME_BRANCH, RELEASE_TAG)

  def setUp(self):
    self.StartPatcher(BuilderRunMock())

  def _Prepare(self, bot_id=None, **kwargs):
    super(MasterUploadPrebuiltsStageTest, self)._Prepare(bot_id, **kwargs)

    self.run.options.prebuilts = True

  def ConstructStage(self):
    return stages.MasterUploadPrebuiltsStage(self.run)

  def _RunStage(self, bot_id):
    """Run the stage under test with the given |bot_id| config.

    Args:
      bot_id: Builder config target name.
    """
    self._Prepare(bot_id)
    self.RunStage()

  def _VerifyResults(self, public_slave_boards=(), private_slave_boards=()):
    """Verify that the expected prebuilt commands were run.

    Do various assertions on the two RunCommands that were run by stage.
    There should be one private (--private) and one public (default) run.

    Args:
      public_slave_boards: List of public slave boards.
      private_slave_boards: List of private slave boards.
    """
    # TODO(mtennant): Add functionality in partial_mock to support more flexible
    # asserting.  For example here, asserting that '--sync-host' appears in
    # the command that did not include '--public'.

    # Some args are expected for any public run.
    if public_slave_boards:
      # It would be nice to confirm that --private is not in command, but note
      # that --sync-host should not appear in the --private command.
      cmd = [self.CMD, '--sync-binhost-conf', '--sync-host']
      self.assertCommandContains(cmd, expected=True)

    # Some args are expected for any private run.
    if private_slave_boards:
      cmd = [self.CMD, '--sync-binhost-conf', '--private']
      self.assertCommandContains(cmd, expected=True)

    # Assert public slave boards are mentioned in public run.
    for board in public_slave_boards:
      # This check does not actually confirm that this board was in the public
      # run rather than the private run, unfortunately.
      cmd = [self.CMD, '--slave-board', board]
      self.assertCommandContains(cmd, expected=True)

    # Assert private slave boards are mentioned in private run.
    for board in private_slave_boards:
      cmd = [self.CMD, '--slave-board', board, '--private']
      self.assertCommandContains(cmd, expected=True)

    # We expect --set-version so long as build config has manifest_version=True.
    self.assertCommandContains([self.CMD, '--set-version', self.VERSION],
                               expected=self.run.config.manifest_version)

  def testMasterPaladinUpload(self):
    self._RunStage('master-paladin')

    # Provide a sample of private/public slave boards that are expected.
    public_slave_boards = ('amd64-generic', 'x86-generic')
    private_slave_boards = ('x86-mario', 'x86-alex', 'lumpy', 'daisy_spring')

    self._VerifyResults(public_slave_boards=public_slave_boards,
                        private_slave_boards=private_slave_boards)


class UploadDevInstallerPrebuiltsStageTest(AbstractStageTest):
  """Tests for the UploadDevInstallerPrebuilts stage."""

  RELEASE_TAG = 'RT'

  def setUp(self):
    self.mox.StubOutWithMock(commands, 'UploadDevInstallerPrebuilts')

    self.StartPatcher(BuilderRunMock())

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(UploadDevInstallerPrebuiltsStageTest, self)._Prepare(bot_id, **kwargs)

    self.run.options.chrome_rev = None
    self.run.options.prebuilts = True
    self.run.config['dev_installer_prebuilts'] = True
    self.run.config['binhost_bucket'] = 'gs://testbucket'
    self.run.config['binhost_key'] = 'dontcare'
    self.run.config['binhost_base_url'] = 'https://dontcare/here'

  def ConstructStage(self):
    return stages.DevInstallerPrebuiltsStage(self.run,
                                             self._current_board)

  def testDevInstallerUpload(self):
    """Basic sanity test testing uploads of dev installer prebuilts."""
    version = 'R%s-%s' % (DEFAULT_CHROME_BRANCH, self.RELEASE_TAG)

    commands.UploadDevInstallerPrebuilts(
        binhost_bucket=self.run.config.binhost_bucket,
        binhost_key=self.run.config.binhost_key,
        binhost_base_url=self.run.config.binhost_base_url,
        buildroot=self.build_root,
        board=self._current_board,
        extra_args=mox.And(mox.IsA(list),
                           mox.In(version)))

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class PublishUprevChangesStageTest(AbstractStageTest):
  """Tests for the PublishUprevChanges stage."""

  def setUp(self):
    self.mox.StubOutWithMock(stages.PublishUprevChangesStage,
                             '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.mox.StubOutWithMock(commands, 'UprevPush')
    self.mox.StubOutWithMock(stages.PublishUprevChangesStage,
                             '_ExtractOverlays')
    stages.PublishUprevChangesStage._ExtractOverlays().AndReturn(
        [['foo'], ['bar']])

  def ConstructStage(self):
    return stages.PublishUprevChangesStage(self.run, success=True)

  def testPush(self):
    """Test values for PublishUprevChanges."""
    self._Prepare(extra_config={'build_type': constants.BUILD_FROM_SOURCE_TYPE,
                                'push_overlays': constants.PUBLIC_OVERLAYS,
                                'master': True},
                  extra_cmd_args=['--chrome_rev', constants.CHROME_REV_TOT])
    self.run.options.prebuilts = True
    stages.commands.UprevPush(self.build_root, ['bar'], False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class CPEExportStageTest(AbstractStageTest):
  """Test CPEExportStage"""

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self.StartPatcher(ArchivingMock())
    self.StartPatcher(parallel_unittest.ParallelMock())

    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult(output='')

    self.stage = None

  def ConstructStage(self):
    """Create a CPEExportStage instance for testing"""
    self.run.GetArchive().SetupArchivePath()
    return stages.CPEExportStage(self.run, self._current_board)

  def assertBoardAttrEqual(self, attr, expected_value):
    """Assert the value of a board run |attr| against |expected_value|."""
    value = self.stage.board_runattrs.GetParallel(attr)
    self.assertEqual(expected_value, value)

  def _TestPerformStage(self):
    """Run PerformStage for the stage."""
    self._Prepare()
    self.run.attrs.release_tag = BuilderRunMock.VERSION

    self.stage = self.ConstructStage()
    self.stage.PerformStage()

  def testCPEExport(self):
    """Test that CPEExport stage runs without syntax errors."""
    self._TestPerformStage()


class DebugSymbolsStageTest(AbstractStageTest):
  """Test DebugSymbolsStage"""

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self.StartPatcher(ArchivingMock())
    self.StartPatcher(parallel_unittest.ParallelMock())

    self.gen_mock = self.PatchObject(commands, 'GenerateBreakpadSymbols')
    self.upload_mock = self.PatchObject(commands, 'UploadSymbols')
    self.tar_mock = self.PatchObject(commands, 'GenerateDebugTarball')

    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult(output='')

    self.stage = None

  def ConstructStage(self):
    """Create a DebugSymbolsStage instance for testing"""
    self.run.GetArchive().SetupArchivePath()
    return stages.DebugSymbolsStage(self.run, self._current_board)

  def assertBoardAttrEqual(self, attr, expected_value):
    """Assert the value of a board run |attr| against |expected_value|."""
    value = self.stage.board_runattrs.GetParallel(attr)
    self.assertEqual(expected_value, value)

  def _TestPerformStage(self, extra_config=None):
    """Run PerformStage for the stage with the given extra config."""
    if not extra_config:
      extra_config = {
          'archive_build_debug': True,
          'vm_tests': True,
          'upload_symbols': True,
      }

    self._Prepare(extra_config=extra_config)
    self.run.attrs.release_tag = BuilderRunMock.VERSION

    self.tar_mock.side_effect = '/my/tar/ball'
    self.stage = self.ConstructStage()
    try:
      self.stage.PerformStage()
    except Exception:
      self.stage._HandleStageException(sys.exc_info())
      raise

  def testPerformStageWithSymbols(self):
    """Smoke test for an PerformStage when debugging is enabled"""
    self._TestPerformStage()

    self.assertEqual(self.gen_mock.call_count, 1)
    self.assertEqual(self.upload_mock.call_count, 1)
    self.assertEqual(self.tar_mock.call_count, 1)

    self.assertBoardAttrEqual('breakpad_symbols_generated', True)
    self.assertBoardAttrEqual('debug_tarball_generated', True)

  def testPerformStageNoSymbols(self):
    """Smoke test for an PerformStage when debugging is disabled"""
    extra_config = {
        'archive_build_debug': False,
        'vm_tests': False,
        'upload_symbols': False,
    }
    self._TestPerformStage(extra_config)

    self.assertEqual(self.gen_mock.call_count, 1)
    self.assertEqual(self.upload_mock.call_count, 0)
    self.assertEqual(self.tar_mock.call_count, 1)

    self.assertBoardAttrEqual('breakpad_symbols_generated', True)
    self.assertBoardAttrEqual('debug_tarball_generated', True)

  def testGenerateCrashStillNotifies(self):
    """Crashes in symbol generation should still notify external events."""
    class TestError(Exception):
      """Unique test exception"""

    self.gen_mock.side_effect = TestError('mew')
    self.assertRaises(TestError, self._TestPerformStage)

    self.assertEqual(self.gen_mock.call_count, 1)
    self.assertEqual(self.upload_mock.call_count, 0)
    self.assertEqual(self.tar_mock.call_count, 0)

    self.assertBoardAttrEqual('breakpad_symbols_generated', False)
    self.assertBoardAttrEqual('debug_tarball_generated', False)

  def testUploadCrashStillNotifies(self):
    """Crashes in symbol upload should still notify external events."""
    class TestError(Exception):
      """Unique test exception"""

    self.upload_mock.side_effect = TestError('mew')
    self.assertRaises(TestError, self._TestPerformStage)

    self.assertEqual(self.gen_mock.call_count, 1)
    self.assertEqual(self.upload_mock.call_count, 1)
    self.assertEqual(self.tar_mock.call_count, 1)

    self.assertBoardAttrEqual('breakpad_symbols_generated', True)
    self.assertBoardAttrEqual('debug_tarball_generated', True)


class PassStage(bs.BuilderStage):
  """PassStage always works"""


class Pass2Stage(bs.BuilderStage):
  """Pass2Stage always works"""


class FailStage(bs.BuilderStage):
  """FailStage always throws an exception"""

  FAIL_EXCEPTION = results_lib.StepFailure("Fail stage needs to fail.")

  def PerformStage(self):
    """Throw the exception to make us fail."""
    raise self.FAIL_EXCEPTION


class SkipStage(bs.BuilderStage):
  """SkipStage is skipped."""
  config_name = 'signer_tests'


class SneakyFailStage(bs.BuilderStage):
  """SneakyFailStage exits with an error."""

  def PerformStage(self):
    """Exit without reporting back."""
    os._exit(1)


class SuicideStage(bs.BuilderStage):
  """SuicideStage kills itself with kill -9."""

  def PerformStage(self):
    """Exit without reporting back."""
    os.kill(os.getpid(), signal.SIGKILL)


class SetAttrStage(bs.BuilderStage):
  """Stage that sets requested run attribute to a value."""

  DEFAULT_ATTR = 'unittest_value'
  VALUE = 'HereTakeThis'

  def __init__(self, builder_run, delay=2, attr=DEFAULT_ATTR, *args, **kwargs):
    super(SetAttrStage, self).__init__(builder_run, *args, **kwargs)
    self.delay = delay
    self.attr = attr

  def PerformStage(self):
    """Wait self.delay seconds then set requested run attribute."""
    time.sleep(self.delay)
    self._run.attrs.SetParallel(self.attr, self.VALUE)

  def QueueableException(self):
    return cbuildbot_run.ParallelAttributeError(self.attr)


class GetAttrStage(bs.BuilderStage):
  """Stage that accesses requested run attribute and confirms value."""

  DEFAULT_ATTR = 'unittest_value'

  def __init__(self, builder_run, tester=None, timeout=5, attr=DEFAULT_ATTR,
               *args, **kwargs):
    super(GetAttrStage, self).__init__(builder_run, *args, **kwargs)
    self.tester = tester
    self.timeout = timeout
    self.attr = attr

  def PerformStage(self):
    """Wait for attrs.test value to show up."""
    assert not self._run.attrs.HasParallel(self.attr)
    value = self._run.attrs.GetParallel(self.attr, self.timeout)
    if self.tester:
      self.tester(value)

  def QueueableException(self):
    return cbuildbot_run.ParallelAttributeError(self.attr)

  def TimeoutException(self):
    return cbuildbot_run.AttrTimeoutError(self.attr)


class BuildStagesResultsTest(cros_test_lib.TestCase):
  """Tests for stage results and reporting."""

  def setUp(self):
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-paladin'
    build_config = config.config[self.bot_id]
    self.build_root = '/fake_root'

    # Create a class to hold
    class Options(object):
      """Dummy class to hold option values."""

    options = Options()
    options.archive_base = 'gs://dontcare'
    options.buildroot = self.build_root
    options.debug = False
    options.prebuilts = False
    options.clobber = False
    options.nosdk = False
    options.remote_trybot = False
    options.latest_toolchain = False
    options.buildnumber = 1234
    options.chrome_rev = None
    options.branch = 'dontcare'

    self._manager = parallel.Manager()
    self._manager.__enter__()

    self.run = cbuildbot_run.BuilderRun(options, build_config, self._manager)

    results_lib.Results.Clear()

  def tearDown(self):
    # Mimic exiting with statement for self._manager.
    self._manager.__exit__(None, None, None)

  def _runStages(self):
    """Run a couple of stages so we can capture the results"""
    # Run two pass stages, and one fail stage.
    PassStage(self.run).Run()
    Pass2Stage(self.run).Run()
    self.assertRaises(
      results_lib.StepFailure,
      FailStage(self.run).Run)

  def _verifyRunResults(self, expectedResults, max_time=2.0):
    actualResults = results_lib.Results.Get()

    # Break out the asserts to be per item to make debugging easier
    self.assertEqual(len(expectedResults), len(actualResults))
    for i in xrange(len(expectedResults)):
      entry = actualResults[i]
      xname, xresult = expectedResults[i]

      if entry.result not in results_lib.Results.NON_FAILURE_TYPES:
        self.assertTrue(isinstance(entry.result, BaseException))
        if isinstance(entry.result, results_lib.StepFailure):
          self.assertEqual(str(entry.result), entry.description)

      self.assertTrue(entry.time >= 0 and entry.time < max_time)
      self.assertEqual(xname, entry.name)
      self.assertEqual(type(xresult), type(entry.result))
      self.assertEqual(repr(xresult), repr(entry.result))

  def _PassString(self):
    record = results_lib.Result('Pass', results_lib.Results.SUCCESS, 'None',
                                'Pass', '', '0')
    return results_lib.Results.SPLIT_TOKEN.join(record) + '\n'

  def testRunStages(self):
    """Run some stages and verify the captured results"""

    self.assertEqual(results_lib.Results.Get(), [])

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', results_lib.Results.SUCCESS),
        ('Pass2', results_lib.Results.SUCCESS),
        ('Fail', FailStage.FAIL_EXCEPTION),
    ]
    self._verifyRunResults(expectedResults)

  def testSuccessTest(self):
    """Run some stages and verify the captured results"""

    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS)

    self.assertTrue(results_lib.Results.BuildSucceededSoFar())

    results_lib.Results.Record('Fail', FailStage.FAIL_EXCEPTION, time=1)

    self.assertFalse(results_lib.Results.BuildSucceededSoFar())

    results_lib.Results.Record('Pass2', results_lib.Results.SUCCESS)

    self.assertFalse(results_lib.Results.BuildSucceededSoFar())

  def _TestParallelStages(self, stage_objs):
    builder = cbuildbot.SimpleBuilder(self.run)
    error = None
    with mock.patch.multiple(parallel._BackgroundTask, PRINT_INTERVAL=0.01):
      try:
        builder._RunParallelStages(stage_objs)
      except parallel.BackgroundFailure as ex:
        error = ex

    return error

  def testParallelStages(self):
    stage_objs = [stage(self.run) for stage in
                  (PassStage, SneakyFailStage, FailStage, SuicideStage,
                   Pass2Stage)]
    error = self._TestParallelStages(stage_objs)
    self.assertTrue(error)
    expectedResults = [
        ('Pass', results_lib.Results.SUCCESS),
        ('Fail', FailStage.FAIL_EXCEPTION),
        ('Pass2', results_lib.Results.SUCCESS),
        ('SneakyFail', error),
        ('Suicide', error),
    ]
    self._verifyRunResults(expectedResults)

  def testParallelStageCommunicationOK(self):
    """Test run attr communication betweeen parallel stages."""
    def assert_test(value):
      self.assertEqual(value, SetAttrStage.VALUE,
                       'Expected value %r to be passed between stages, but'
                       ' got %r.' % (SetAttrStage.VALUE, value))
    stage_objs = [
        SetAttrStage(self.run),
        GetAttrStage(self.run, assert_test, timeout=30),
        GetAttrStage(self.run, assert_test, timeout=30),
    ]
    error = self._TestParallelStages(stage_objs)
    self.assertFalse(error)
    expectedResults = [
        ('SetAttr', results_lib.Results.SUCCESS),
        ('GetAttr', results_lib.Results.SUCCESS),
        ('GetAttr', results_lib.Results.SUCCESS),
    ]
    self._verifyRunResults(expectedResults, max_time=30.0)

    # Make sure run attribute propagated up to the top, too.
    value = self.run.attrs.GetParallel('unittest_value')
    self.assertEqual(SetAttrStage.VALUE, value)

  def testParallelStageCommunicationTimeout(self):
    """Test run attr communication between parallel stages that times out."""
    def assert_test(value):
      self.assertEqual(value, SetAttrStage.VALUE,
                       'Expected value %r to be passed between stages, but'
                       ' got %r.' % (SetAttrStage.VALUE, value))
    stage_objs = [SetAttrStage(self.run, delay=11),
                  GetAttrStage(self.run, assert_test, timeout=1),
                 ]
    error = self._TestParallelStages(stage_objs)
    self.assertTrue(error)
    expectedResults = [
        ('SetAttr', results_lib.Results.SUCCESS),
        ('GetAttr', stage_objs[1].TimeoutException()),
    ]
    self._verifyRunResults(expectedResults, max_time=12.0)

  def testParallelStageCommunicationNotQueueable(self):
    """Test setting non-queueable run attr in parallel stage."""
    stage_objs = [SetAttrStage(self.run, attr='release_tag'),
                  GetAttrStage(self.run, timeout=2),
                 ]
    error = self._TestParallelStages(stage_objs)
    self.assertTrue(error)
    expectedResults = [
        ('SetAttr', stage_objs[0].QueueableException()),
        ('GetAttr', stage_objs[1].TimeoutException()),
    ]
    self._verifyRunResults(expectedResults, max_time=12.0)

  def testStagesReportSuccess(self):
    """Tests Stage reporting."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    results_lib.Results.Record('Sync', results_lib.Results.SUCCESS, time=1)
    results_lib.Results.Record('Build', results_lib.Results.SUCCESS, time=2)
    results_lib.Results.Record('Test', FailStage.FAIL_EXCEPTION, time=3)
    results_lib.Results.Record('SignerTests', results_lib.Results.SKIPPED)
    result = cros_build_lib.CommandResult(cmd=['/bin/false', '/nosuchdir'],
                                          returncode=2)
    results_lib.Results.Record(
        'Archive',
        cros_build_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            result), time=4)

    results = StringIO.StringIO()

    results_lib.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Sync (0:00:01)\n"
        "************************************************************\n"
        "** PASS Build (0:00:02)\n"
        "************************************************************\n"
        "** FAIL Test (0:00:03) with StepFailure\n"
        "************************************************************\n"
        "** FAIL Archive (0:00:04) in /bin/false\n"
        "************************************************************\n"
    )

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(min(len(actualLines), len(expectedLines))):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportError(self):
    """Tests Stage reporting with exceptions."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    results_lib.Results.Record('Sync', results_lib.Results.SUCCESS, time=1)
    results_lib.Results.Record('Build', results_lib.Results.SUCCESS, time=2)
    results_lib.Results.Record('Test', FailStage.FAIL_EXCEPTION,
                               'failException Msg\nLine 2', time=3)
    result = cros_build_lib.CommandResult(cmd=['/bin/false', '/nosuchdir'],
                                          returncode=2)
    results_lib.Results.Record(
        'Archive',
        cros_build_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            result),
        'FailRunCommand msg', time=4)

    results = StringIO.StringIO()

    results_lib.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Sync (0:00:01)\n"
        "************************************************************\n"
        "** PASS Build (0:00:02)\n"
        "************************************************************\n"
        "** FAIL Test (0:00:03) with StepFailure\n"
        "************************************************************\n"
        "** FAIL Archive (0:00:04) in /bin/false\n"
        "************************************************************\n"
        "\n"
        "Failed in stage Test:\n"
        "\n"
        "failException Msg\n"
        "Line 2\n"
        "\n"
        "Failed in stage Archive:\n"
        "\n"
        "FailRunCommand msg\n"
   )

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(min(len(actualLines), len(expectedLines))):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportReleaseTag(self):
    """Tests Release Tag entry in stages report."""

    current_version = "release_tag_string"
    archive_urls = {'board1': 'result_url1',
                    'board2': 'result_url2'}

    # Store off a known set of results and generate a report
    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS, time=1)

    results = StringIO.StringIO()

    results_lib.Results.Report(results, archive_urls, current_version)

    expectedResults = (
        "************************************************************\n"
        "** RELEASE VERSION: release_tag_string\n"
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Pass (0:00:01)\n"
        "************************************************************\n"
        "** BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n"
        "**  board1: result_url1\n"
        "@@@STEP_LINK@Artifacts[board1]@result_url1@@@\n"
        "**  board2: result_url2\n"
        "@@@STEP_LINK@Artifacts[board2]@result_url2@@@\n"
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
    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS)
    results_lib.Results.Record('Fail', FailStage.FAIL_EXCEPTION)
    results_lib.Results.Record('Pass2', results_lib.Results.SUCCESS)

    saveFile = StringIO.StringIO()
    results_lib.Results.SaveCompletedStages(saveFile)
    self.assertEqual(saveFile.getvalue(), self._PassString())

  def testRestoreCompletedStages(self):
    """Tests that we can read in completed stages."""

    results_lib.Results.RestoreCompletedStages(
        StringIO.StringIO(self._PassString()))

    previous = results_lib.Results.GetPrevious()
    self.assertEqual(previous.keys(), ['Pass'])

  def testRunAfterRestore(self):
    """Tests that we skip previously completed stages."""

    # Fake results_lib.Results.RestoreCompletedStages
    results_lib.Results.RestoreCompletedStages(
        StringIO.StringIO(self._PassString()))

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', results_lib.Results.SUCCESS),
        ('Pass2', results_lib.Results.SUCCESS),
        ('Fail', FailStage.FAIL_EXCEPTION),
    ]
    self._verifyRunResults(expectedResults)

  def testFailedButForgiven(self):
    """Tests that warnings are flagged as such."""
    results_lib.Results.Record('Warn', results_lib.Results.FORGIVEN, time=1)
    results = StringIO.StringIO()
    results_lib.Results.Report(results)
    self.assertTrue('@@@STEP_WARNINGS@@@' in results.getvalue())


class ReportStageTest(AbstractStageTest):
  """Test the Report stage."""

  RELEASE_TAG = ''

  def setUp(self):
    for cmd in ((osutils, 'ReadFile'), (osutils, 'WriteFile'),
                (commands, 'UploadArchivedFile'),
                (alerts, 'SendEmail')):
      self.StartPatcher(mock.patch.object(*cmd, autospec=True))

    self.StartPatcher(BuilderRunMock())
    self.cq = CLStatusMock()
    self.StartPatcher(self.cq)
    self.sync_stage = None

    self._Prepare()

  def _SetupUpdateStreakCounter(self, counter_value=-1):
    self.PatchObject(stages.ReportStage, '_UpdateStreakCounter',
                     autospec=True, return_value=counter_value)

  def _SetupCommitQueueSyncPool(self):
    self.sync_stage = stages.CommitQueueSyncStage(self.run)
    pool = validation_pool.ValidationPool(constants.BOTH_OVERLAYS,
        self.build_root, build_number=3, builder_name=self.bot_id,
        is_master=True, dryrun=True)
    pool.changes = [MockPatch()]
    self.sync_stage.pool = pool

  def ConstructStage(self):
    return stages.ReportStage(self.run, self.sync_stage, None)

  def testCheckResults(self):
    """Basic sanity check for results stage functionality"""
    self._SetupUpdateStreakCounter()
    self.PatchObject(stages.ReportStage, '_UploadArchiveIndex',
                     return_value={'any': 'dict'})
    self.RunStage()
    filenames = (
        'LATEST-%s' % self.TARGET_MANIFEST_BRANCH,
        'LATEST-%s' % BuilderRunMock.VERSION,
    )
    calls = [mock.call(mock.ANY, mock.ANY, 'metadata.json', False,
                       update_list=True, acl=mock.ANY)]
    calls += [mock.call(mock.ANY, mock.ANY, filename, False,
                        acl=mock.ANY) for filename in filenames]
    # pylint: disable=E1101
    self.assertEquals(calls, commands.UploadArchivedFile.call_args_list)

  def testCommitQueueResults(self):
    """Check that commit queue patches get serialized"""
    self._SetupUpdateStreakCounter()
    self._SetupCommitQueueSyncPool()
    self.RunStage()

  def testAlertEmail(self):
    """Send out alerts when streak counter reaches the threshold."""
    self._Prepare(extra_config={'health_threshold': 3,
                                'health_alert_recipients': ['fake_recipient']})
    self._SetupUpdateStreakCounter(counter_value=-3)
    self._SetupCommitQueueSyncPool()
    self.RunStage()
    # pylint: disable=E1101
    self.assertGreater(alerts.SendEmail.call_count, 0,
                       'CQ health alerts emails were not sent.')

  def testAlertEmailOnFailingStreak(self):
    """Continue sending out alerts when streak counter exceeds the threshold."""
    self._Prepare(extra_config={'health_threshold': 3,
                                'health_alert_recipients': ['fake_recipient']})
    self._SetupUpdateStreakCounter(counter_value=-5)
    self._SetupCommitQueueSyncPool()
    self.RunStage()
    # pylint: disable=E1101
    self.assertGreater(alerts.SendEmail.call_count, 0,
                       'CQ health alerts emails were not sent.')


class BoardSpecificBuilderStageTest(cros_test_lib.TestCase):
  """Tests option/config settings on board-specific stages."""

  def testCheckOptions(self):
    """Makes sure options/config settings are setup correctly."""

    parser = cbuildbot._CreateParser()
    (options, _) = parser.parse_args([])

    for attr in dir(stages):
      obj = eval('stages.' + attr)
      if not hasattr(obj, '__base__'):
        continue
      if not obj.__base__ is stages.BoardSpecificBuilderStage:
        continue
      if obj.option_name:
        self.assertTrue(getattr(options, obj.option_name))
      if obj.config_name:
        if not obj.config_name in config._settings:
          self.fail(('cbuildbot_stages.%s.config_name "%s" is missing from '
                     'cbuildbot_config._settings') % (attr, obj.config_name))


class MockPatch(mock.MagicMock):
  """MagicMock for a GerritPatch-like object."""

  gerrit_number = '1234'
  patch_number = '1'
  project = 'chromiumos/chromite'
  status = 'NEW'
  internal = False
  current_patch_set = {
    'number': patch_number,
    'draft': False,
  }
  patch_dict = {
    'currentPatchSet': current_patch_set,
  }

  def HasApproval(self, field, value):
    """Pretends the patch is good.

    Pretend the patch has all of the values listed in
    constants.DEFAULT_CQ_READY_FIELDS, but not any other fields.
    """
    return constants.DEFAULT_CQ_READY_FIELDS.get(field, 0) == value


class BaseCQTest(StageTest):
  """Helper class for testing the CommitQueueSync stage"""
  PALADIN_BOT_ID = None
  MANIFEST_CONTENTS = '<manifest/>'

  def setUp(self):
    """Setup patchers for specified bot id."""
    # Mock out methods as needed.
    self.PatchObject(lkgm_manager, 'GenerateBlameList')
    self.PatchObject(repository.RepoRepository, 'ExportManifest',
                     return_value=MANIFEST_CONTENTS, autospec=True)
    self.StartPatcher(git_unittest.ManifestMock())
    self.StartPatcher(git_unittest.ManifestCheckoutMock())
    version_file = os.path.join(self.build_root, constants.VERSION_FILE)
    manifest_version_unittest.VersionInfoTest.WriteFakeVersionFile(version_file)
    rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc_mock.SetDefaultCmdResult()

    # Block the CQ from contacting GoB.
    self.PatchObject(gerrit.GerritHelper, 'RemoveCommitReady')
    self.PatchObject(gerrit.GerritHelper, 'SubmitChange')
    self.PatchObject(validation_pool.PaladinMessage, 'Send')

    # If a test is still contacting GoB, something is busted.
    self.PatchObject(gob_util, 'CreateHttpConn',
                     side_effect=AssertionError('Test should not contact GoB'))

    # Create a fake repo / manifest on disk that is used by subclasses.
    for subdir in ('repo', 'manifests'):
      osutils.SafeMakedirs(os.path.join(self.build_root, '.repo', subdir))
    self.manifest_path = os.path.join(self.build_root, '.repo', 'manifest.xml')
    osutils.WriteFile(self.manifest_path, MANIFEST_CONTENTS)
    self.PatchObject(validation_pool.ValidationPool, 'ReloadChanges',
                     side_effect=lambda x: x)

    self.sync_stage = None
    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(BaseCQTest, self)._Prepare(bot_id, **kwargs)

    self.sync_stage = stages.CommitQueueSyncStage(self.run)

  def PerformSync(self, remote='cros', committed=False, tree_open=True,
                  tree_throttled=False, tracking_branch='master',
                  num_patches=1, runs=0):
    """Helper to perform a basic sync for master commit queue."""
    p = MockPatch(remote=remote, tracking_branch=tracking_branch)
    my_patches = [p] * num_patches
    self.PatchObject(gerrit.GerritHelper, 'IsChangeCommitted',
                     return_value=committed, autospec=True)
    self.PatchObject(gerrit.GerritHelper, 'Query',
                     return_value=my_patches, autospec=True)
    if tree_throttled:
      self.PatchObject(timeout_util, 'WaitForTreeStatus',
                       return_value=constants.TREE_THROTTLED, autospec=True)
    elif tree_open:
      self.PatchObject(timeout_util, 'WaitForTreeStatus',
                       return_value=constants.TREE_OPEN, autospec=True)
    else:
      self.PatchObject(timeout_util, 'WaitForTreeStatus',
                       side_effect=timeout_util.TimeoutError())

    exit_it = itertools.chain([False] * runs, itertools.repeat(True))
    self.PatchObject(validation_pool.ValidationPool, 'ShouldExitEarly',
                     side_effect=exit_it)
    self.sync_stage.PerformStage()

  def ReloadPool(self):
    """Save the pool to disk and reload it."""
    with tempfile.NamedTemporaryFile() as f:
      cPickle.dump(self.sync_stage.pool, f)
      f.flush()
      self.run.options.validation_pool = f.name
      self.sync_stage = stages.CommitQueueSyncStage(self.run)
      self.sync_stage.HandleSkip()


class SlaveCQSyncTest(BaseCQTest):
  """Tests the CommitQueueSync stage for the paladin slaves."""
  BOT_ID = 'x86-alex-paladin'

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    self.PatchObject(lkgm_manager.LKGMManager, 'GetLatestCandidate',
                     return_value=self.manifest_path, autospec=True)
    self.sync_stage.PerformStage()
    self.ReloadPool()


class MasterCQSyncTest(BaseCQTest):
  """Tests the CommitQueueSync stage for the paladin masters.

  Tests in this class should apply both to the paladin masters and to the
  Pre-CQ Launcher.
  """
  BOT_ID = 'master-paladin'

  def setUp(self):
    """Setup patchers for specified bot id."""
    self.AutoPatch([[validation_pool.ValidationPool, 'ApplyPoolIntoRepo']])
    self.PatchObject(lkgm_manager.LKGMManager, 'CreateNewCandidate',
                     return_value=self.manifest_path, autospec=True)
    self.PatchObject(lkgm_manager.LKGMManager, 'CreateFromManifest',
                     return_value=self.manifest_path, autospec=True)

  def testCommitNonManifestChange(self, **kwargs):
    """Test the commit of a non-manifest change."""
    # Setting tracking_branch=foo makes this a non-manifest change.
    kwargs.setdefault('committed', True)
    self.PerformSync(tracking_branch='foo', **kwargs)

  def testFailedCommitOfNonManifestChange(self):
    """Test that the commit of a non-manifest change fails."""
    self.testCommitNonManifestChange(committed=False)

  def testCommitManifestChange(self, **kwargs):
    """Test committing a change to a project that's part of the manifest."""
    self.PatchObject(validation_pool.ValidationPool, '_FilterNonCrosProjects',
                     side_effect=lambda x, _: (x, []))
    self.PerformSync(**kwargs)

  def testDefaultSync(self):
    """Test basic ability to sync with standard options."""
    self.PerformSync()


class ExtendedMasterCQSyncTest(MasterCQSyncTest):
  """Additional tests for the CommitQueueSync stage.

  These only apply to the paladin master and not to any other stages.
  """

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    # Use zero patches because MockPatches can't be pickled.
    self.PerformSync(num_patches=0, runs=0)
    self.ReloadPool()

  def testTreeClosureBlocksCommit(self):
    """Test that tree closures block commits."""
    self.assertRaises(SystemExit, self.testCommitNonManifestChange,
                      tree_open=False)

  def testTreeThrottleUsesAlternateGerritQuery(self):
    """Test that if the tree is throttled, we use an alternate gerrit query."""
    self.PerformSync(tree_throttled=True)
    gerrit.GerritHelper.Query.assert_called_with(
        mock.ANY, constants.THROTTLED_CQ_READY_QUERY,
        sort='lastUpdated')


class CLStatusMock(partial_mock.PartialMock):
  """Partial mock for CLStatus methods in ValidationPool."""

  TARGET = 'chromite.buildbot.validation_pool.ValidationPool'
  ATTRS = ('GetCLStatus', 'GetCLStatusCount', 'UpdateCLStatus',)

  def __init__(self):
    partial_mock.PartialMock.__init__(self)
    self.calls = {}
    self.status = {}
    self.status_count = {}

  def GetCLStatus(self, _bot, change):
    return self.status.get(change)

  def GetCLStatusCount(self, _bot, change, count, latest_patchset_only=True):
    # pylint: disable=W0613
    return self.status_count.get(change, 0)

  def UpdateCLStatus(self, _bot, change, status, dry_run):
    # pylint: disable=W0613
    self.calls[status] = self.calls.get(status, 0) + 1
    self.status[change] = status
    self.status_count[change] = self.status_count.get(change, 0) + 1


class PreCQLauncherStageTest(MasterCQSyncTest):
  """Tests for the PreCQLauncherStage."""
  BOT_ID = 'pre-cq-launcher'
  STATUS_LAUNCHING = validation_pool.ValidationPool.STATUS_LAUNCHING
  STATUS_WAITING = validation_pool.ValidationPool.STATUS_WAITING
  STATUS_FAILED = validation_pool.ValidationPool.STATUS_FAILED

  def setUp(self):
    self.PatchObject(time, 'sleep', autospec=True)
    self.pre_cq = CLStatusMock()
    self.StartPatcher(self.pre_cq)

  def _Prepare(self, bot_id=None, **kwargs):
    super(PreCQLauncherStageTest, self)._Prepare(bot_id, **kwargs)

    self.sync_stage = stages.PreCQLauncherStage(self.run)

  def testTreeClosureIsOK(self):
    """Test that tree closures block commits."""
    self.testCommitNonManifestChange(tree_open=False)

  def testLaunchTrybot(self):
    """Test launching a trybot."""
    self.testCommitManifestChange()
    self.assertEqual(self.pre_cq.status.values(), [self.STATUS_LAUNCHING])
    self.assertEqual(self.pre_cq.calls.keys(), [self.STATUS_LAUNCHING])

  def runTrybotTest(self, launching, waiting, failed, runs):
    self.testCommitManifestChange(runs=runs)
    self.assertEqual(self.pre_cq.calls.get(self.STATUS_LAUNCHING, 0), launching)
    self.assertEqual(self.pre_cq.calls.get(self.STATUS_WAITING, 0), waiting)
    self.assertEqual(self.pre_cq.calls.get(self.STATUS_FAILED, 0), failed)

  def testLaunchTrybotTimesOutOnce(self):
    """Test what happens when a trybot launch times out."""
    it = itertools.chain([True], itertools.repeat(False))
    self.PatchObject(stages.PreCQLauncherStage, '_HasLaunchTimedOut',
                     side_effect=it)
    self.runTrybotTest(launching=2, waiting=1, failed=0, runs=3)

  def testLaunchTrybotTimesOutTwice(self):
    """Test what happens when a trybot launch times out."""
    self.PatchObject(stages.PreCQLauncherStage, '_HasLaunchTimedOut',
                     return_value=True)
    self.runTrybotTest(launching=2, waiting=1, failed=1, runs=3)


class ChromeSDKStageTest(AbstractStageTest, cros_test_lib.LoggingTestCase):
  """Verify stage that creates the chrome-sdk and builds chrome with it."""
  BOT_ID = 'link-paladin'
  RELEASE_TAG = ''

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self.StartPatcher(parallel_unittest.ParallelMock())

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(ChromeSDKStageTest, self)._Prepare(bot_id, **kwargs)

    self.run.options.chrome_root = '/tmp/non-existent'

  def ConstructStage(self):
    self.run.GetArchive().SetupArchivePath()
    return stages.ChromeSDKStage(self.run, self._current_board)

  def testIt(self):
    """A simple run-through test."""
    rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc_mock.SetDefaultCmdResult()
    self.PatchObject(stages.ChromeSDKStage, '_ArchiveChromeEbuildEnv',
                     autospec=True)
    self.PatchObject(stages.ChromeSDKStage, '_VerifyChromeDeployed',
                     autospec=True)
    self.PatchObject(stages.ChromeSDKStage, '_VerifySDKEnvironment',
                     autospec=True)
    self.RunStage()

  def testChromeEnvironment(self):
    """Test that the Chrome environment is built."""
    # Create the chrome environment compressed file.
    stage = self.ConstructStage()
    chrome_env_dir = os.path.join(
        stage._pkg_dir, constants.CHROME_CP + '-25.3643.0_rc1')
    env_file = os.path.join(chrome_env_dir, 'environment')
    osutils.Touch(env_file, makedirs=True)

    cros_build_lib.RunCommand(['bzip2', env_file])

    # Run the code.
    stage._ArchiveChromeEbuildEnv()

    env_tar_base = stage._upload_queue.get()[0]
    env_tar = os.path.join(stage.archive_path, env_tar_base)
    self.assertTrue(os.path.exists(env_tar))
    cros_test_lib.VerifyTarball(env_tar, ['./', 'environment'])


class BranchUtilStageTest(AbstractStageTest, cros_test_lib.LoggingTestCase):
  """Tests for branch creation/deletion."""

  BOT_ID = constants.BRANCH_UTIL_CONFIG
  DEFAULT_VERSION = '111.0.0'
  RELEASE_BRANCH_NAME = 'release-test-branch'

  def _CreateVersionFile(self, version=None):
    if version is None:
      version = self.DEFAULT_VERSION
    version_file = os.path.join(self.build_root, constants.VERSION_FILE)
    manifest_version_unittest.VersionInfoTest.WriteFakeVersionFile(
        version_file, version=version)

  def setUp(self):
    """Setup patchers for specified bot id."""
    # Mock out methods as needed.
    self.StartPatcher(parallel_unittest.ParallelMock())
    self.StartPatcher(git_unittest.ManifestCheckoutMock())
    self._CreateVersionFile()
    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()

    # We have a versioned manifest (generated by ManifestVersionSyncStage) and
    # the regular, user-maintained manifests.
    manifests = {
        '.repo/manifest.xml': VERSIONED_MANIFEST_CONTENTS,
        'manifest/full.xml': MANIFEST_CONTENTS,
        'manifest-internal/full.xml': MANIFEST_CONTENTS,
    }
    for m_path, m_content in manifests.iteritems():
      full_path = os.path.join(self.build_root, m_path)
      osutils.SafeMakedirs(os.path.dirname(full_path))
      osutils.WriteFile(full_path, m_content)

    self.norm_name = git.NormalizeRef(self.RELEASE_BRANCH_NAME)

  def _Prepare(self, bot_id=None, **kwargs):
    if 'cmd_args' not in kwargs:
      # Fill in cmd_args so we do not use the default, which specifies
      # --branch.  That is incompatible with some branch-util flows.
      kwargs['cmd_args'] = ['-r', self.build_root]
    super(BranchUtilStageTest, self)._Prepare(bot_id, **kwargs)

  def ConstructStage(self):
    return stages.BranchUtilStage(self.run)

  def _VerifyPush(self, new_branch, rename_from=None, delete=False):
    """Verify that |new_branch| has been created.

    Args:
      new_branch: The new remote branch to create (or delete).
      rename_from: If set, |rename_from| is being renamed to |new_branch|.
      delete: If set, |new_branch| is being deleted.
    """
    # Pushes all operate on remote branch refs.
    new_branch = git.NormalizeRef(new_branch)

    # Calculate source and destination revisions.
    suffixes = ['', '-new-special-branch', '-old-special-branch']
    if delete:
      src_revs = [''] * len(suffixes)
    elif rename_from is not None:
      rename_from = git.NormalizeRef(rename_from)
      rename_from_tracking = git.NormalizeRemoteRef('cros', rename_from)
      src_revs = [
          '%s%s' % (rename_from_tracking, suffix) for suffix in suffixes
      ]
    else:
      src_revs = [CHROMITE_REVISION, SPECIAL_REVISION1, SPECIAL_REVISION2]
    dest_revs = ['%s%s' % (new_branch, suffix) for suffix in suffixes]

    # Verify pushes happened correctly.
    for src_rev, dest_rev in zip(src_revs, dest_revs):
      cmd = ['push', '%s:%s' % (src_rev, dest_rev)]
      self.rc_mock.assertCommandContains(cmd)
      if rename_from is not None:
        cmd = ['push', ':%s' % (rename_from,)]
        self.rc_mock.assertCommandContains(cmd)

  def testRelease(self):
    """Run-through of branch creation."""
    self._Prepare(extra_cmd_args=['--branch-name', self.RELEASE_BRANCH_NAME,
                                  '--version', self.DEFAULT_VERSION])
    # Simulate branch not existing.
    self.rc_mock.AddCmdResult(
        partial_mock.ListRegex('git show-ref .*%s' % self.RELEASE_BRANCH_NAME),
        returncode=1)

    before = manifest_version.VersionInfo.from_repo(self.build_root)
    self.RunStage()
    after = manifest_version.VersionInfo.from_repo(self.build_root)
    # Verify Chrome version was bumped.
    self.assertEquals(int(after.chrome_branch) - int(before.chrome_branch), 1)
    self.assertEquals(int(after.build_number) - int(before.build_number), 1)

    # Verify that manifests were branched properly.
    branch_names = {
      'chromite': self.norm_name,
      'src/special-new': self.norm_name + '-new-special-branch',
      'src/special-old': self.norm_name + '-old-special-branch',
    }
    for m in ['manifest/full.xml', 'manifest-internal/full.xml']:
      manifest = git.Manifest(os.path.join(self.build_root, m))
      for project_data in manifest.checkouts_by_path.itervalues():
        branch_name = branch_names[project_data['path']]
        msg = (
            'Branch name for %s should be %r, but got %r' %
                (project_data['path'], branch_name, project_data['revision'])
        )
        self.assertEquals(project_data['revision'], branch_name, msg)

    self._VerifyPush(self.norm_name)

  def testNonRelease(self):
    """Non-release branch creation."""
    self._Prepare(extra_cmd_args=['--branch-name', 'refs/heads/test-branch',
                                  '--version', self.DEFAULT_VERSION])
    # Simulate branch not existing.
    self.rc_mock.AddCmdResult(
        partial_mock.ListRegex('git show-ref .*test-branch'),
        returncode=1)

    before = manifest_version.VersionInfo.from_repo(self.build_root)
    # Disable the new branch increment so that
    # IncrementVersionOnDiskForSourceBranch detects we need to bump the version.
    self.PatchObject(stages.BranchUtilStage,
                     '_IncrementVersionOnDiskForNewBranch', autospec=True)
    self.RunStage()
    after = manifest_version.VersionInfo.from_repo(self.build_root)
    # Verify only branch number is bumped.
    self.assertEquals(after.chrome_branch, before.chrome_branch)
    self.assertEquals(int(after.build_number) - int(before.build_number), 1)
    self._VerifyPush(self.run.options.branch_name)

  def testDeletion(self):
    """Branch deletion."""
    self._Prepare(extra_cmd_args=['--branch-name', self.RELEASE_BRANCH_NAME,
                                  '--delete-branch'])
    self.rc_mock.AddCmdResult(
        partial_mock.ListRegex('git show-ref .*release-test-branch.*'),
        output='SomeSHA1Value'
    )
    self.RunStage()
    self._VerifyPush(self.norm_name, delete=True)

  def testRename(self):
    """Branch rename."""
    self._Prepare(extra_cmd_args=['--branch-name', self.RELEASE_BRANCH_NAME,
                                  '--rename-to', 'refs/heads/release-rename'])
    # Simulate source branch existing and destination branch not existing.
    self.rc_mock.AddCmdResult(
        partial_mock.ListRegex('git show-ref .*%s' % self.RELEASE_BRANCH_NAME),
        output='SomeSHA1Value')
    self.rc_mock.AddCmdResult(
        partial_mock.ListRegex('git show-ref .*release-rename'),
        returncode=1)
    self.RunStage()
    self._VerifyPush(self.run.options.rename_to, rename_from=self.norm_name)

  def testDryRun(self):
    """Verify all pushes are done with --dryrun when --debug is set."""
    def VerifyDryRun(cmd, *_args, **_kwargs):
      self.assertTrue('--dry-run' in cmd)

    # Simulate branch not existing.
    self.rc_mock.AddCmdResult(
        partial_mock.ListRegex('git show-ref .*%s' % self.RELEASE_BRANCH_NAME),
        returncode=1)

    self._Prepare(extra_cmd_args=['--branch-name', self.RELEASE_BRANCH_NAME,
                                  '--debug',
                                  '--version', self.DEFAULT_VERSION])
    self.rc_mock.AddCmdResult(partial_mock.In('push'),
                              side_effect=VerifyDryRun)
    self.RunStage()
    self.rc_mock.assertCommandContains(['push', '--dry-run'])

  def _DetermineIncrForVersion(self, version):
    version_info = manifest_version.VersionInfo(version)
    stage_cls = stages.BranchUtilStage
    return stage_cls.DetermineBranchIncrParams(version_info)

  def testDetermineIncrBranch(self):
    """Verify branch increment detection."""
    incr_type, _ = self._DetermineIncrForVersion(self.DEFAULT_VERSION)
    self.assertEquals(incr_type, 'branch')

  def testDetermineIncrPatch(self):
    """Verify patch increment detection."""
    incr_type, _ = self._DetermineIncrForVersion('111.1.0')
    self.assertEquals(incr_type, 'patch')

  def testDetermineBranchIncrError(self):
    """Detect unbranchable version."""
    self.assertRaises(stages.BranchError, self._DetermineIncrForVersion,
                      '111.1.1')

  def _SimulateIncrementFailure(self):
    """Simulates a git push failure during source branch increment."""
    self._Prepare(extra_cmd_args=['--branch-name', self.RELEASE_BRANCH_NAME,
                                  '--version', self.DEFAULT_VERSION])
    overlay_dir = os.path.join(
        self.build_root, constants.CHROMIUMOS_OVERLAY_DIR)
    self.rc_mock.AddCmdResult(partial_mock.In('push'), returncode=128)
    stage = self.ConstructStage()
    args = (overlay_dir, 'gerrit', 'refs/heads/master')
    stage._IncrementVersionOnDiskForSourceBranch(*args)

  def testSourceIncrementWarning(self):
    """Test the warning case for incrementing failure."""
    # Since all git commands are mocked out, the _FetchAndCheckoutTo function
    # does nothing, and leaves the chromeos_version.sh file in the bumped state,
    # so it looks like TOT version was indeed bumped by another bot.
    with cros_test_lib.LoggingCapturer() as logger:
      self._SimulateIncrementFailure()
      self.AssertLogsContain(logger, 'bumped by another')

  def testSourceIncrementFailure(self):
    """Test the failure case for incrementing failure."""
    def FetchAndCheckoutTo(*_args, **_kwargs):
      self._CreateVersionFile()

    # Simulate a git checkout of TOT.
    self.PatchObject(stages.BranchUtilStage, '_FetchAndCheckoutTo',
                     side_effect=FetchAndCheckoutTo, autospec=True)
    self.assertRaises(cros_build_lib.RunCommandError,
                      self._SimulateIncrementFailure)


if __name__ == '__main__':
  cros_test_lib.main()
