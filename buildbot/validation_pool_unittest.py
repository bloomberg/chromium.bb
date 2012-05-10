#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains unittests for validation_pool module."""

import logging
import mox
import os
import pickle
import sys
import time
import unittest
import urllib
import copy
import itertools

import constants
sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.buildbot import gerrit_helper
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import patch_unittest
from chromite.buildbot import repository
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib

_CountingSource = itertools.count()

def GetTestJson(change_id=None):
  """Get usable fake Gerrit patch json data

  Args:
    change_id: If given, force this ChangeId
  """
  data = copy.deepcopy(patch_unittest.FAKE_PATCH_JSON)
  if change_id is not None:
    data['id'] = str(change_id)
  return data


# pylint: disable=W0212,R0904
class TestValidationPool(mox.MoxTestBase):
  """Tests methods in validation_pool.ValidationPool."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.mox.StubOutWithMock(validation_pool, '_RunCommand')
    self.mox.StubOutWithMock(time, 'sleep')
    self.mox.StubOutWithMock(validation_pool.ValidationPool, '_IsTreeOpen')
    # Supress all gerrit access; having this occur is generally a sign
    # the code is either misbehaving, or the tests are bad.
    self.mox.StubOutWithMock(gerrit_helper.GerritHelper, 'Query')

  def MockPatch(self, change_id, patch_number=None):
    patch = self.mox.CreateMock(cros_patch.GerritPatch)

    patch.internal = False
    patch.change_id = patch.id = 'ChangeId%i' % (change_id,)
    patch.gerrit_number = change_id
    patch.patch_number = (patch_number if patch_number is not None else
                          _CountingSource.next())
    patch.url = 'fake_url/%i' % (change_id,)
    patch.apply_error_message = None
    patch.project = 'chromiumos/chromite'
    return patch

  def GetPool(self, *args, **kwds):
    kwds.setdefault('helper_pool', validation_pool.HelperPool.SimpleCreate())
    pool = validation_pool.ValidationPool(*args, **kwds)
    self.mox.StubOutWithMock(pool, '_SendNotification')
    self.mox.StubOutWithMock(gerrit_helper.GerritHelper, '_SqlQuery')
    self.mox.StubOutWithMock(gerrit_helper.GerritHelper,
                             'FindContentMergingProjects')
    return pool

  @staticmethod
  def SetPoolsContentMergingProjects(pool, *projects):
    gerrit_helper.GerritHelper.FindContentMergingProjects().AndReturn(
        frozenset(projects))

  def testSimpleDepApplyPoolIntoRepo(self):
    """Test that we can apply changes correctly and respect deps.

    This tests a simple out-of-order change where change1 depends on change2
    but tries to get applied before change2.  What should happen is that
    we should notice change2 is a dep of change1 and apply it first.
    """
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)

    build_root = 'fakebuildroot'

    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2]
    self.SetPoolsContentMergingProjects(pool)

    patch1.GerritDependencies(build_root).AndReturn(['ChangeId2'])
    patch1.PaladinDependencies(build_root).AndReturn([])

    patch2.Apply(build_root, trivial=True)
    pool.HandleApplied(patch2)
    patch1.Apply(build_root, trivial=True)
    pool.HandleApplied(patch1)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    self.mox.VerifyAll()

  def testSimpleNoApplyPoolIntoRepo(self):
    """Test that we don't try to apply a change without met dependencies.

    Patch2 is in the validation pool that depends on Patch1 (which is not)
    Nothing should get applied.
    """
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    patch2.project = 'fake_project'
    build_root = 'fakebuildroot'

    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)
    helper_pool = validation_pool.HelperPool(external=helper)
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name',
                        True, False, helper_pool=helper_pool)
    pool.changes = [patch2]

    patch2.GerritDependencies(build_root).AndReturn(['ChangeId1'])
    patch2.PaladinDependencies(build_root).AndReturn([])
    helper.IsChangeCommitted(patch1.id, must_match=False).AndReturn(False)
    pool._SendNotification(patch2, mox.StrContains('dependent change'))
    helper.RemoveCommitReady(patch2, dryrun=False)

    self.mox.ReplayAll()
    self.assertFalse(pool.ApplyPoolIntoRepo(build_root))
    self.mox.VerifyAll()

  def testSimpleDepApplyWhenAlreadySubmitted(self):
    """Test that we apply a change with dependency already committed."""
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    patch2.project = 'fake_project'
    build_root = 'fakebuildroot'

    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    pool.changes = [patch2]
    patch2.project = '3way-project'
    self.SetPoolsContentMergingProjects(pool, '3way-project')

    self.mox.StubOutWithMock(gerrit_helper.GerritHelper, 'IsChangeCommitted')
    gerrit_helper.GerritHelper.IsChangeCommitted(
        patch1.id, must_match=False).AndReturn(True)

    patch2.GerritDependencies(build_root).AndReturn(['ChangeId1'])
    patch2.PaladinDependencies(build_root).AndReturn([])
    patch2.Apply(build_root, trivial=False)
    pool.HandleApplied(patch2)

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
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    patch3 = self.MockPatch(3)
    patch4 = self.MockPatch(4)
    build_root = 'fakebuildroot'

    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2, patch3, patch4]
    self.mox.StubOutWithMock(gerrit_helper.GerritHelper, 'RemoveCommitReady')
    self.SetPoolsContentMergingProjects(pool)
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
    pool.HandleApplied(patch3)

    # This one should be handled later (not where patch1 is handled.
    patch4.GerritDependencies(build_root).AndReturn([])
    patch4.PaladinDependencies(build_root).AndReturn([])
    patch4.Apply(build_root, trivial=True).AndRaise(
        cros_patch.ApplyPatchException(patch1, inflight=True))

    validation_pool.MarkChangeFailedToT(patch1)
    pool.HandleCouldNotApply(patch1)

    self.mox.ReplayAll()
    # TODO(ferringb): remove the need for this.
    # Reset before re-running so the error messages don't persist; they're
    # currently stored on the instances themselves, although that'll be
    # rectified soon enough
    for patch in pool.changes:
      patch.apply_error_message = None
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    self.assertTrue(patch4 in pool.changes_that_failed_to_apply_earlier)
    self.mox.VerifyAll()

  def testSimpleApplyButMissingChangeIDIntoRepo(self):
    """Test that applies changes correctly with a dep with missing changeid."""
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    build_root = 'fakebuildroot'

    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    self.mox.StubOutWithMock(pool, 'HandleCouldNotApply')

    pool.changes = [patch1, patch2]
    pool.build_log = 'log'
    self.SetPoolsContentMergingProjects(pool)

    patch1.GerritDependencies(build_root).AndRaise(
        cros_patch.MissingChangeIDException('Could not find changeid'))

    patch2.GerritDependencies(build_root).AndReturn([])
    patch2.PaladinDependencies(build_root).AndReturn([])
    patch2.Apply(build_root, trivial=True)

    pool.HandleApplied(patch2)
    pool.HandleCouldNotApply(patch1)

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
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    patch3 = self.MockPatch(3)
    patch4 = self.MockPatch(4)
    patch5 = self.MockPatch(5)

    build_root = 'fakebuildroot'

    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2, patch3, patch4, patch5]

    self.SetPoolsContentMergingProjects(pool)

    patch1.GerritDependencies(build_root).AndReturn(['ChangeId2'])
    patch1.PaladinDependencies(build_root).AndReturn([])
    patch3.GerritDependencies(build_root).AndReturn(['ChangeId1', 'ChangeId2'])
    patch3.PaladinDependencies(build_root).AndReturn([])
    patch4.GerritDependencies(build_root).AndReturn([])
    patch4.PaladinDependencies(build_root).AndReturn(['ChangeId5'])

    patch2.Apply(build_root, trivial=True)
    pool.HandleApplied(patch2)
    patch1.Apply(build_root, trivial=True)
    pool.HandleApplied(patch1)
    patch3.Apply(build_root, trivial=True)
    pool.HandleApplied(patch3)
    patch5.Apply(build_root, trivial=True)
    pool.HandleApplied(patch5)
    patch4.Apply(build_root, trivial=True)
    pool.HandleApplied(patch4)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    # Check order.
    self.assertEquals([x.id for x in pool.changes],
                      [y.id for y in [patch2, patch1, patch3, patch5, patch4]])
    self.mox.VerifyAll()

  def testNoDepsApplyPoolIntoRepo(self):
    """Simple apply of two changes with no dependent CL's."""
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    build_root = 'fakebuildroot'

    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    pool.changes = [patch1, patch2]
    self.SetPoolsContentMergingProjects(pool)

    patch1.GerritDependencies(build_root).AndReturn([])
    patch1.PaladinDependencies(build_root).AndReturn([])
    patch2.GerritDependencies(build_root).AndReturn([])
    patch2.PaladinDependencies(build_root).AndReturn([])

    patch1.Apply(build_root, trivial=True)
    pool.HandleApplied(patch1)

    patch2.Apply(build_root, trivial=True)
    pool.HandleApplied(patch2)

    self.mox.ReplayAll()
    self.assertTrue(pool.ApplyPoolIntoRepo(build_root))
    self.mox.VerifyAll()

  def testSubmitPoolWithSomeFailures(self):
    """Tests submitting a pool when some changes fail to be submitted.

    Tests what happens when we try to submit 3 patches with 2 patches failing
    to submit correctly (one with submit failure and the other not showing up
    as submitted in Gerrit.
    """
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    patch3 = self.MockPatch(3)

    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)

    build_root = 'fakebuildroot'

    validation_pool.ValidationPool._IsTreeOpen().AndReturn(True)
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False,
                        helper_pool=validation_pool.HelperPool(external=helper,
                                                               internal=None))
    pool.changes = [patch1, patch2, patch3]
    pool.dryrun = False

    self.mox.StubOutWithMock(pool, 'SubmitChange')
    self.mox.StubOutWithMock(helper, 'RemoveCommitReady')
    pool.SubmitChange(patch1)
    helper.IsChangeCommitted(str(patch1.gerrit_number), False).AndReturn(False)
    pool.HandleCouldNotSubmit(patch1)
    result = cros_build_lib.CommandResult(cmd='cmd', returncode=1)
    pool.SubmitChange(patch2).AndRaise(
        cros_build_lib.RunCommandError('Failed to submit', result))
    pool.HandleCouldNotSubmit(patch2)
    pool.SubmitChange(patch3)
    helper.IsChangeCommitted(str(patch3.gerrit_number), False).AndReturn(True)

    self.mox.ReplayAll()
    self.assertRaises(validation_pool.FailedToSubmitAllChangesException,
                      validation_pool.ValidationPool.SubmitPool, (pool))
    self.mox.VerifyAll()

  def testSimpleSubmitPool(self):
    """Tests the ability to submit a list of changes."""
    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)

    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    build_root = 'fakebuildroot'

    validation_pool.ValidationPool._IsTreeOpen().AndReturn(True)
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False,
                        helper_pool=validation_pool.HelperPool(external=helper,
                                                               internal=None))
    pool.changes = [patch1, patch2]
    pool.dryrun = False

    self.mox.StubOutWithMock(pool, 'SubmitChange')
    pool.SubmitChange(patch1)
    helper.IsChangeCommitted(str(patch1.gerrit_number), False).AndReturn(True)
    pool.SubmitChange(patch2)
    helper.IsChangeCommitted(str(patch2.gerrit_number), False).AndReturn(True)

    self.mox.ReplayAll()
    pool.SubmitPool()
    self.mox.VerifyAll()

  def testSubmitNonManifestChanges(self):
    """Simple test to make sure we can submit non-manifest changes."""
    patch1 = self.MockPatch(1)
    patch2 = self.MockPatch(2)
    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)
    build_root = 'fakebuildroot'

    validation_pool.ValidationPool._IsTreeOpen().AndReturn(True)
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False,
                        helper_pool=validation_pool.HelperPool(external=helper,
                                                               internal=None))
    pool.non_manifest_changes = [patch1, patch2]
    pool.dryrun = False

    self.mox.StubOutWithMock(pool, 'SubmitChange')
    pool.SubmitChange(patch1)
    helper.IsChangeCommitted(str(patch1.gerrit_number), False).AndReturn(True)
    pool.SubmitChange(patch2)
    helper.IsChangeCommitted(str(patch2.gerrit_number), False).AndReturn(True)

    self.mox.ReplayAll()
    pool.SubmitNonManifestChanges()
    self.mox.VerifyAll()

  def testGerritSubmit(self):
    """Tests submission review string looks correct."""
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)

    my_patch = cros_patch.GerritPatch(GetTestJson(), False)
    validation_pool._RunCommand(
        'ssh -p 29418 gerrit.chromium.org gerrit review '
        '--submit 1112,2'.split(), False).AndReturn(None)
    self.mox.ReplayAll()
    pool.SubmitChange(my_patch)
    self.mox.VerifyAll()

  def testGerritHandleApplied(self):
    """Tests review string looks correct."""
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)

    my_patch = cros_patch.GerritPatch(GetTestJson(), False)
    pool._SendNotification(my_patch, mox.IgnoreArg())

    self.mox.ReplayAll()
    pool.HandleApplied(my_patch)
    self.mox.VerifyAll()

  def testGerritHandleApplyError(self):
    """Tests review string looks correct."""
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    gerrit_helper.GerritHelper._SqlQuery(
        mox.IgnoreArg(), dryrun=mox.IgnoreArg(),
        is_command=True).AndReturn(None)

    my_patch = cros_patch.GerritPatch(GetTestJson(), False)
    pool._SendNotification(my_patch, mox.IgnoreArg())

    self.mox.ReplayAll()
    pool.HandleCouldNotApply(my_patch)
    self.mox.VerifyAll()

  def testGerritHandleSubmitError(self):
    """Tests review string looks correct."""
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    gerrit_helper.GerritHelper._SqlQuery(
        mox.IgnoreArg(), dryrun=mox.IgnoreArg(),
        is_command=True).AndReturn(None)

    my_patch = cros_patch.GerritPatch(GetTestJson(), False)
    pool._SendNotification(my_patch, mox.IgnoreArg())

    self.mox.ReplayAll()
    pool.HandleCouldNotSubmit(my_patch)
    self.mox.VerifyAll()

  def testGerritHandleVerifyError(self):
    """Tests review string looks correct."""
    pool = self.GetPool(constants.PUBLIC_OVERLAYS, 1, 'build_name', True, False)
    gerrit_helper.GerritHelper._SqlQuery(
        mox.IgnoreArg(), dryrun=mox.IgnoreArg(),
        is_command=True).AndReturn(None)

    my_patch = cros_patch.GerritPatch(GetTestJson(), False)
    pool._SendNotification(my_patch, mox.IgnoreArg())

    self.mox.ReplayAll()
    pool.HandleCouldNotVerify(my_patch)
    self.mox.VerifyAll()


# pylint: disable=W0212,R0904
class TestTreeStatus(mox.MoxTestBase):
  """Tests methods in validation_pool.ValidationPool."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.mox.StubOutWithMock(validation_pool, '_RunCommand')
    self.mox.StubOutWithMock(time, 'sleep')

  def _TreeStatusFile(self, message, general_state):
    """Returns a file-like object with the status message writtin in it."""
    my_response = self.mox.CreateMockAnything()
    my_response.json = '{"message": "%s", "general_state": "%s"}' % (
        message, general_state)
    return my_response

  def _TreeStatusTestHelper(self, tree_status, general_state, expected_return,
                            retries_500=0, max_timeout=0):
    """Tests whether we return the correct value based on tree_status."""
    return_status = self._TreeStatusFile(tree_status, general_state)
    self.mox.StubOutWithMock(urllib, 'urlopen')
    status_url = 'https://chromiumos-status.appspot.com/current?format=json'
    backoff = 1
    for attempt in range(retries_500):
      urllib.urlopen(status_url).AndReturn(return_status)
      return_status.getcode().AndReturn(500)
      time.sleep(backoff)
      backoff *= 2

    urllib.urlopen(status_url).MultipleTimes().AndReturn(return_status)
    if expected_return == False:
      self.mox.StubOutWithMock(time, 'time')
      time.time().AndReturn(1)
      time.time().AndReturn(1)
      sleep_timeout = min(max(max_timeout / 5, 1), 30)
      x = 0
      while x < max_timeout:
        time.time().AndReturn(x + 1)
        x += sleep_timeout
      time.time().AndReturn(max_timeout + 1)
      time.sleep(mox.IgnoreArg()).MultipleTimes()

    return_status.getcode().MultipleTimes().AndReturn(200)
    return_status.read().MultipleTimes().AndReturn(return_status.json)
    self.mox.ReplayAll()
    self.assertEqual(validation_pool.ValidationPool._IsTreeOpen(max_timeout),
                     expected_return)
    self.mox.VerifyAll()

  def testTreeIsOpen(self):
    """Tests that we return True is the tree is open."""
    self._TreeStatusTestHelper('Tree is open (flaky bug on flaky builder)',
                               'open', True)

  def testTreeIsClosed(self):
    """Tests that we return false is the tree is closed."""
    self._TreeStatusTestHelper('Tree is closed (working on a patch)', 'closed',
                               False, max_timeout=5)

  def testTreeIsOpenWithTimeout(self):
    """Tests that we return True even if we get some failures."""
    self._TreeStatusTestHelper('Tree is open (flaky test)', 'open',
                               True, retries_500=2)

  def testTreeIsThrottled(self):
    """Tests that we return false is the tree is throttled."""
    self._TreeStatusTestHelper('Tree is throttled (waiting to cycle)',
                               'throttled', True)

  def testTreeStatusWithNetworkFailures(self):
    """Checks for non-500 errors.."""
    self._TreeStatusTestHelper('Tree is open (flaky bug on flaky builder)',
                               'open', True, retries_500=2)



class TestPickling(cros_test_lib.TempDirMixin, unittest.TestCase):

  """Tests to validate pickling of ValidationPool, covering CQ's needs"""

  def testSelfCompatibility(self):
    """Verify compatibility of current git HEAD against itself."""
    self._CheckTestData(self._GetTestData())

  def testToTCompatibility(self):
    """Validate that ToT can use our pickles, and that we can use ToT's data."""
    repo = os.path.join(self.tempdir, 'chromite')
    reference = os.path.abspath(__file__)
    reference = os.path.normpath(os.path.join(reference, '../../'))

    repository.CloneGitRepo(repo,
                            '%s/chromiumos/chromite' % constants.GIT_HTTP_URL,
                            reference=reference)

    code = """
import sys
from chromite.buildbot import validation_pool_unittest
if not hasattr(validation_pool_unittest, 'TestPickling'):
  sys.exit(0)
sys.stdout.write(validation_pool_unittest.TestPickling.%s)
"""

    # Verify ToT can take our pickle.
    cros_build_lib.RunCommandCaptureOutput(
        ['python', '-c', code % '_CheckTestData(sys.stdin.read())'],
        cwd=self.tempdir, print_cmd=False,
        input=self._GetTestData())

    # Verify we can handle ToT's pickle.
    ret = cros_build_lib.RunCommandCaptureOutput(
        ['python', '-c', code % '_GetTestData()'],
        cwd=self.tempdir, print_cmd=False)

    self._CheckTestData(ret.output)

  @staticmethod
  def _GetTestData():
    ids = [cros_patch.MakeChangeId() for _ in xrange(3)]
    changes = [cros_patch.GerritPatch(GetTestJson(ids[0]), True)]
    non_os = [cros_patch.GerritPatch(GetTestJson(ids[1]), False)]
    conflicting = [cros_patch.GerritPatch(GetTestJson(ids[2]), True)]
    pool = validation_pool.ValidationPool(
        constants.PUBLIC_OVERLAYS, 1, 'testing', True, True,
        changes=changes, non_os_changes=non_os,
        conflicting_changes=conflicting)
    return pickle.dumps([pool, changes, non_os, conflicting])

  @staticmethod
  def _CheckTestData(data):
    results = pickle.loads(data)
    pool, changes, non_os, conflicting = results
    def _f(source, value):
      assert len(source) == len(value)
      for s_item, v_item in zip(source, value):
        assert s_item.id == v_item.id
        assert s_item.internal == v_item.internal
    _f(pool.changes, changes)
    _f(pool.non_manifest_changes, non_os)
    _f(pool.changes_that_failed_to_apply_earlier, conflicting)
    return ''


if __name__ == '__main__':
  logging_format = '%(asctime)s - %(filename)s - %(levelname)-8s: %(message)s'
  date_format = '%H:%M:%S'
  logging.basicConfig(level=logging.DEBUG, format=logging_format,

                      datefmt=date_format)
  unittest.main()
