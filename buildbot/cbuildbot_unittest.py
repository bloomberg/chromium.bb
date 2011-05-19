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
    self.mox.StubOutWithMock(cbuildbot, '_RunCommandInFileDir')
    output = ('cros/11.1.241.B\n'
                     'cros/master\n'
                     'm/11.1.241.B -> cros/11.1.241.B')
    cbuildbot._RunCommandInFileDir(mox.In('branch')).AndReturn(output)

    self.mox.ReplayAll()
    manifest_branch = cbuildbot._GetManifestBranch()
    self.mox.VerifyAll()
    self.assertEquals(manifest_branch, '11.1.241.B')

  def testGetTrackingBranchSuccess(self):
    """Verify getting the tracking branch from the current branch."""
    self.mox.StubOutWithMock(cbuildbot, '_RunCommandInFileDir')
    (cbuildbot._RunCommandInFileDir(mox.In('symbolic-ref'))
        .AndReturn('refs/heads/tracking'))
    (cbuildbot._RunCommandInFileDir(mox.In('branch.tracking.merge'))
        .AndReturn('refs/heads/master'))

    self.mox.ReplayAll()
    upstream_branch = cbuildbot._GetTrackingBranch()
    self.mox.VerifyAll()
    self.assertEquals(upstream_branch, 'master')

  def testGetTrackingBranchDetachedHeadSuccess(self):
    """Case where repo is on detached head of manifest branch."""
    self.mox.StubOutWithMock(cbuildbot, '_RunCommandInFileDir')
    self.mox.StubOutWithMock(cbuildbot, '_GetManifestBranch')
    error = cros_lib.RunCommandError('error', 'command')

    cbuildbot._RunCommandInFileDir(mox.In('symbolic-ref')).AndRaise(error)
    cbuildbot._GetManifestBranch().AndReturn('master')
    cbuildbot._RunCommandInFileDir(mox.In('rev-parse') and
                                   mox.In('HEAD')).AndReturn('A2564A5B')
    cbuildbot._RunCommandInFileDir(mox.In('rev-parse') and
                                   mox.In('cros/master')).AndReturn('A2564A5B')
    self.mox.ReplayAll()
    upstream_branch = cbuildbot._GetTrackingBranch()
    self.mox.VerifyAll()
    self.assertEquals(upstream_branch, 'master')

  def testGetTrackingBranchDetachedHeadFailure(self):
    """Case where repo is on detached head, but not of manifest branch."""
    self.mox.StubOutWithMock(cbuildbot, '_RunCommandInFileDir')
    self.mox.StubOutWithMock(cbuildbot, '_GetManifestBranch')
    error = cros_lib.RunCommandError('error', 'command')

    cbuildbot._RunCommandInFileDir(mox.In('symbolic-ref')).AndRaise(error)
    cbuildbot._GetManifestBranch().AndReturn('master')
    cbuildbot._RunCommandInFileDir(mox.In('rev-parse') and
                                   mox.In('HEAD')).AndReturn('A2564A5B')
    cbuildbot._RunCommandInFileDir(mox.In('rev-parse') and
                                   mox.In('cros/master')).AndReturn('BD3445CC')

    self.mox.ReplayAll()
    self.assertRaises(cbuildbot.BranchError, cbuildbot._GetTrackingBranch)
    self.mox.VerifyAll()

  def testGetTrackingBranchNoUpstream(self):
    """Case where current branch is not tracking a remote branch."""
    self.mox.StubOutWithMock(cbuildbot, '_RunCommandInFileDir')
    self.mox.StubOutWithMock(cbuildbot, '_GetManifestBranch')
    error = cros_lib.RunCommandError('error', 'command')

    (cbuildbot._RunCommandInFileDir(mox.In('symbolic-ref'))
        .AndReturn('refs/heads/tracking'))
    (cbuildbot._RunCommandInFileDir(mox.In('branch.tracking.merge'))
        .AndRaise(error))
    cbuildbot._GetManifestBranch().AndReturn('master')

    self.mox.ReplayAll()
    self.assertRaises(cbuildbot.BranchError, cbuildbot._GetTrackingBranch)
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
