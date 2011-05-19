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

def _MockRunCommandWithOutput(cmd_item, output):
  """Mock a RunCommand call with a specific command item and output.

  Args:
    cmd_item: cmd element that RunCommand needs to be called with
    output: the output the mock RunCommand will return
  """
  result = cros_lib.CommandResult()
  result.output = output
  cros_lib.RunCommand(mox.In(cmd_item), redirect_stdout=True).AndReturn(result)


class CBuildBotTest(mox.MoxTestBase):

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

  def testNoResultCodeReturned(self):
    """Test a non-error run."""

    self.options.resume = True

    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cbuildbot, 'RunBuildStages')

    os.path.exists(mox.IsA(str)).AndReturn(False)

    cbuildbot.RunBuildStages(self.bot_id,
                             self.options,
                             self.build_config)

    os.path.exists(mox.IsA(str)).AndReturn(False)

    self.mox.ReplayAll()

    cbuildbot.RunEverything(self.bot_id,
                            self.options,
                            self.build_config)

    self.mox.VerifyAll()

  # Verify bug 13035 is fixed.
  def testResultCodeReturned(self):
    """Verify that we return failure exit code on error."""

    self.options.resume = False

    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cbuildbot, 'RunBuildStages')

    os.path.exists(mox.IsA(str)).AndReturn(False)

    cbuildbot.RunBuildStages(self.bot_id,
                             self.options,
                             self.build_config).AndRaise(
                                 Exception('Test Error'))

    self.mox.ReplayAll()

    self.assertRaises(
        SystemExit,
        lambda : cbuildbot.RunEverything(self.bot_id,
                                         self.options,
                                         self.build_config))

    self.mox.VerifyAll()

  def testChromeosOfficialSet(self):
    """Verify that CHROMEOS_OFFICIAL is set correctly."""

    self.build_config['chromeos_official'] = True

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

  def testGetManifestBranch(self):
    """Verify helper function for getting the manifest branch."""
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    output = ('cros/11.1.241.B\n'
                     'cros/master\n'
                     'm/11.1.241.B -> cros/11.1.241.B')
    _MockRunCommandWithOutput('branch', output)

    self.mox.ReplayAll()
    manifest_branch = cbuildbot._GetManifestBranch()
    self.mox.VerifyAll()
    self.assertEquals(manifest_branch, '11.1.241.B')

  def testGetTrackingBranchSuccess(self):
    """Verify getting the tracking branch from the current branch."""
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    _MockRunCommandWithOutput('symbolic-ref', 'refs/heads/tracking')
    _MockRunCommandWithOutput('branch.tracking.merge', 'refs/heads/master')

    self.mox.ReplayAll()
    upstream_branch = cbuildbot._GetTrackingBranch()
    self.mox.VerifyAll()
    self.assertEquals(upstream_branch, 'master')

  def testGetTrackingBranchDetachedHeadSuccess(self):
    """Case where repo is on detached head of manifest branch."""
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    error = cros_lib.RunCommandError('error', 'command')
    cros_lib.RunCommand(mox.In('symbolic-ref'),
                        redirect_stdout=True).AndRaise(error)
    self.mox.StubOutWithMock(cbuildbot, '_GetManifestBranch')
    cbuildbot._GetManifestBranch().AndReturn('master')
    _MockRunCommandWithOutput(r'm/master..', '')

    self.mox.ReplayAll()
    upstream_branch = cbuildbot._GetTrackingBranch()
    self.mox.VerifyAll()
    self.assertEquals(upstream_branch, 'master')

  def testGetTrackingBranchDetachedHeadFailure(self):
    """Case where repo is on detached head, but not of manifest branch."""
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    error = cros_lib.RunCommandError('error', 'command')
    cros_lib.RunCommand(mox.In('symbolic-ref'),
                        redirect_stdout=True).AndRaise(error)
    self.mox.StubOutWithMock(cbuildbot, '_GetManifestBranch')
    cbuildbot._GetManifestBranch().AndReturn('master')
    _MockRunCommandWithOutput('m/master..',
                              'e53373f7125875ed41a3acbc68c0414764c634f3')

    self.mox.ReplayAll()
    self.assertRaises(cbuildbot.BranchError, cbuildbot._GetTrackingBranch)
    self.mox.VerifyAll()

  def testGetTrackingBranchNoUpstream(self):
    """Case where current branch is not tracking a remote branch."""
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    error = cros_lib.RunCommandError('error', 'command')

    _MockRunCommandWithOutput('symbolic-ref', 'refs/heads/tracking')
    cros_lib.RunCommand(mox.In('branch.tracking.merge'),
                        redirect_stdout=True).AndRaise(error)
    self.mox.StubOutWithMock(cbuildbot, '_GetManifestBranch')
    cbuildbot._GetManifestBranch().AndReturn('master')

    self.mox.ReplayAll()
    self.assertRaises(cbuildbot.BranchError, cbuildbot._GetTrackingBranch)
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
