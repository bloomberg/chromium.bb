# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for generic stages."""

from __future__ import print_function

import contextlib
import copy
import mock
import os
import sys
import unittest

from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import chromeos_config
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cidb
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import partial_mock
from chromite.lib import portage_util
from chromite.scripts import cbuildbot


DEFAULT_BUILD_NUMBER = 1234321
DEFAULT_BUILD_ID = 31337
DEFAULT_BUILD_STAGE_ID = 313377


# pylint: disable=protected-access


# The inheritence order ensures the patchers are stopped before
# cleaning up the temporary directories.
class StageTestCase(cros_test_lib.MockOutputTestCase,
                    cros_test_lib.TempDirTestCase):
  """Test running a single stage in isolation."""

  TARGET_MANIFEST_BRANCH = 'ooga_booga'
  BUILDROOT = 'buildroot'

  # Subclass should override this to default to a different build config
  # for its tests.
  BOT_ID = 'x86-generic-paladin'

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

  def _Prepare(self, bot_id=None, extra_config=None, cmd_args=None,
               extra_cmd_args=None, build_id=DEFAULT_BUILD_ID,
               waterfall=constants.WATERFALL_INTERNAL,
               waterfall_url=constants.BUILD_INT_DASHBOARD,
               master_build_id=None,
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
      cmd_args: List to override the default cbuildbot command args.
      extra_cmd_args: List to add to default cbuildbot command args.  This
        is a good way to adjust an options value for your test.
        Example: ['branch-name', 'some-branch-name'] will effectively cause
        self._run.options.branch_name to be set to 'some-branch-name'.
      build_id: mock build id
      waterfall: Name of the current waterfall.
                 Possibly from constants.CIDB_KNOWN_WATERFALLS.
      waterfall_url: Url for the current waterfall.
      master_build_id: mock build id of master build.
      site_config: SiteConfig to use (or MockSiteConfig)
    """
    # Use cbuildbot parser to create options object and populate default values.
    parser = cbuildbot._CreateParser()
    if not cmd_args:
      # Fill in default command args.
      cmd_args = [
          '-r', self.build_root, '--buildbot', '--noprebuilts',
          '--buildnumber', str(DEFAULT_BUILD_NUMBER),
          '--branch', self.TARGET_MANIFEST_BRANCH,
      ]
    if extra_cmd_args:
      cmd_args += extra_cmd_args
    (options, args) = parser.parse_args(cmd_args)

    # The bot_id can either be specified as arg to _Prepare method or in the
    # cmd_args (as cbuildbot normally accepts it from command line).
    if args:
      self._bot_id = args[0]
      if bot_id:
        # This means bot_id was specified as _Prepare arg and in cmd_args.
        # Make sure they are the same.
        self.assertEquals(self._bot_id, bot_id)
    else:
      self._bot_id = bot_id or self.BOT_ID
      args = [self._bot_id]
    cbuildbot._FinishParsing(options, args)

    if site_config is None:
      site_config = chromeos_config.GetConfig()

    # Populate build_config corresponding to self._bot_id.
    build_config = copy.deepcopy(site_config[self._bot_id])
    build_config['manifest_repo_url'] = 'fake_url'
    if extra_config:
      build_config.update(extra_config)
    if options.remote_trybot:
      build_config = config_lib.OverrideConfigForTrybot(
          build_config, options)
    options.managed_chrome = build_config['sync_chrome']

    self._boards = build_config['boards']
    self._current_board = self._boards[0] if self._boards else None

    # Some preliminary sanity checks.
    self.assertEquals(options.buildroot, self.build_root)

    # Construct a real BuilderRun using options and build_config.
    self._run = cbuildbot_run.BuilderRun(
        options, site_config, build_config, self._manager)

    if build_id is not None:
      self._run.attrs.metadata.UpdateWithDict({'build_id': build_id})

    if master_build_id is not None:
      self._run.options.master_build_id = master_build_id

    self._run.attrs.metadata.UpdateWithDict({'buildbot-master-name': waterfall})
    self._run.attrs.metadata.UpdateWithDict({'buildbot-url': waterfall_url})

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

  def ConstructStage(self):
    """Returns an instance of the stage to be tested.

    Note: Must be implemented in subclasses.
    """
    raise NotImplementedError(self, "ConstructStage: Implement in your test")

  def RunStage(self):
    """Creates and runs an instance of the stage to be tested.

    Note: Requires ConstructStage() to be implemented.

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
      stack.Add(lambda ret=arg: ret)
    yield


class BuilderStageTest(AbstractStageTestCase):
  """Tests for BuilderStage class."""

  def setUp(self):
    self._Prepare(waterfall=constants.WATERFALL_EXTERNAL)
    self.mock_cidb = mock.MagicMock()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.mock_cidb)

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

    self.PatchObject(self.mock_cidb, 'InsertBuildStage',
                     return_value=DEFAULT_BUILD_STAGE_ID)
    stage = stage_class(self._run)
    self.mock_cidb.InsertBuildStage.assert_called_once_with(
        build_id=DEFAULT_BUILD_ID,
        name=mock.ANY)
    return stage

  def ConstructStage(self):
    return self._ConstructStageWithExpectations(generic_stages.BuilderStage)

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    stage = self.ConstructStage()
    board = self._current_board

    envvar = 'EXAMPLE'
    rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc_mock.AddCmdResult(['portageq-%s' % board, 'envvar', envvar],
                         output='RESULT\n')

    result = stage._GetPortageEnvVar(envvar, board)
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

    exp_url = ('https://uberchromegw.corp.google.com/i/chromeos/builders/'
               'x86-generic-paladin/builds/%s' % DEFAULT_BUILD_NUMBER)
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

    perform_mock = self.PatchObject(generic_stages.BuilderStage, 'PerformStage')
    perform_mock.side_effect = TestError('fail!')

    stage = self.ConstructStage()
    results_lib.Results.Clear()
    self.assertRaises(failures_lib.StepFailure, self._RunCapture, stage)

    results = results_lib.Results.Get()[0]
    self.assertTrue(isinstance(results.result, TestError))
    self.assertEqual(str(results.result), 'fail!')
    self.mock_cidb.StartBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID)
    self.mock_cidb.FinishBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID,
        constants.BUILDER_STATUS_FAILED)

  def testRunWithWaitFailure(self):
    """Test Run when WaitUntilReady returns False"""
    stage = self.ConstructStage()
    self.PatchObject(generic_stages.BuilderStage,
                     'WaitUntilReady',
                     return_value=False)
    stage.Run()
    self.mock_cidb.WaitBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID)
    self.mock_cidb.FinishBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID,
        constants.BUILDER_STATUS_SKIPPED)
    self.assertFalse(self.mock_cidb.StartBuildStage.called)

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
    self.mock_cidb.StartBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID)
    self.mock_cidb.FinishBuildStage.assert_called_once_with(
        DEFAULT_BUILD_STAGE_ID,
        constants.BUILDER_STATUS_FAILED)


class BoardSpecificBuilderStageTest(AbstractStageTestCase):
  """Tests option/config settings on board-specific stages."""

  DEFAULT_BOARD_NAME = 'my_shiny_test_board'

  def setUp(self):
    self._Prepare()

  def ConstructStage(self):
    return generic_stages.BoardSpecificBuilderStage(self._run,
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


class RunCommandAbstractStageTestCase(
    AbstractStageTestCase, cros_build_lib_unittest.RunCommandTestCase):
  """Base test class for testing a stage and mocking RunCommand."""

  # pylint: disable=abstract-method

  FULL_BOT_ID = 'x86-generic-full'
  BIN_BOT_ID = 'x86-generic-paladin'

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
