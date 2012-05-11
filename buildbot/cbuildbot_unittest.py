#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

import glob
import mox
import os
import shutil
import sys
import tempfile
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config as config
from chromite.buildbot import cbuildbot_stages as stages
from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import osutils
from chromite.scripts import cbuildbot

# pylint: disable=W0212,R0904
class TestExitedException(Exception):
  """Exception used by sys.exit() mock to halt execution."""
  pass

class TestHaltedException(Exception):
  """Exception used by mocks to halt execution without indicating failure."""
  pass

class TestFailedException(Exception):
  """Exception used by mocks to halt execution and indicate failure."""
  pass

class RunBuildStagesTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.buildroot = tempfile.mkdtemp()
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-paladin'
    self.build_config = config.config[self.bot_id]
    self.build_config['master'] = False
    self.build_config['important'] = False

    # Use the cbuildbot parser to create properties and populate default values.
    self.parser = cbuildbot._CreateParser()
    (self.options, _) = self.parser.parse_args(['-r', self.buildroot,
                                                '--buildbot', '--debug'])
    self.options.clean = False
    self.options.resume = False
    self.options.sync = False
    self.options.build = False
    self.options.uprev = False
    self.options.tests = False
    self.options.archive = False
    self.options.remote_test_status = False
    self.options.patches = None
    self.options.prebuilts = False

    self.mox.StubOutWithMock(cbuildbot, '_GetChromiteTrackingBranch')
    cbuildbot._GetChromiteTrackingBranch().MultipleTimes().AndReturn('master')
    self.mox.StubOutWithMock(stages.SyncStage, 'HandleSkip')
    stages.SyncStage.HandleSkip()

  def tearDown(self):
    if os.path.exists(self.buildroot):
      shutil.rmtree(self.buildroot)

  def testChromeosOfficialSet(self):
    """Verify that CHROMEOS_OFFICIAL is set correctly."""
    self.build_config['chromeos_official'] = True

    # Clean up before
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

    result = self.mox.CreateMock(cros_lib.CommandResult)
    result.returncode = 0
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    cros_lib.RunCommand(mox.IgnoreArg(), cwd=self.buildroot,
                        error_code_ok=True,
                        kill_timeout=mox.IgnoreArg()).AndReturn(result)
    self.mox.ReplayAll()

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    cbuildbot.SimpleBuilder(self.options, self.build_config).Run()

    self.assertTrue('CHROMEOS_OFFICIAL' in os.environ)

    self.mox.VerifyAll()

    # Clean up after the test
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

  def testChromeosOfficialNotSet(self):
    """Verify that CHROMEOS_OFFICIAL is not always set."""

    self.build_config['chromeos_official'] = False

    # Clean up before
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

    result = self.mox.CreateMock(cros_lib.CommandResult)
    result.returncode = 0
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    cros_lib.RunCommand(mox.IgnoreArg(), cwd=self.buildroot,
                        error_code_ok=True,
                        kill_timeout=mox.IgnoreArg()).AndReturn(result)

    self.mox.ReplayAll()

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    cbuildbot.SimpleBuilder(self.options, self.build_config).Run()

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    self.mox.VerifyAll()

    # Clean up after the test
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']


class LogTest(mox.MoxTestBase):

  def _generateLogs(self, num):
    """Generates cbuildbot.log and num backups."""
    with open(os.path.join(self.tempdir, 'cbuildbot.log'), 'w') as f:
      f.write(str(num+1))

    for i in range(1, num+1):
      with open(os.path.join(self.tempdir, 'cbuildbot.log.' + str(i)),
                'w') as f:
        f.write(str(i))

  @osutils.TempDirDecorator
  def testZeroToOneLogs(self):
    """Test beginning corner case."""
    self._generateLogs(0)
    cbuildbot._BackupPreviousLog(os.path.join(self.tempdir, 'cbuildbot.log'),
                                 backup_limit=25)
    with open(os.path.join(self.tempdir, 'cbuildbot.log.1')) as f:
      self.assertEquals(f.readline(), '1')

  @osutils.TempDirDecorator
  def testNineToTenLogs(self):
    """Test handling *.log.9 to *.log.10 (correct sorting)."""
    self._generateLogs(9)
    cbuildbot._BackupPreviousLog(os.path.join(self.tempdir, 'cbuildbot.log'),
                                 backup_limit=25)
    with open(os.path.join(self.tempdir, 'cbuildbot.log.10')) as f:
      self.assertEquals(f.readline(), '10')

  @osutils.TempDirDecorator
  def testOverLimit(self):
    """Test going over the limit and having to purge old logs."""
    self._generateLogs(25)
    cbuildbot._BackupPreviousLog(os.path.join(self.tempdir, 'cbuildbot.log'),
                                 backup_limit=25)
    with open(os.path.join(self.tempdir, 'cbuildbot.log.26')) as f:
      self.assertEquals(f.readline(), '26')

    self.assertEquals(len(glob.glob(os.path.join(self.tempdir, 'cbuildbot*'))),
                      25)


class InterfaceTest(mox.MoxTestBase):

  _X86_PREFLIGHT = 'x86-generic-paladin'
  _BUILD_ROOT = '/b/test_build1'
  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.parser = cbuildbot._CreateParser()
    self.mox.StubOutWithMock(cros_lib, 'Die')

  def testDebugBuildBotSetByDefault(self):
    """Test that debug and buildbot flags are set by default."""
    args = ['--local', '-r', self._BUILD_ROOT, self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.debug, True)
    self.assertEquals(options.buildbot, False)

  def testBuildBotOption(self):
    """Test that --buildbot option unsets debug flag."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.debug, False)
    self.assertEquals(options.buildbot, True)

  def testBuildBotWithDebugOption(self):
    """Test that --debug option overrides --buildbot option."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', '--debug',
            self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.debug, True)
    self.assertEquals(options.buildbot, True)

  def testBuildBotWithPatches(self):
    """Test that --buildbot errors out with patches."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', '-g', '1234',
            self._X86_PREFLIGHT]
    cros_lib.Die(mox.IgnoreArg()).AndRaise(Exception)
    self.mox.ReplayAll()
    self.assertRaises(Exception, cbuildbot._ParseCommandLine, self.parser, args)

  def testRemoteBuildBotWithPatches(self):
    """Test that --buildbot and --remote errors out with patches."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', '--remote', '-g', '1234',
            self._X86_PREFLIGHT]
    cros_lib.Die(mox.IgnoreArg()).AndRaise(Exception)
    self.mox.ReplayAll()
    self.assertRaises(Exception, cbuildbot._ParseCommandLine, self.parser, args)

  def testBuildBotWithoutProfileOption(self):
    """Test that no --profile option gets defaulted."""
    args = ['--buildbot', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.profile, None)

  def testBuildBotWithProfileOption(self):
    """Test that --profile option gets parsed."""
    args = ['--buildbot', '--profile', 'carp', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.profile, 'carp')

  def testValidateClobberUserDeclines_1(self):
    """Test case where user declines in prompt."""
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cros_lib, 'GetInput')

    os.path.exists(self._BUILD_ROOT).AndReturn(True)
    cros_lib.GetInput(mox.IgnoreArg()).AndReturn('No')

    self.mox.ReplayAll()
    self.assertFalse(commands.ValidateClobber(self._BUILD_ROOT))
    self.mox.VerifyAll()

  def testValidateClobberUserDeclines_2(self):
    """Test case where user does not enter the full 'yes' pattern."""
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cros_lib, 'GetInput')

    os.path.exists(self._BUILD_ROOT).AndReturn(True)
    cros_lib.GetInput(mox.IgnoreArg()).AndReturn('y')
    cros_lib.GetInput(mox.IgnoreArg()).AndReturn('No')

    self.mox.ReplayAll()
    self.assertFalse(commands.ValidateClobber(self._BUILD_ROOT))
    self.mox.VerifyAll()

  def testValidateClobberProtectRunningChromite(self):
    """User should not be clobbering our own source."""
    cwd = os.path.dirname(os.path.realpath(__file__))
    buildroot = os.path.dirname(cwd)
    cros_lib.Die(mox.IgnoreArg()).AndRaise(Exception)
    self.mox.ReplayAll()
    self.assertRaises(Exception, commands.ValidateClobber, buildroot)
    self.mox.VerifyAll()

  def testBuildBotWithBadChromeRevOption(self):
    """chrome_rev can't be passed an invalid option after chrome_root."""
    args = ['--local',
        '--buildroot=/tmp',
        '--chrome_root=.',
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        self._X86_PREFLIGHT]
    cros_lib.Die(mox.IgnoreArg()).AndRaise(Exception)
    self.mox.ReplayAll()
    self.assertRaises(Exception, cbuildbot._ParseCommandLine, self.parser, args)
    self.mox.VerifyAll()

  def testBuildBotWithBadChromeRootOption(self):
    """chrome_root can't get passed after non-local chrome_rev."""
    args = ['--local',
        '--buildroot=/tmp',
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        '--chrome_root=.',
        self._X86_PREFLIGHT]
    cros_lib.Die(mox.IgnoreArg()).AndRaise(Exception)
    self.mox.ReplayAll()
    self.assertRaises(Exception, cbuildbot._ParseCommandLine, self.parser, args)
    self.mox.VerifyAll()

  def testBuildBotWithBadChromeRevOptionLocal(self):
    """chrome_rev can't be local without chrome_root."""
    args = ['--local',
        '--buildroot=/tmp',
        '--chrome_rev=%s' % constants.CHROME_REV_LOCAL,
        self._X86_PREFLIGHT]
    cros_lib.Die(mox.IgnoreArg()).AndRaise(Exception)
    self.mox.ReplayAll()
    self.assertRaises(Exception, cbuildbot._ParseCommandLine, self.parser, args)
    self.mox.VerifyAll()

  def testBuildBotWithGoodChromeRootOption(self):
    """chrome_root can be set without chrome_rev."""
    args = ['--local',
        '--buildroot=/tmp',
        '--chrome_root=.',
        self._X86_PREFLIGHT]
    self.mox.ReplayAll()
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.mox.VerifyAll()
    self.assertEquals(options.chrome_rev, constants.CHROME_REV_LOCAL)
    self.assertNotEquals(options.chrome_root, None)

  def testBuildBotWithGoodChromeRevAndRootOption(self):
    """chrome_rev can get reset around chrome_root."""
    args = ['--local',
        '--buildroot=/tmp',
        '--chrome_rev=%s' % constants.CHROME_REV_LATEST,
        '--chrome_rev=%s' % constants.CHROME_REV_STICKY,
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        '--chrome_rev=%s' % constants.CHROME_REV_STICKY,
        '--chrome_rev=%s' % constants.CHROME_REV_LATEST,
        '--chrome_rev=%s' % constants.CHROME_REV_LOCAL,
        '--chrome_root=.',
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        '--chrome_rev=%s' % constants.CHROME_REV_LOCAL,
        self._X86_PREFLIGHT]
    self.mox.ReplayAll()
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.mox.VerifyAll()
    self.assertEquals(options.chrome_rev, constants.CHROME_REV_LOCAL)
    self.assertNotEquals(options.chrome_root, None)

  def testPassThroughOptions(self):
    """Test we are building up pass-through list properly."""
    args = ['--remote', '--lkgm', '-g', '1234', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.pass_through_args, ['--lkgm', '-g', '1234'])


class FullInterfaceTest(unittest.TestCase):
  """Tests that run the cbuildbot.main() function directly.

  Don't inherit from MoxTestBase since it runs VerifyAll() at the end of every
  test which we don't want.
  """
  _BUILD_ROOT = '/b/test_build1'

  def setUp(self):
    def fake_real_path(path):
      """Used to mock out os.path.realpath"""
      if path == FullInterfaceTest._BUILD_ROOT:
        return path
      else:
        raise TestFailedException('Unexpected os.path.realpath(%s) call' % path)

    self.mox = mox.Mox()

    # Create the parser before we stub out os.path.exists() - which the parser
    # creation code actually uses.
    parser = cbuildbot._CreateParser()

    # Stub out all relevant methods regardless of whether they are called in the
    # specific test case.  We can do this because we don't run VerifyAll() at
    # the end of every test.
    self.mox.StubOutWithMock(parser, 'error')
    self.mox.StubOutWithMock(cbuildbot.os.path, 'exists')
    self.mox.StubOutWithMock(cros_lib, 'IsInsideChroot')
    self.mox.StubOutWithMock(cbuildbot, '_CreateParser')
    self.mox.StubOutWithMock(sys, 'exit')
    self.mox.StubOutWithMock(cros_lib, 'GetInput')
    self.mox.StubOutWithMock(cros_lib, 'FindRepoDir')
    self.mox.StubOutWithMock(cbuildbot, '_RunBuildStagesWrapper')
    self.mox.StubOutWithMock(cbuildbot.os.path, 'realpath')

    cbuildbot.os.path.realpath = fake_real_path
    # Suppress cgroups code.  For cbuildbot invocation, it doesn't hugely
    # care about cgroups- that's a blackbox to it.  As such these unittests
    # should not be sensitive to it.
    os.path.exists('/proc/cgroups').InAnyOrder().AndReturn(False)
    os.path.exists(constants.SOURCE_ROOT).InAnyOrder().AndReturn(True)
    os.path.exists(os.path.join(
        constants.SOURCE_ROOT, '.repo')).InAnyOrder().AndReturn(True)
    parser.error(mox.IgnoreArg()).InAnyOrder().AndRaise(TestExitedException())
    cros_lib.IsInsideChroot().InAnyOrder().AndReturn(False)
    cbuildbot._CreateParser().InAnyOrder().AndReturn(parser)
    sys.exit(mox.IgnoreArg()).InAnyOrder().AndRaise(TestExitedException())
    cros_lib.FindRepoDir().InAnyOrder().AndReturn('/b/test_build1/.repo')
    cbuildbot._RunBuildStagesWrapper(
        mox.IgnoreArg(),
        mox.IgnoreArg()).InAnyOrder().AndReturn(True)

    self.external_marker = ('/b/trybot/.trybot')
    self.internal_marker = ('/b/trybot-internal/.trybot')

  def tearDown(self):
    self.mox.UnsetStubs()

  def testNullArgsStripped(self):
    """Test that null args are stripped out and don't cause error."""
    self.mox.ReplayAll()
    cbuildbot.main(['--local', '-r', self._BUILD_ROOT, '', '',
                    'x86-generic-paladin'])

  def testMultipleConfigsError(self):
    """Test that multiple configs cause error if --remote is not used."""
    self.mox.ReplayAll()
    self.assertRaises(TestExitedException, cbuildbot.main,
                     ['--local',
                      '-r', self._BUILD_ROOT,
                      'arm-generic-paladin',
                      'x86-generic-paladin'])

  def testDontInferBuildrootForBuildBotRuns(self):
    """Test that we don't infer buildroot if run with --buildbot option."""
    self.mox.ReplayAll()
    self.assertRaises(TestExitedException, cbuildbot.main,
                      ['--buildbot', 'x86-generic-paladin'])

  def testInferExternalBuildRoot(self):
    """Test that we default to correct buildroot for external config."""
    self.mox.StubOutWithMock(cbuildbot, '_ConfirmBuildRoot')
    (cbuildbot._ConfirmBuildRoot(mox.IgnoreArg()).InAnyOrder()
        .AndRaise(TestHaltedException()))
    os.path.exists(self.external_marker).InAnyOrder().AndReturn(False)

    self.mox.ReplayAll()
    self.assertRaises(TestHaltedException, cbuildbot.main,
                      ['--local', 'x86-generic-paladin'])

  def testInferInternalBuildRoot(self):
    """Test that we default to correct buildroot for internal config."""
    self.mox.StubOutWithMock(cbuildbot, '_ConfirmBuildRoot')
    (cbuildbot._ConfirmBuildRoot(mox.IgnoreArg()).InAnyOrder()
        .AndRaise(TestHaltedException()))
    os.path.exists(self.internal_marker).InAnyOrder().AndReturn(False)

    self.mox.ReplayAll()
    self.assertRaises(TestHaltedException, cbuildbot.main,
                      ['--local', 'mario-paladin'])

  def testInferBuildRootPromptNo(self):
    """Test that a 'no' answer on the prompt halts execution."""
    os.path.exists(self.external_marker).InAnyOrder().AndReturn(False)
    cros_lib.GetInput(mox.IgnoreArg()).InAnyOrder().AndReturn('no')

    self.mox.ReplayAll()
    self.assertRaises(TestExitedException, cbuildbot.main,
                      ['--local', 'x86-generic-paladin'])

  def testInferBuildRootExists(self):
    """Test that we don't prompt the user if buildroot already exists."""
    os.path.exists(self.external_marker).InAnyOrder().AndReturn(True)
    (cros_lib.GetInput(mox.IgnoreArg()).InAnyOrder()
        .AndRaise(TestFailedException()))

    self.mox.ReplayAll()
    cbuildbot.main(['--local', 'x86-generic-paladin'])

  def testBuildbotDiesInChroot(self):
    """Buildbot should quit if run inside a chroot."""
    # Need to do this since a cros_lib.IsInsideChroot() call is already queued
    # up in setup() and we can't Reset() an individual mock.
    # pylint: disable=E1102
    new_is_inside_chroot = self.mox.CreateMockAnything()
    new_is_inside_chroot().InAnyOrder().AndReturn(True)
    cros_lib.IsInsideChroot = new_is_inside_chroot
    self.mox.ReplayAll()
    self.assertRaises(TestExitedException, cbuildbot.main,
                      ['--local', '-r', self._BUILD_ROOT,
                       'x86-generic-paladin'])


if __name__ == '__main__':
  unittest.main()
