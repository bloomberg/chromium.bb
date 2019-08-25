# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for generic stages."""

from __future__ import print_function

import contextlib
import copy
import os
import sys
import unittest

import mock

from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cidb
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import failures_lib
from chromite.lib import failure_message_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import partial_mock
from chromite.lib import portage_util
from chromite.lib import results_lib
from chromite.lib.buildstore import FakeBuildStore, BuildIdentifier
from chromite.scripts import cbuildbot


DEFAULT_BUILD_NUMBER = 1234321
DEFAULT_BUILD_ID = 31337
DEFAULT_BUILD_STAGE_ID = 313377

# pylint: disable=protected-access


# The inheritance order ensures the patchers are stopped before
# cleaning up the temporary directories.
class StageTestCase(cros_test_lib.MockOutputTestCase,
                    cros_test_lib.TempDirTestCase):
  """Test running a single stage in isolation."""

  TARGET_MANIFEST_BRANCH = 'ooga_booga'
  BUILDROOT = 'buildroot'

  # Subclass should override this to default to a different build config
  # for its tests.
  BOT_ID = 'amd64-generic-paladin'

  # Subclasses can override this.  If non-None, value is inserted into
  # self._run.attrs.release_tag.
  RELEASE_TAG = None

  def setUp(self):
    # Prepare a fake build root in self.tempdir, save at self.build_root.
    self.build_root = os.path.join(self.tempdir, self.BUILDROOT)
    osutils.SafeMakedirs(os.path.join(self.build_root, '.repo'))

    self._manager = parallel.Manager()
    self._manager.__enter__()

    # These are here to make pylint happy.  Values filled in by _Prepare.
    self._bot_id = None
    self._current_board = None
    self._boards = None
    self._run = None
    self._model = None
    self.buildstore = FakeBuildStore()

  def CreateMockOverlay(self, overlay, build_root=None):
    """Helper for creating an overlay in the fake buildroot.

    Args:
      overlay: The overlay name to create. Usually the board name.
      build_root: The buildroot in which to create the mock overlay.
    """
    if not build_root:
      build_root = self.build_root

    layout_path = os.path.join(build_root, 'src', 'overlays',
                               'overlay-%s' % overlay,
                               'metadata', 'layout.conf')

    layout_content = 'repo-name = %s\n' % overlay

    # Don't use osutils.WriteFile, some tests mock it out.
    osutils.SafeMakedirs(os.path.dirname(layout_path))
    with open(layout_path, 'w') as f:
      f.writelines(cros_build_lib.iflatten_instance(layout_content))

  def _Prepare(self, bot_id=None, extra_config=None, cmd_args=None,
               extra_cmd_args=None, build_id=DEFAULT_BUILD_ID,
               master_build_id=None, buildbucket_id=None,
               site_config=None):
    """Prepare a BuilderRun at self._run for this test.

    This method must allow being called more than once.  Subclasses can
    override this method, but those subclass methods should also call this one.

    The idea is that all test preparation that falls out from the choice of
    build config and cbuildbot options should go in _Prepare.

    This will populate the following attributes on self:
      run: A BuilderRun object.
      bot_id: The bot id (name) that was used from the site_config.
      self._boards: Same as self._run.config.boards.  TODO(mtennant): remove.
      self._current_board: First board in list, if there is one.

    Args:
      bot_id: Name of build config to use, defaults to self.BOT_ID.
      extra_config: Dict used to add to the build config for the given
        bot_id.  Example: {'push_image': True}.
      cmd_args: List to override the default cbuildbot command args, including
        the bot_id.
      extra_cmd_args: List to add to default cbuildbot command args.  This
        is a good way to adjust an options value for your test.
        Example: ['branch-name', 'some-branch-name'] will effectively cause
        self._run.options.branch_name to be set to 'some-branch-name'.
      build_id: mock build id
      buildbucket_id: mock buildbucket_id
      master_build_id: mock build id of master build.
      site_config: SiteConfig to use (or MockSiteConfig)
    """
    assert not bot_id or not cmd_args

    # Use cbuildbot parser to create options object and populate default values.
    if not cmd_args:
      # Fill in default command args.
      cmd_args = [
          '-r', self.build_root, '--buildbot', '--noprebuilts',
          '--buildnumber', str(DEFAULT_BUILD_NUMBER),
          '--branch', self.TARGET_MANIFEST_BRANCH,
          bot_id or self.BOT_ID
      ]
    if extra_cmd_args:
      cmd_args += extra_cmd_args

    parser = cbuildbot._CreateParser()
    options = cbuildbot.ParseCommandLine(parser, cmd_args)
    self._bot_id = options.build_config_name

    if site_config is None:
      site_config = config_lib.GetConfig()

    # Populate build_config corresponding to self._bot_id.
    build_config = copy.deepcopy(site_config[self._bot_id])
    build_config['manifest_repo_url'] = 'fake_url'
    if extra_config:
      build_config.update(extra_config)
    options.managed_chrome = build_config['sync_chrome']

    self._boards = build_config['boards']
    self._current_board = self._boards[0] if self._boards else None
    self._model = self._current_board

    # Some preliminary sanity checks.
    self.assertEqual(options.buildroot, self.build_root)

    # Construct a real BuilderRun using options and build_config.
    self._run = cbuildbot_run.BuilderRun(
        options, site_config, build_config, self._manager)

    if build_id is not None:
      self._run.attrs.metadata.UpdateWithDict({'build_id': build_id})

    if buildbucket_id is not None:
      self._run.options.buildbucket_id = buildbucket_id

    if master_build_id is not None:
      self._run.options.master_build_id = master_build_id

    if self.RELEASE_TAG is not None:
      self._run.attrs.release_tag = self.RELEASE_TAG

    portage_util._OVERLAY_LIST_CMD = '/bin/true'

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
    hw_tests = self._run.config['hw_tests']
    if not hw_tests:
      # TODO(milleral): Add HWTests back to lumpy-chrome-perf.
      raise unittest.SkipTest('Missing HWTest for %s' % (self._bot_id,))

    return hw_tests[0]

  def assertRaisesStringifyable(self, exception, functor, *args, **kwargs):
    """assertRaises replacement that also verifies exception is Stringifyable.

    This helper is intended to be used anywhere assertRaises can be used, but
    will also verify the exception raised can pass through
    BuilderStage._StringifyException.

    Args:
      exception: See unittest.TestCase.assertRaises.
      functor: See unittest.TestCase.assertRaises.
      args: See unittest.TestCase.assertRaises.
      kwargs: See unittest.TestCase.assertRaises.

    Raises:
      Unittest failures if the expected exception is not raised, or
      _StringifyException exceptions if that process fails.
    """
    try:
      functor(*args, **kwargs)

      # We didn't get the exception, fail the test.
      self.fail('%s was not raised.' % exception)

    except exception:
      # Ensure that this exception can be converted properly.
      # Verifies fix for crbug.com/418358 and related.
      generic_stages.BuilderStage._StringifyException(sys.exc_info())

    except Exception as e:
      # We didn't get the exception, fail the test.
      self.fail('%s raised instead of %s' % (e, exception))


class AbstractStageTestCase(StageTestCase):
  """Base class for tests that test a particular build stage.

  Abstract base class that sets up the build config and options with some
  default values for testing BuilderStage and its derivatives.
  """
  def setUp(self):
    # Value which will be populated as the stage runs, so that tests can examine
    # it's state afterwards.
    self.stage = None

  def ConstructStage(self, **kwargs):
    """Returns an instance of the stage to be tested.

    Note: Must be implemented in subclasses.
    """
    raise NotImplementedError(self, 'ConstructStage: Implement in your test')

  def RunStage(self, **kwargs):
    """Creates and runs an instance of the stage to be tested.

    Note: Requires ConstructStage() to be implemented.

    Raises:
      NotImplementedError: ConstructStage() was not implemented.
    """

    # Stage construction is usually done as late as possible because the tests
    # set up the build configuration and options used in constructing the stage.
    results_lib.Results.Clear()
    self.stage = self.ConstructStage(**kwargs)
    self.stage.Run()
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
      stack.Add(lambda ret=arg: ret)
    yield


class BuilderStageTest(AbstractStageTestCase):
  """Tests for BuilderStage class."""

  def setUp(self):
    self._Prepare()
    self.mock_cidb = mock.MagicMock()
    self.buildstore = FakeBuildStore(self.mock_cidb)
    cidb.CIDBConnectionFactory.SetupMockCidb(self.mock_cidb)
    # Many tests modify the global results_lib.Results instance.
    results_lib.Results.Clear()

  def tearDown(self):
    cidb.CIDBConnectionFactory.ClearMock()

  def _ConstructStageWithExpectations(self, stage_class):
    """Construct an instance of the stage, verifying expectations from init.

    Args:
      stage_class: The class to instantitate.

    Returns:
      The instantiated class instance.
    """
    if stage_class is None:
      stage_class = generic_stages.BuilderStage

    self.PatchObject(
        FakeBuildStore, 'InsertBuildStage',
        return_value=constants.MOCK_STAGE_ID)
    self.PatchObject(
        FakeBuildStore, 'StartBuildStage',
        return_value=constants.MOCK_STAGE_ID)
    self.PatchObject(
        FakeBuildStore, 'WaitBuildStage',
        return_value=constants.MOCK_STAGE_ID)
    self.PatchObject(
        FakeBuildStore, 'FinishBuildStage',
        return_value=constants.MOCK_STAGE_ID)
    stage = stage_class(self._run, self.buildstore)
    self.buildstore.InsertBuildStage.assert_called_once_with(
        constants.MOCK_BUILD_ID, stage.name, None,
        constants.BUILDER_STATUS_PLANNED)
    return stage

  def ConstructStage(self):
    return self._ConstructStageWithExpectations(generic_stages.BuilderStage)

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    stage = self.ConstructStage()
    board = self._current_board

    envvar = 'EXAMPLE'
    envvar_val = 'RESULT'
    self.PatchObject(portage_util, 'PortageqEnvvars',
                     return_value={envvar: envvar_val})

    result = stage._GetPortageEnvVar(envvar, board)
    self.assertEqual(result, envvar_val)

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

    exp_url = 'https://ci.chromium.org/b/fake_bb_id'
    self.assertEqual(stage.ConstructDashboardURL(), exp_url)

    stage_name = 'Archive'
    exp_url = ('https://luci-logdog.appspot.com/v/?s=chromeos/buildbucket/'
               'cr-buildbucket.appspot.com/1234321/%2B/steps/Archive/0/stdout')

    self.assertEqual(stage.ConstructDashboardURL(stage=stage_name), exp_url)

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
    return output

  def testRunException(self):
    """Verify stage exceptions are handled."""
    class TestError(Exception):
      """Unique test exception"""

    perform_mock = self.PatchObject(generic_stages.BuilderStage, 'PerformStage')
    perform_mock.side_effect = TestError('fail!')

    stage = self.ConstructStage()
    results_lib.Results.Clear()
    self.assertRaises(failures_lib.StepFailure, self._RunCapture, stage)

    results = results_lib.Results.Get()[0]
    self.assertTrue(isinstance(results.result, TestError))
    self.assertEqual(str(results.result), 'fail!')
    self.buildstore.StartBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID)
    self.buildstore.FinishBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID,
        constants.BUILDER_STATUS_FAILED)

  def testRunExitEarlyException(self):
    """Verify stage exit early exceptions are handled."""
    class TestError(Exception):
      """Unique test exception"""

    perform_mock = self.PatchObject(generic_stages.BuilderStage, 'PerformStage')
    perform_mock.side_effect = TestError('fail!')

    stage = self.ConstructStage()
    results_lib.Results.Clear()
    self.assertRaises(failures_lib.StepFailure, self._RunCapture, stage)

    results = results_lib.Results.Get()[0]
    self.assertTrue(isinstance(results.result, TestError))
    self.assertEqual(str(results.result), 'fail!')
    self.buildstore.StartBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID)
    self.buildstore.FinishBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID,
        constants.BUILDER_STATUS_FAILED)

  def testRunWithWaitFailure(self):
    """Test Run when WaitUntilReady returns False"""
    stage = self.ConstructStage()
    self.PatchObject(generic_stages.BuilderStage,
                     'WaitUntilReady',
                     return_value=False)
    stage.Run()
    self.buildstore.WaitBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID)
    self.buildstore.FinishBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID,
        constants.BUILDER_STATUS_SKIPPED)
    self.assertFalse(self.mock_cidb.StartBuildStage.called)

  @osutils.TempFileDecorator
  def testRunSkipsPreviouslyCompletedStage(self):
    """Test that a stage that has run before is skipped, and marked as such."""
    handle_skip_mock = self.PatchObject(generic_stages.BuilderStage,
                                        'HandleSkip')
    stage = self.ConstructStage()

    # Record a result as if the stage succeeded in a _previous_ run.
    results_lib.Results.Record(stage.name, results_lib.Results.SUCCESS,
                               description='Injected success')
    with open(self.tempfile, 'w') as out:
      results_lib.Results.SaveCompletedStages(out)
    results_lib.Results.Clear()
    with open(self.tempfile, 'r') as out:
      results_lib.Results.RestoreCompletedStages(out)

    output = self._RunCapture(stage)
    all_out = output.GetStdout()
    all_out += output.GetStderr()
    self.assertTrue('[PREVIOUSLY PROCESSED]' in all_out)
    self.assertTrue(handle_skip_mock.called)

  def testHandleExceptionException(self):
    """Verify exceptions in HandleException handlers are themselves handled."""
    class TestError(Exception):
      """Unique test exception"""

    class BadStage(generic_stages.BuilderStage):
      """Stage that throws an exception when PerformStage is called."""

      handled_exceptions = []

      def PerformStage(self):
        raise TestError('first fail')

      def _HandleStageException(self, exc_info):
        self.handled_exceptions.append(str(exc_info[1]))
        raise TestError('nested')

    stage = self._ConstructStageWithExpectations(BadStage)
    results_lib.Results.Clear()
    self.assertRaises(failures_lib.StepFailure, self._RunCapture, stage)

    # Verify the results tracked the original exception.
    results = results_lib.Results.Get()[0]
    self.assertTrue(isinstance(results.result, TestError))
    self.assertEqual(str(results.result), 'first fail')

    self.assertEqual(stage.handled_exceptions, ['first fail'])

    # Verify the stage is still marked as failed in cidb.
    self.buildstore.StartBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID)
    self.buildstore.FinishBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID,
        constants.BUILDER_STATUS_FAILED)

  def testGetJobKeyvalsRegularBuild(self):
    """Test GetJobKeyvals for a build with master."""
    stage = self.ConstructStage()
    build_identifier = BuildIdentifier(cidb_id=constants.MOCK_BUILD_ID,
                                       buildbucket_id=1234)
    self.PatchObject(stage._run, 'GetCIDBHandle',
                     return_value=[build_identifier, None])
    self.PatchObject(self.buildstore, 'GetBuildStatuses',
                     return_value=[{'build_config':'master-paladin'}])
    stage._run.options.master_buildbucket_id = 2341
    stage._run.config.name = 'something-paladin'
    stage._run.options.branch = 'master'
    expected = {
        constants.JOB_KEYVAL_DATASTORE_PARENT_KEY: (
            'Build', constants.MOCK_BUILD_ID),
        constants.JOB_KEYVAL_CIDB_BUILD_ID:
            constants.MOCK_BUILD_ID,
        constants.JOB_KEYVAL_BUILD_CONFIG:
            'something-paladin',
        constants.JOB_KEYVAL_BRANCH:
            'master',
        constants.JOB_KEYVAL_MASTER_BUILD_CONFIG:
            'master-paladin'
    }
    self.assertEqual(stage.GetJobKeyvals(), expected)

  def testGetJobKeyvalsSpecialBuild(self):
    """Test GetJobKeyvals for a build without master."""
    stage = self.ConstructStage()
    build_identifier = BuildIdentifier(cidb_id=constants.MOCK_BUILD_ID,
                                       buildbucket_id=1234)
    self.PatchObject(stage._run, 'GetCIDBHandle',
                     return_value=[build_identifier, None])
    stage._run.config.name = 'something-paladin'
    stage._run.options.branch = 'master'
    expected = {
        constants.JOB_KEYVAL_DATASTORE_PARENT_KEY: (
            'Build', constants.MOCK_BUILD_ID),
        constants.JOB_KEYVAL_CIDB_BUILD_ID:
            constants.MOCK_BUILD_ID,
        constants.JOB_KEYVAL_BUILD_CONFIG:
            'something-paladin',
        constants.JOB_KEYVAL_BRANCH:
            'master',
        constants.JOB_KEYVAL_MASTER_BUILD_CONFIG:
            None
    }
    self.assertEqual(stage.GetJobKeyvals(), expected)

class BuilderStageGetBuildFailureMessage(AbstractStageTestCase):
  """Test GetBuildFailureMessage in BuilderStage."""

  def setUp(self):
    self._Prepare()
    self.buildstore = FakeBuildStore()
    # Many tests modify the global results_lib.Results instance.
    results_lib.Results.Clear()

  def tearDown(self):
    cidb.CIDBConnectionFactory.ClearMock()

  def ConstructStage(self):
    return generic_stages.BuilderStage(self._run, self.buildstore)

  def testGetBuildFailureMessageFromResults(self):
    """Test GetBuildFailureMessageFromResults."""
    ex = failures_lib.StepFailure()
    results_lib.Results.Record('CommitQueueSync', ex)
    stage = self.ConstructStage()
    build_failure_msg = stage.GetBuildFailureMessageFromResults()
    self.assertEqual(build_failure_msg.builder, self.BOT_ID)
    self.assertTrue(isinstance(build_failure_msg.failure_messages[0],
                               failure_message_lib.StageFailureMessage))

  def testGetBuildFailureMessageWithoutBuildStore(self):
    """Test GetBuildFailureMessage without working BuildStore instance."""
    stage = self.ConstructStage()
    message = 'foo'
    get_msg_from_cidb = self.PatchObject(
        stage, 'GetBuildFailureMessageFromBuildStore', return_value=message)
    self.PatchObject(FakeBuildStore, 'AreClientsReady', return_value=False)
    get_msg_from_results = self.PatchObject(
        stage, 'GetBuildFailureMessageFromResults', return_value=message)

    stage.GetBuildFailureMessage()
    get_msg_from_cidb.assert_not_called()
    get_msg_from_results.assert_called_once_with()

  def testGetBuildFailureMessageWithBuildStore(self):
    """Test GetBuildFailureMessage with working BuildStore."""
    db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(db)
    stage = self.ConstructStage()
    message = 'foo'
    get_msg_from_cidb = self.PatchObject(
        stage, 'GetBuildFailureMessageFromBuildStore', return_value=message)
    self.PatchObject(FakeBuildStore, 'AreClientsReady', return_value=True)
    get_msg_from_results = self.PatchObject(
        stage, 'GetBuildFailureMessageFromResults', return_value=message)

    build_identifier = BuildIdentifier(cidb_id=constants.MOCK_BUILD_ID,
                                       buildbucket_id=None)
    self.PatchObject(stage._run, 'GetCIDBHandle',
                     return_value=[build_identifier, None])
    stage.GetBuildFailureMessage()
    get_msg_from_cidb.assert_called_once_with(
        self.buildstore, build_identifier)
    get_msg_from_results.assert_not_called()

  def testMeaningfulMessage(self):
    """Tests that all essential components are in the message."""
    stage = self.ConstructStage()

    exception = Exception('failed!')
    stage_failure_message = failures_lib.GetStageFailureMessageFromException(
        'TacoStage', 1, exception, stage_prefix_name='TacoStage')
    self.PatchObject(FakeBuildStore, 'AreClientsReady', return_value=False)
    self.PatchObject(
        results_lib.Results, 'GetStageFailureMessage',
        return_value=[stage_failure_message])

    msg = stage.GetBuildFailureMessage()
    self.assertTrue(stage._run.config.name in msg.message_summary)
    self.assertTrue(stage._run.ConstructDashboardURL() in msg.message_summary)
    self.assertTrue('the builder failed' in msg.message_summary)


class MasterConfigBuilderStageTest(AbstractStageTestCase):
  """Tests for BuilderStage on master build."""

  BOT_ID = 'master-paladin'

  def setUp(self):
    self._Prepare()
    self.mock_cidb = mock.MagicMock()
    self.buildstore = FakeBuildStore()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.mock_cidb)
    results_lib.Results.Clear()

  def tearDown(self):
    cidb.CIDBConnectionFactory.ClearMock()

  def ConstructStage(self):
    return generic_stages.BuilderStage(self._run, self.buildstore)

  def testGetBuildbucketClient(self):
    """GetBuildbucketClient returns not None."""
    self.PatchObject(buildbucket_lib, 'GetServiceAccount',
                     return_value=True)
    self.PatchObject(auth.AuthorizedHttp, '__init__',
                     return_value=None)
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     '_GetHost',
                     return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)
    stage = self.ConstructStage()
    self.assertIsNotNone(stage.GetBuildbucketClient())

  def testGetBuildbucketClientWithoutServiceAccount(self):
    """GetBuildbucketClient returns None with no ServiceAccount."""
    self.PatchObject(buildbucket_lib, 'GetServiceAccount',
                     return_value=False)
    stage = self.ConstructStage()
    self.assertIsNone(stage.GetBuildbucketClient())

  def testGetBuildbucketClientRaisesException(self):
    """GetBuildbucketClient raises exceptions correctly."""
    self.PatchObject(buildbucket_lib, 'GetServiceAccount',
                     return_value=False)
    self.PatchObject(cbuildbot_run._BuilderRunBase, 'InProduction',
                     return_value=True)
    stage = self.ConstructStage()
    self.assertRaises(buildbucket_lib.NoBuildbucketClientException,
                      stage.GetBuildbucketClient)

  def testGetScheduledSlaveBuildbucketIdsReturnsEmpty(self):
    """test GetScheduledSlaveBuildbucketIds with no builds."""
    stage = self.ConstructStage()
    self.assertEqual(stage.GetScheduledSlaveBuildbucketIds(), [])

  def testGetScheduledSlaveBuildbucketIdsWithoutRetriedBuilds(self):
    """test GetScheduledSlaveBuildbucketIds without retried builds."""
    stage = self.ConstructStage()
    scheduled_slave_builds = [('slave1', 'bb_id1', 0),
                              ('slave2', 'bb_id2', 0)]
    self._run.attrs.metadata.ExtendKeyListWithList(
        constants.METADATA_SCHEDULED_IMPORTANT_SLAVES, scheduled_slave_builds)
    self.assertEqual(set(stage.GetScheduledSlaveBuildbucketIds()),
                     {'bb_id1', 'bb_id2'})

  def testGetScheduledSlaveBuildbucketIdsWithRetriedBuilds(self):
    """test GetScheduledSlaveBuildbucketIds With Retried Builds."""
    stage = self.ConstructStage()
    scheduled_slave_builds = [('slave1', 'bb_id1', 0),
                              ('slave2', 'bb_id2', 0),
                              ('slave1', 'bb_id3', 3)]
    self._run.attrs.metadata.ExtendKeyListWithList(
        constants.METADATA_SCHEDULED_IMPORTANT_SLAVES, scheduled_slave_builds)
    self.assertEqual(set(stage.GetScheduledSlaveBuildbucketIds()),
                     {'bb_id3', 'bb_id2'})

  def testGetScheduledSlaveBuildbucketIdsReturnsNone(self):
    """Returns None for non master build."""
    stage = self.ConstructStage()
    stage._run.config.slave_configs = False
    self.assertIsNone(stage.GetScheduledSlaveBuildbucketIds())

  def testGetSlaveConfigs(self):
    """Verify that _GetSlaveConfigs filters out experimental builders."""
    stage = self.ConstructStage()

    slave_configs = stage._GetSlaveConfigs()
    slave_config_names = [config['name'] for config in slave_configs]
    self.assertIn('arm-generic-paladin', slave_config_names)

    self._run.attrs.metadata.UpdateWithDict(
        {constants.METADATA_EXPERIMENTAL_BUILDERS: ['arm-generic-paladin']}
    )
    slave_configs = stage._GetSlaveConfigs()
    slave_config_names = [config['name'] for config in slave_configs]
    self.assertNotIn('arm-generic-paladin', slave_config_names)

  def testGetSlaveConfigMap(self):
    """Verify that _GetSlaveConfigMap filters out experimental builders."""
    stage = self.ConstructStage()

    slave_config_map = stage._GetSlaveConfigMap()
    self.assertIn('arm-generic-paladin', slave_config_map)

    self._run.attrs.metadata.UpdateWithDict({
        constants.METADATA_EXPERIMENTAL_BUILDERS: ['arm-generic-paladin']
    })
    slave_config_map = stage._GetSlaveConfigMap(important_only=True)
    self.assertNotIn('arm-generic-paladin', slave_config_map)
    slave_config_map = stage._GetSlaveConfigMap(important_only=False)
    self.assertIn('arm-generic-paladin', slave_config_map)


class BoardSpecificBuilderStageTest(AbstractStageTestCase):
  """Tests option/config settings on board-specific stages."""

  DEFAULT_BOARD_NAME = 'my_shiny_test_board'

  def setUp(self):
    self.buildstore = FakeBuildStore()
    self._Prepare()

  def ConstructStage(self):
    return generic_stages.BoardSpecificBuilderStage(self._run,
                                                    self.buildstore,
                                                    self.DEFAULT_BOARD_NAME)

  def testBuilderNameContainsBoardName(self):
    self._run.config.grouped = True
    stage = self.ConstructStage()
    self.assertTrue(self.DEFAULT_BOARD_NAME in stage.name)

  # TODO (yjhong): Fix this test.
  # def testCheckOptions(self):
  #   """Makes sure options/config settings are setup correctly."""
  #   parser = cbuildbot._CreateParser()
  #   (options, _) = parser.parse_args([])

  #   for attr in dir(stages):
  #     obj = eval('stages.' + attr)
  #     if not hasattr(obj, '__base__'):
  #       continue
  #     if not obj.__base__ is stages.BoardSpecificBuilderStage:
  #       continue
  #     if obj.option_name:
  #       self.assertTrue(getattr(options, obj.option_name))
  #     if obj.config_name:
  #       if not obj.config_name in config._settings:
  #         self.fail(('cbuildbot_stages.%s.config_name "%s" is missing from '
  #                    'cbuildbot_config._settings') % (attr, obj.config_name))

  def testListOfPackagesToBuild(self):
    """Test the default list of packages to build."""
    stage = self.ConstructStage()
    packages = stage.GetListOfPackagesToBuild()
    expected = ['virtual/target-os', 'virtual/target-os-dev',
                'virtual/target-os-test', 'virtual/target-os-factory',
                'virtual/target-os-factory-shim', 'chromeos-base/autotest-all']
    self.assertEqual(expected, packages)

    # Test if an explicit list of packages is configured, only that list is set.
    self._run.config.packages = ['pkgA', 'pkgB']
    packages = stage.GetListOfPackagesToBuild()
    self.assertEqual(['pkgA', 'pkgB'], packages)

    # Test if a list of packages is set for ChromeOS Findit builds, they are
    # added to an explicit list.
    self._run.options.cbb_build_packages = ['pkgC', 'pkgD']
    packages = stage.GetListOfPackagesToBuild()
    self.assertEqual(['pkgC', 'pkgD', 'pkgA', 'pkgB'], packages)

    # Only check packages setup for ChromeOS Findit.
    self._run.config.packages = []
    packages = stage.GetListOfPackagesToBuild()
    self.assertEqual(['pkgC', 'pkgD'], packages)


class RunCommandAbstractStageTestCase(
    AbstractStageTestCase, cros_test_lib.RunCommandTestCase):
  """Base test class for testing a stage and mocking RunCommand."""

  # pylint: disable=abstract-method

  FULL_BOT_ID = 'amd64-generic-full'
  BIN_BOT_ID = 'amd64-generic-paladin'

  def _Prepare(self, bot_id, **kwargs):
    super(RunCommandAbstractStageTestCase, self)._Prepare(bot_id, **kwargs)

  def _PrepareFull(self, **kwargs):
    self._Prepare(self.FULL_BOT_ID, **kwargs)

  def _PrepareBin(self, **kwargs):
    self._Prepare(self.BIN_BOT_ID, **kwargs)

  def _Run(self, dir_exists):
    """Helper for running the build."""
    with patch(os.path, 'isdir', return_value=dir_exists):
      self.RunStage()


class ArchivingStageMixinMock(partial_mock.PartialMock):
  """Partial mock for ArchivingStageMixin."""

  TARGET = 'chromite.cbuildbot.stages.generic_stages.ArchivingStageMixin'
  ATTRS = ('UploadArtifact',)

  def UploadArtifact(self, *args, **kwargs):
    with patch(commands, 'ArchiveFile', return_value='foo.txt'):
      with patch(commands, 'UploadArchivedFile'):
        self.backup['UploadArtifact'](*args, **kwargs)
