#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains unittests for validation_pool module."""

import logging
import mox
import sys
import unittest
import urllib

import constants
sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import gerrit_helper
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib


# pylint: disable=W0212,R0904
class TestValidationPool(mox.MoxTestBase):
  """Tests methods in validation_pool.ValidationPool."""

  def _TreeStatusFile(self, message, general_state):
    """Returns a file-like object with the status message writtin in it."""
    my_response = self.mox.CreateMockAnything()
    my_response.json = '{"message": "%s", "general_state": "%s"}' % (
        message, general_state)
    return my_response

  def _TreeStatusTestHelper(self, tree_status, general_state, expected_return,
                            retries_500=0):
    """Tests whether we return the correct value based on tree_status."""
    return_status = self._TreeStatusFile(tree_status, general_state)
    self.mox.StubOutWithMock(urllib, 'urlopen')
    status_url = 'https://chromiumos-status.appspot.com/current?format=json'
    for _ in range(retries_500):
      urllib.urlopen(status_url).AndReturn(return_status)
      return_status.getcode().AndReturn(500)

    urllib.urlopen(status_url).AndReturn(return_status)
    return_status.getcode().AndReturn(200)
    return_status.read().AndReturn(return_status.json)
    self.mox.ReplayAll()
    self.assertEqual(validation_pool.ValidationPool._IsTreeOpen(),
                     expected_return)
    self.mox.VerifyAll()

  def testTreeIsOpen(self):
    """Tests that we return True is the tree is open."""
    self._TreeStatusTestHelper('Tree is open (flaky bug on flaky builder)',
                               'open', True)

  def testTreeIsClosed(self):
    """Tests that we return false is the tree is closed."""
    self._TreeStatusTestHelper('Tree is closed (working on a patch)', 'closed',
                               False)

  def testTreeIsThrottled(self):
    """Tests that we return false is the tree is throttled."""
    self._TreeStatusTestHelper('Tree is throttled (waiting to cycle)',
                               'throttled', True)

  def testTreeStatusWithNetworkFailures(self):
    """Checks for non-500 errors.."""
    self._TreeStatusTestHelper('Tree is open (flaky bug on flaky builder)',
                               'open', True, retries_500=2)

  def testSimpleDepApplyPoolIntoRepo(self):
    """Test that we can apply changes correctly and respect deps.

    This tests a simple out-of-order change where change1 depends on change2
    but tries to get applied before change2.  What should happen is that
    we should notice change2 is a dep of change1 and apply it first.
    """
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch1.url = 'fake_url/1'
    patch2.url = 'fake_url/2'
    build_root = 'fakebuildroot'

    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2]

    patch1.GerritDependencies(build_root).AndReturn(['ChangeId2'])
    patch1.PaladinDependencies(build_root).AndReturn([])

    patch2.Apply(build_root, trivial=True)
    patch2.HandleApplied(None, pool.build_log, dryrun=False)
    patch1.Apply(build_root, trivial=True)
    patch1.HandleApplied(None, pool.build_log, dryrun=False)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    self.mox.VerifyAll()

  def testSimpleNoApplyPoolIntoRepo(self):
    """Test that we don't try to apply a change without met dependencies.

    Patch2 is in the validation pool that depends on Patch1 (which is not)
    Nothing should get applied.
    """
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch2.project = 'fake_project'
    patch1.url = 'fake_url/1'
    patch2.url = 'fake_url/2'
    build_root = 'fakebuildroot'

    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch2]
    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)
    pool.gerrit_helper = helper
    patch2.GerritDependencies(build_root).AndReturn(['ChangeId1'])
    patch2.PaladinDependencies(build_root).AndReturn([])
    helper.IsChangeCommitted(patch1.id).AndReturn(False)

    self.mox.ReplayAll()
    self.assertFalse(pool.ApplyPoolIntoRepo(build_root))
    self.mox.VerifyAll()

  def testSimpleDepApplyWhenAlreadySubmitted(self):
    """Test that we apply a change with dependency already committed."""
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch2.project = 'fake_project'
    patch1.url = 'fake_url/1'
    patch2.url = 'fake_url/2'
    build_root = 'fakebuildroot'

    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch2]
    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)
    pool.gerrit_helper = helper
    patch2.GerritDependencies(build_root).AndReturn(['ChangeId1'])
    patch2.PaladinDependencies(build_root).AndReturn([])
    helper.IsChangeCommitted(patch1.id).AndReturn(True)
    patch2.Apply(build_root, trivial=True)
    patch2.HandleApplied(helper, pool.build_log, dryrun=False)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    self.mox.VerifyAll()

  def testSimpleDepFailedApplyPoolIntoRepo(self):
    """Test that can apply changes correctly when one change fails to apply.

    This tests a simple change order where 1 depends on 2 and 1 fails to apply.
    Only 1 should get tried as 2 will abort once it sees that 1 can't be
    applied.  3 with no dependencies should go through fine.

    Since patch1 fails to apply, we should also get a call to handle the
    failure.
    """
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch3 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch4 = self.mox.CreateMock(cros_patch.GerritPatch)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch3.id = 'ChangeId3'
    patch4.id = 'ChangeId4'
    patch1.url = 'fake_url/1'
    patch2.url = 'fake_url/2'
    patch3.url = 'fake_url/3'
    patch4.url = 'fake_url/4'
    build_root = 'fakebuildroot'

    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2, patch3, patch4]
    pool.build_log = 'log'

    patch1.GerritDependencies(build_root).AndReturn([])
    patch1.PaladinDependencies(build_root).AndReturn([])
    patch1.Apply(build_root, trivial=True).AndRaise(
        cros_patch.ApplyPatchException(patch1))

    patch2.GerritDependencies(build_root).AndReturn(['ChangeId1'])
    patch2.PaladinDependencies(build_root).AndReturn([])
    patch3.GerritDependencies(build_root).AndReturn([])
    patch3.PaladinDependencies(build_root).AndReturn([])
    patch3.Apply(build_root, trivial=True)
    patch3.HandleApplied(None, pool.build_log, dryrun=False)

    # This one should be handled later (not where patch1 is handled.
    patch4.GerritDependencies(build_root).AndReturn([])
    patch4.PaladinDependencies(build_root).AndReturn([])
    patch4.Apply(build_root, trivial=True).AndRaise(
        cros_patch.ApplyPatchException(
            patch1,
            patch_type=\
                cros_patch.ApplyPatchException.TYPE_REBASE_TO_PATCH_INFLIGHT))

    patch1.HandleCouldNotApply(None, pool.build_log, dryrun=False)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    self.assertTrue(patch4 in pool.changes_that_failed_to_apply_earlier)
    self.mox.VerifyAll()

  def testSimpleApplyButMissingChangeIDIntoRepo(self):
    """Test that applies changes correctly with a dep with missing changeid."""
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch1.url = 'fake_url/1'
    patch2.url = 'fake_url/2'
    build_root = 'fakebuildroot'

    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2]
    pool.build_log = 'log'

    patch1.GerritDependencies(build_root).AndRaise(
        cros_patch.MissingChangeIDException('Could not find changeid'))

    patch2.GerritDependencies(build_root).AndReturn([])
    patch2.PaladinDependencies(build_root).AndReturn([])
    patch2.Apply(build_root, trivial=True)
    patch2.HandleApplied(None, pool.build_log, dryrun=False)

    patch1.HandleCouldNotApply(None, pool.build_log, dryrun=False)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    self.assertEqual([patch2.id], [x.id for x in pool.changes])
    self.mox.VerifyAll()

  def testMoreComplexDepApplyPoolIntoRepo(self):
    """More complex deps test.

    This tests a total of 2 change chains where the first change we see
    only has a partial chain with the 3rd change having the whole chain i.e.
    1->2, 3->1->2, 4->nothing.  Since we get these in the order 1,2,3,4 the
    order we should apply is 2,1,3,4.

    This test also checks the patch order to verify that Apply re-orders
    correctly based on the chain.
    """
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch3 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch4 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch5 = self.mox.CreateMock(cros_patch.GerritPatch)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch3.id = 'ChangeId3'
    patch4.id = 'ChangeId4'
    patch5.id = 'ChangeId5'
    patch1.url = 'fake_url/1'
    patch2.url = 'fake_url/2'
    patch3.url = 'fake_url/3'
    patch4.url = 'fake_url/4'
    patch5.url = 'fake_url/5'

    build_root = 'fakebuildroot'

    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2, patch3, patch4, patch5]

    patch1.GerritDependencies(build_root).AndReturn(['ChangeId2'])
    patch1.PaladinDependencies(build_root).AndReturn([])
    patch3.GerritDependencies(build_root).AndReturn(['ChangeId1', 'ChangeId2'])
    patch3.PaladinDependencies(build_root).AndReturn([])
    patch4.GerritDependencies(build_root).AndReturn([])
    patch4.PaladinDependencies(build_root).AndReturn(['ChangeId5'])

    patch2.Apply(build_root, trivial=True)
    patch2.HandleApplied(None, pool.build_log, dryrun=False)
    patch1.Apply(build_root, trivial=True)
    patch1.HandleApplied(None, pool.build_log, dryrun=False)
    patch3.Apply(build_root, trivial=True)
    patch3.HandleApplied(None, pool.build_log, dryrun=False)
    patch5.Apply(build_root, trivial=True)
    patch5.HandleApplied(None, pool.build_log, dryrun=False)
    patch4.Apply(build_root, trivial=True)
    patch4.HandleApplied(None, pool.build_log, dryrun=False)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    # Check order.
    self.assertEquals([x.id for x in pool.changes],
                      [y.id for y in [patch2, patch1, patch3, patch5, patch4]])
    self.mox.VerifyAll()

  def testNoDepsApplyPoolIntoRepo(self):
    """Simple apply of two changes with no dependent CL's."""
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch1.url = 'fake_url/1'
    patch2.url = 'fake_url/2'
    build_root = 'fakebuildroot'

    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2]

    patch1.GerritDependencies(build_root).AndReturn([])
    patch1.PaladinDependencies(build_root).AndReturn([])
    patch2.GerritDependencies(build_root).AndReturn([])
    patch2.PaladinDependencies(build_root).AndReturn([])

    patch1.Apply(build_root, trivial=True)
    patch1.HandleApplied(None, pool.build_log, dryrun=False)

    patch2.Apply(build_root, trivial=True)
    patch2.HandleApplied(None, pool.build_log, dryrun=False)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    self.mox.VerifyAll()

  def testSubmitPoolWithSomeFailures(self):
    """Tests submitting a pool when some changes fail to be submitted.

    Tests what happens when we try to submit 3 patches with 2 patches failing
    to submit correctly (one with submit failure and the other not showing up
    as submitted in Gerrit.
    """
    self.mox.StubOutWithMock(validation_pool.ValidationPool, '_IsTreeOpen')
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch3 = self.mox.CreateMock(cros_patch.GerritPatch)
    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch3.id = 'ChangeId3'
    build_root = 'fakebuildroot'

    validation_pool.ValidationPool._IsTreeOpen().AndReturn(True)
    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2, patch3]
    pool.gerrit_helper = helper
    pool.dryrun = False

    patch1.Submit(helper, dryrun=False)
    helper.IsChangeCommitted(patch1.id).AndReturn(False)
    patch1.HandleCouldNotSubmit(helper, pool.build_log, dryrun=False)
    patch2.Submit(helper, dryrun=False).AndRaise(
        cros_build_lib.RunCommandError('Failed to submit', 'cmd', 1))
    patch2.HandleCouldNotSubmit(helper, pool.build_log, dryrun=False)
    patch3.Submit(helper, dryrun=False)
    helper.IsChangeCommitted(patch3.id).AndReturn(True)

    self.mox.ReplayAll()
    self.assertRaises(validation_pool.FailedToSubmitAllChangesException,
                      validation_pool.ValidationPool.SubmitPool, (pool))
    self.mox.VerifyAll()

  def testSimpleSubmitPool(self):
    """Tests the ability to submit a list of changes."""
    self.mox.StubOutWithMock(validation_pool.ValidationPool, '_IsTreeOpen')
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)
    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    build_root = 'fakebuildroot'

    validation_pool.ValidationPool._IsTreeOpen().AndReturn(True)
    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2]
    pool.gerrit_helper = helper
    pool.dryrun = False

    patch1.Submit(helper, dryrun=False)
    helper.IsChangeCommitted(patch1.id).AndReturn(True)
    patch2.Submit(helper, dryrun=False)
    helper.IsChangeCommitted(patch2.id).AndReturn(True)

    self.mox.ReplayAll()
    pool.SubmitPool()
    self.mox.VerifyAll()

  def testSubmitNonManifestChanges(self):
    """Simple test to make sure we can submit non-manifest changes."""
    self.mox.StubOutWithMock(validation_pool.ValidationPool, '_IsTreeOpen')
    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)
    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    build_root = 'fakebuildroot'

    validation_pool.ValidationPool._IsTreeOpen().AndReturn(True)
    pool = validation_pool.ValidationPool(False, 1, 'build_name', True, False)
    pool.non_manifest_changes = [patch1, patch2]
    pool.gerrit_helper = helper
    pool.dryrun = False

    patch1.Submit(helper, dryrun=False)
    helper.IsChangeCommitted(patch1.id).AndReturn(True)
    patch2.Submit(helper, dryrun=False)
    helper.IsChangeCommitted(patch2.id).AndReturn(True)

    self.mox.ReplayAll()
    pool.SubmitNonManifestChanges()
    self.mox.VerifyAll()


if __name__ == '__main__':
  logging_format = '%(asctime)s - %(filename)s - %(levelname)-8s: %(message)s'
  date_format = '%H:%M:%S'
  logging.basicConfig(level=logging.DEBUG, format=logging_format,

                      datefmt=date_format)
  unittest.main()
