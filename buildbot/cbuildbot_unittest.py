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
    self.options.remote_test_status = False

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





if __name__ == '__main__':
  unittest.main()
