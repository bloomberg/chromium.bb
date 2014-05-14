#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for generic stages."""

import contextlib
import copy
import mox
import os
import sys
import unittest

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import cbuildbot_commands as commands
from chromite.cbuildbot import cbuildbot_config as config
from chromite.cbuildbot import cbuildbot_failures as failures_lib
from chromite.cbuildbot import cbuildbot_results as results_lib
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import portage_utilities
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import partial_mock
from chromite.scripts import cbuildbot

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


DEFAULT_CHROME_BRANCH = '27'


class BuilderRunMock(partial_mock.PartialMock):
  """Partial mock for BuilderRun class."""

  TARGET = 'chromite.cbuildbot.cbuildbot_run._BuilderRunBase'
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
               extra_cmd_args=None):
    """Prepare a BuilderRun at self._run for this test.

    This method must allow being called more than once.  Subclasses can
    override this method, but those subclass methods should also call this one.

    The idea is that all test preparation that falls out from the choice of
    build config and cbuildbot options should go in _Prepare.

    This will populate the following attributes on self:
      run: A BuilderRun object.
      bot_id: The bot id (name) that was used from config.config.
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
      self._bot_id = args[0]
      if bot_id:
        # This means bot_id was specified as _Prepare arg and in cmd_args.
        # Make sure they are the same.
        self.assertEquals(self._bot_id, bot_id)
    else:
      self._bot_id = bot_id or self.BOT_ID
      args = [self._bot_id]
    cbuildbot._FinishParsing(options, args)

    # Populate build_config corresponding to self._bot_id.
    build_config = copy.deepcopy(config.config[self._bot_id])
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
    self._run = cbuildbot_run.BuilderRun(options, build_config, self._manager)

    if self.RELEASE_TAG is not None:
      self._run.attrs.release_tag = self.RELEASE_TAG

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
    hw_tests = self._run.config['hw_tests']
    if not hw_tests:
      # TODO(milleral): Add HWTests back to lumpy-chrome-perf.
      raise unittest.SkipTest('Missing HWTest for %s' % (self._bot_id,))

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
    return generic_stages.BuilderStage(self._run)

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

    perform_mock = self.PatchObject(generic_stages.BuilderStage, 'PerformStage')
    perform_mock.side_effect = TestError('fail!')

    stage = self.ConstructStage()
    results_lib.Results.Clear()
    self.assertRaises(failures_lib.StepFailure, self._RunCapture, stage)

    results = results_lib.Results.Get()[0]
    self.assertTrue(isinstance(results.result, TestError))
    self.assertEqual(str(results.result), 'fail!')

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

    stage = BadStage(self._run)
    results_lib.Results.Clear()
    self.assertRaises(failures_lib.StepFailure, self._RunCapture, stage)

    # Verify the results tracked the original exception.
    results = results_lib.Results.Get()[0]
    self.assertTrue(isinstance(results.result, TestError))
    self.assertEqual(str(results.result), 'first fail')

    self.assertEqual(stage.handled_exceptions, ['first fail'])


class BoardSpecificBuilderStageTest(cros_test_lib.TestCase):
  """Tests option/config settings on board-specific stages."""

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


class ArchivingStageMixinMock(partial_mock.PartialMock):
  """Partial mock for ArchivingStageMixin."""

  TARGET = 'chromite.cbuildbot.stages.generic_stages.ArchivingStageMixin'
  ATTRS = ('UploadArtifact',)

  def UploadArtifact(self, *args, **kwargs):
    with patch(commands, 'ArchiveFile', return_value='foo.txt'):
      with patch(commands, 'UploadArchivedFile'):
        self.backup['UploadArtifact'](*args, **kwargs)



if __name__ == '__main__':
  cros_test_lib.main()
