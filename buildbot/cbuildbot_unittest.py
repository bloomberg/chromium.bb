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
import chromite.buildbot.cbuildbot as cbuildbot
import chromite.buildbot.cbuildbot_config as config
import chromite.lib.cros_build_lib as cros_lib

class RunBuildStagesTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-pre-flight-queue'
    self.build_config = config.config[self.bot_id]
    self.build_config['master'] = False
    self.build_config['important'] = False

    self.options = self.mox.CreateMockAnything()
    self.options.buildroot = '.'
    self.options.resume = False
    self.options.sync = False
    self.options.build = False
    self.options.uprev = False
    self.options.tests = False
    self.options.archive = False
    self.options.remote_test_status = False
    self.options.patches = None

  def testChromeosOfficialSet(self):
    """Verify that CHROMEOS_OFFICIAL is set correctly."""

    self.build_config['chromeos_official'] = True
    self.mox.StubOutWithMock(cbuildbot, '_GetChromiteTrackingBranch')
    cbuildbot._GetChromiteTrackingBranch().AndReturn('master')

    # Clean up before
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

    self.mox.ReplayAll()

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    cbuildbot.RunBuildStages(self.bot_id,
                             self.options,
                             self.build_config)

    self.assertTrue('CHROMEOS_OFFICIAL' in os.environ)

    self.mox.VerifyAll()

    # Clean up after the test
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

  def testChromeosOfficialNotSet(self):
    """Verify that CHROMEOS_OFFICIAL is not always set."""

    self.build_config['chromeos_official'] = False
    self.mox.StubOutWithMock(cbuildbot, '_GetChromiteTrackingBranch')
    cbuildbot._GetChromiteTrackingBranch().AndReturn('master')

    # Clean up before
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

    self.mox.ReplayAll()

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    cbuildbot.RunBuildStages(self.bot_id,
                             self.options,
                             self.build_config)

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    self.mox.VerifyAll()

    # Clean up after the test
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']


class InterfaceTest(mox.MoxTestBase):

  _X86_PREFLIGHT = 'x86-generic-pre-flight-queue'
  _BUILD_ROOT = '/b/test_build1'
  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.parser = cbuildbot._CreateParser()

  def testDebugBuildBotSetByDefault(self):
    """Test that debug and buildbot flags are set by default."""
    args = ['-r', self._BUILD_ROOT, self._X86_PREFLIGHT]
    (options, args) = self.parser.parse_args(args=args)
    self.assertEquals(options.debug, True)
    self.assertEquals(options.buildbot, False)

  def testBuildBotOption(self):
    """Test that --buildbot option unsets debug flag."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', self._X86_PREFLIGHT]
    (options, args) = self.parser.parse_args(args=args)
    self.assertEquals(options.debug, False)
    self.assertEquals(options.buildbot, True)

  def testBuildBotWithDebugOption(self):
    """Test that --debug option overrides --buildbot option."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', '--debug',
            self._X86_PREFLIGHT]
    (options, args) = self.parser.parse_args(args=args)
    self.assertEquals(options.debug, True)
    self.assertEquals(options.buildbot, True)

  def testNoClobberConfirmationForBuildBotBuilds(self):
    """Test that we don't ask for clobber confirmation for --buildbot runs."""
    self.mox.StubOutWithMock(cbuildbot, '_GetInput')
    self.mox.ReplayAll()
    cbuildbot._ValidateClobber(self._BUILD_ROOT, buildbot=True)
    self.mox.VerifyAll()

  def testValidateClobberUserDeclines_1(self):
    """Test case where user declines in prompt."""
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cbuildbot, '_GetInput')
    self.mox.StubOutWithMock(sys, 'exit')

    os.path.exists(self._BUILD_ROOT).AndReturn(True)
    cbuildbot._GetInput(mox.IgnoreArg()).AndReturn('No')
    sys.exit(0)

    self.mox.ReplayAll()
    cbuildbot._ValidateClobber(self._BUILD_ROOT, False)
    self.mox.VerifyAll()

  def testValidateClobberUserDeclines_2(self):
    """Test case where user does not enter the full 'yes' pattern."""
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cbuildbot, '_GetInput')
    self.mox.StubOutWithMock(sys, 'exit')

    os.path.exists(self._BUILD_ROOT).AndReturn(True)
    cbuildbot._GetInput(mox.IgnoreArg()).AndReturn('y')
    sys.exit(0)

    self.mox.ReplayAll()
    cbuildbot._ValidateClobber(self._BUILD_ROOT, False)
    self.mox.VerifyAll()

  def testValidateClobberProtectRunningChromite(self):
    """User should not be clobbering our own source."""
    self.mox.StubOutWithMock(cros_lib, 'Die')
    cwd = os.path.dirname(os.path.realpath(__file__))
    buildroot = os.path.dirname(cwd)
    cros_lib.Die(mox.IgnoreArg()).AndRaise(Exception)
    self.mox.ReplayAll()
    self.assertRaises(Exception, cbuildbot._ValidateClobber, buildroot, False)
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
