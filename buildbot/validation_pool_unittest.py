#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands.  Needs to be run inside of chroot for mox."""

import mox
import sys
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib as cros_lib


class GerritServerTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    results = (
        '{"project":"chromiumos/platform/init","branch":"master",'
        '"id":"Iee5c89d929f1850d7d4e1a4ff5f21adda800025e",'
        '"number":"1111",'
        '"subject":"init commit",'
        '"owner":{"name":"Init master","email":"init@chromium.org"},'
        '"url":"http://gerrit.chromium.org/gerrit/1111",'
        '"lastUpdated":1311024429,'
        '"sortKey":"00166e8700001051",'
        '"open":true,"'
        'status":"NEW"}'
        '\n'
        '{"project":"chromiumos/chromite","branch":"master",'
        '"id":"Iee5c89d929f1850d7d4e1a4ff5f21adda800025f",'
        '"number":"1112",'
        '"subject":"chromite commit",'
        '"owner":{"name":"Chromite Master","email":"chromite@chromium.org"},'
        '"url":"http://gerrit.chromium.org/gerrit/1112",'
        '"lastUpdated":1311024529,'
        '"sortKey":"00166e8700001052",'
        '"open":true,"'
        'status":"NEW"}'
        )

    self.results = results

  def testParseFakeResults(self):
    """Parses our own fake gerrit query results to verify we parse correctly."""
    fake_array = ['1111', '1112']
    fake_result = self.mox.CreateMock(cros_lib.CommandResult)
    fake_result.output = self.results
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    self.mox.StubOutWithMock(cros_patch, 'GetGerritPatchInfo')
    cros_lib.RunCommand(mox.In('gerrit.chromium.org'),
                        redirect_stdout=True).AndReturn(fake_result)
    cros_patch.GetGerritPatchInfo(fake_array).AndReturn(fake_array)
    self.mox.ReplayAll()
    server = validation_pool._GerritServer('master', False)
    changes = server.GrabChangesReadyForCommit()
    self.assertEqual(changes, ['1111', '1112'])
    self.mox.VerifyAll()

  def testParseFakeResultsWithInternalURL(self):
    """Parses our own fake gerrit query results but sets internal bit."""
    fake_array = ['*1111', '*1112']
    fake_result = self.mox.CreateMock(cros_lib.CommandResult)
    fake_result.output = self.results
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    self.mox.StubOutWithMock(cros_patch, 'GetGerritPatchInfo')
    cros_lib.RunCommand(mox.In('gerrit-int.chromium.org'),
                        redirect_stdout=True).AndReturn(fake_result)
    cros_patch.GetGerritPatchInfo(fake_array).AndReturn(fake_array)
    self.mox.ReplayAll()
    server = validation_pool._GerritServer('master', True)
    changes = server.GrabChangesReadyForCommit()
    self.assertEqual(changes, fake_array)
    self.mox.VerifyAll()

  def _PrintChanges(self, changes):
    """Deep print of an array of changes."""
    for change in changes:
      print change

  def testRealCommandWorks(self):
    """This is just a sanity test that the command is valid.

    Runs the command and prints out the changes.  Should not throw an exception.
    """
    server = validation_pool._GerritServer('master', False)
    changes = server.GrabChangesReadyForCommit()
    self._PrintChanges(changes)

  def testInternalCommandWorks(self):
    """This is just a sanity test that the internal command is valid.

    Runs the command and prints out the changes.  Should not throw an exception.
    """
    server = validation_pool._GerritServer('master', True)
    changes = server.GrabChangesReadyForCommit()
    self._PrintChanges(changes)


if __name__ == '__main__':
  unittest.main()
