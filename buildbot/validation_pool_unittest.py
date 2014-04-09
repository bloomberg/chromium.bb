#!/usr/bin/python

# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains unittests for validation_pool module."""

import contextlib
import copy
import functools
import httplib
import itertools
import mox
import os
import pickle
import sys
import time

import constants
sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import cbuildbot_metadata
from chromite.buildbot import repository
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gerrit
from chromite.lib import gob_util
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.lib import patch as cros_patch
from chromite.lib import patch_unittest
from chromite.lib import timeout_util


import mock


_GetNumber = iter(itertools.count()).next


def GetTestJson(change_id=None):
  """Get usable fake Gerrit patch json data

  Args:
    change_id: If given, force this ChangeId
  """
  data = copy.deepcopy(patch_unittest.FAKE_PATCH_JSON)
  if change_id is not None:
    data['id'] = str(change_id)
  return data


class MockManifest(object):
  """Helper class for Mocking Manifest objects."""

  def __init__(self, path, **kwargs):
    self.root = path
    for key, attr in kwargs.iteritems():
      setattr(self, key, attr)

  def ProjectIsContentMerging(self, _project):
    return False


# pylint: disable=W0212,R0904
class Base(cros_test_lib.MockTestCase):
  """Test case base class with helpers for other test suites."""

  def setUp(self):
    self.patch_mock = None
    self._patch_counter = (itertools.count(1)).next
    self.build_root = 'fakebuildroot'
    self.PatchObject(gob_util, 'CreateHttpConn',
                     side_effect=AssertionError('Test should not contact GoB'))
    self.PatchObject(timeout_util, 'IsTreeOpen', return_value=True)
    self.PatchObject(timeout_util, 'WaitForTreeStatus',
                     return_value=constants.TREE_OPEN)

  def MockPatch(self, change_id=None, patch_number=None, is_merged=False,
                project='chromiumos/chromite', remote=constants.EXTERNAL_REMOTE,
                tracking_branch='refs/heads/master', is_draft=False,
                approvals=()):
    """Helper function to create mock GerritPatch objects."""
    if change_id is None:
      change_id = self._patch_counter()
    gerrit_number = str(change_id)
    change_id = hex(change_id)[2:].rstrip('L').lower()
    change_id = 'I%s' % change_id.rjust(40, '0')
    sha1 = hex(_GetNumber())[2:].rstrip('L').lower().rjust(40, '0')
    patch_number = (patch_number if patch_number is not None else _GetNumber())
    fake_url = 'http://foo/bar'
    if not approvals:
      approvals = [{'type': 'VRIF', 'value': '1', 'grantedOn': 1391733002},
                   {'type': 'CRVW', 'value': '2', 'grantedOn': 1391733002},
                   {'type': 'COMR', 'value': '1', 'grantedOn': 1391733002},]

    current_patch_set = {
      'number': patch_number,
      'revision': sha1,
      'draft': is_draft,
      'approvals': approvals,
    }
    patch_dict = {
      'currentPatchSet': current_patch_set,
      'id': change_id,
      'number': gerrit_number,
      'project': project,
      'branch': tracking_branch,
      'owner': {'email': 'elmer.fudd@chromium.org'},
      'remote': remote,
      'status': 'MERGED' if is_merged else 'NEW',
      'url': '%s/%s' % (fake_url, change_id),
    }

    return cros_patch.GerritPatch(patch_dict, remote, fake_url)

  def GetPatches(self, how_many=1, always_use_list=False, **kwargs):
    """Get a sequential list of patches.

    Args:
      how_many: How many patches to return.
      always_use_list: Whether to use a list for a single item list.
      **kwargs: Keyword arguments for self.MockPatch.
    """
    patches = [self.MockPatch(**kwargs) for _ in xrange(how_many)]
    if self.patch_mock:
      for i, patch in enumerate(patches):
        self.patch_mock.SetGerritDependencies(patch, patches[:i + 1])
    if how_many == 1 and not always_use_list:
      return patches[0]
    return patches


class MoxBase(Base, cros_test_lib.MoxTestCase):
  """Base class for other test suites with numbers mocks patched in."""

  def setUp(self):
    self.mox.StubOutWithMock(validation_pool, '_RunCommand')
    # Suppress all gerrit access; having this occur is generally a sign
    # the code is either misbehaving, or that the tests are bad.
    self.mox.StubOutWithMock(gerrit.GerritHelper, 'Query')
    self.PatchObject(gs.GSContext, 'Cat', side_effect=gs.GSNoSuchKey())
    self.PatchObject(gs.GSContext, 'Copy')
    self.PatchObject(gs.GSContext, 'Exists', return_value=False)
    self.PatchObject(gs.GSCounter, 'Increment')

  def MakeHelper(self, cros_internal=None, cros=None):
    # pylint: disable=W0201
    if cros_internal:
      cros_internal = self.mox.CreateMock(gerrit.GerritHelper)
      cros_internal.version = '2.2'
      cros_internal.remote = constants.INTERNAL_REMOTE
    if cros:
      cros = self.mox.CreateMock(gerrit.GerritHelper)
      cros.remote = constants.EXTERNAL_REMOTE
      cros.version = '2.2'
    return validation_pool.HelperPool(cros_internal=cros_internal,
                                      cros=cros)


class IgnoredStagesTest(Base):
  """Tests for functions that calculate what stages to ignore."""

  def testBadConfigFile(self):
    """Test if we can handle an incorrectly formatted config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path, 'foobar')
      ignored = validation_pool.GetStagesToIgnoreFromConfigFile(path)
      self.assertEqual([], ignored)

  def testMissingConfigFile(self):
    """Test if we can handle a missing config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      ignored = validation_pool.GetStagesToIgnoreFromConfigFile(path)
      self.assertEqual([], ignored)

  def testGoodConfigFile(self):
    """Test if we can handle a good config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path, '[GENERAL]\nignored-stages: bar baz\n')
      ignored = validation_pool.GetStagesToIgnoreFromConfigFile(path)
      self.assertEqual(['bar', 'baz'], ignored)


class TestPatchSeries(MoxBase):
  """Tests resolution and applying logic of validation_pool.ValidationPool."""

  @contextlib.contextmanager
  def _ValidateTransactionCall(self, _changes):
    yield

  def GetPatchSeries(self, helper_pool=None, force_content_merging=False):
    if helper_pool is None:
      helper_pool = self.MakeHelper(cros_internal=True, cros=True)
    series = validation_pool.PatchSeries(self.build_root, helper_pool,
                                         force_content_merging)

    # Suppress transactions.
    series._Transaction = self._ValidateTransactionCall
    series.GetGitRepoForChange = \
        lambda change, **kwargs: os.path.join(self.build_root, change.project)
    series._IsContentMerging = lambda change: False

    return series

  def assertPath(self, _patch, return_value, path):
    self.assertEqual(path, os.path.join(self.build_root, _patch.project))
    if isinstance(return_value, Exception):
      raise return_value
    return return_value

  def SetPatchDeps(self, patch, parents=(), cq=()):
    """Set the dependencies of |patch|.

    Args:
      patch: The patch to process.
      parents: A set of strings to set as parents of |patch|.
      cq: A set of strings to set as paladin dependencies of |patch|.
    """
    patch.GerritDependencies = (
        lambda: [cros_patch.ParsePatchDep(x) for x in parents])
    patch.PaladinDependencies = functools.partial(
        self.assertPath, patch, [cros_patch.ParsePatchDep(x) for x in cq])
    patch.Fetch = functools.partial(
        self.assertPath, patch, patch.sha1)

  def _ValidatePatchApplyManifest(self, value):
    self.assertTrue(isinstance(value, MockManifest))
    self.assertEqual(value.root, self.build_root)
    return True

  def SetPatchApply(self, patch, trivial=True):
    self.mox.StubOutWithMock(patch, 'ApplyAgainstManifest')
    return patch.ApplyAgainstManifest(
        mox.Func(self._ValidatePatchApplyManifest),
        trivial=trivial)

  def assertResults(self, series, changes, applied=(), failed_tot=(),
                    failed_inflight=(), frozen=True, dryrun=False):
    # Convenience; set the content pool as necessary.
    for remote in set(x.remote for x in changes):
      try:
        helper = series._helper_pool.GetHelper(remote)
        series._content_merging_projects.setdefault(helper, frozenset())
      except validation_pool.GerritHelperNotAvailable:
        continue

    manifest = MockManifest(self.build_root)
    result = series.Apply(changes, dryrun=dryrun,
                          frozen=frozen, manifest=manifest)

    _GetIds = lambda seq:[x.id for x in seq]
    _GetFailedIds = lambda seq: _GetIds(x.patch for x in seq)

    applied_result = _GetIds(result[0])
    failed_tot_result, failed_inflight_result = map(_GetFailedIds, result[1:])

    applied = _GetIds(applied)
    failed_tot = _GetIds(failed_tot)
    failed_inflight = _GetIds(failed_inflight)

    self.assertEqual(
        [applied, failed_tot, failed_inflight],
        [applied_result, failed_tot_result, failed_inflight_result])
    return result

  def testApplyWithDeps(self):
    """Test that we can apply changes correctly and respect deps.

    This tests a simple out-of-order change where change1 depends on change2
    but tries to get applied before change2.  What should happen is that
    we should notice change2 is a dep of change1 and apply it first.
    """
    series = self.GetPatchSeries()

    patch1, patch2 = patches = self.GetPatches(2)

    self.SetPatchDeps(patch2)
    self.SetPatchDeps(patch1, [patch2.id])

    self.SetPatchApply(patch2)
    self.SetPatchApply(patch1)

    self.mox.ReplayAll()
    self.assertResults(series, patches, [patch2, patch1])
    self.mox.VerifyAll()

  def testSha1Deps(self):
    """Test that we can apply changes correctly and respect sha1 deps.

    This tests a simple out-of-order change where change1 depends on change2
    but tries to get applied before change2.  What should happen is that
    we should notice change2 is a dep of change1 and apply it first.
    """
    series = self.GetPatchSeries()

    patch1, patch2, patch3 = patches = self.GetPatches(3)
    patch2.change_id = patch2.id = patch2.sha1
    patch3.change_id = patch3.id = '*' + patch3.sha1
    patch3.remote = constants.INTERNAL_REMOTE

    self.SetPatchDeps(patch1, [patch2.sha1])
    self.SetPatchDeps(patch2, ['*%s' % patch3.sha1])
    self.SetPatchDeps(patch3)

    self.SetPatchApply(patch2)
    self.SetPatchApply(patch3)
    self.SetPatchApply(patch1)

    self.mox.ReplayAll()
    self.assertResults(series, patches, [patch3, patch2, patch1])
    self.mox.VerifyAll()

  def testGerritNumberDeps(self):
    """Test that we can apply changes correctly and respect gerrit number deps.

    This tests a simple out-of-order change where change1 depends on change2
    but tries to get applied before change2.  What should happen is that
    we should notice change2 is a dep of change1 and apply it first.
    """
    series = self.GetPatchSeries()

    patch1, patch2, patch3 = patches = self.GetPatches(3)

    self.SetPatchDeps(patch3, cq=[patch1.gerrit_number])
    self.SetPatchDeps(patch2, cq=[patch3.gerrit_number])
    self.SetPatchDeps(patch1, cq=[patch2.id])

    self.SetPatchApply(patch3)
    self.SetPatchApply(patch2)
    self.SetPatchApply(patch1)

    self.mox.ReplayAll()
    self.assertResults(series, patches, patches)
    self.mox.VerifyAll()

  def testGerritLazyMapping(self):
    """Given a patch lacking a gerrit number, via gerrit, map it to that change.

    Literally, this ensures that local patches pushed up- lacking a gerrit
    number- are mapped back to a changeid via asking gerrit for that number,
    then the local matching patch is used if available.
    """
    series = self.GetPatchSeries()

    patch1 = self.MockPatch()
    self.PatchObject(patch1, 'LookupAliases', return_value=[patch1.id])
    patch2 = self.MockPatch(change_id=int(patch1.change_id[1:]))
    patch3 = self.MockPatch()

    self.SetPatchDeps(patch3, cq=[patch2.gerrit_number])
    self.SetPatchDeps(patch2)
    self.SetPatchDeps(patch1)

    self.SetPatchApply(patch1)
    self.SetPatchApply(patch3)

    self._SetQuery(series, patch2, query=patch2.gerrit_number).AndReturn(patch2)

    self.mox.ReplayAll()
    applied = self.assertResults(series, [patch1, patch3], [patch3, patch1])[0]
    self.assertTrue(applied[0] is patch3)
    self.assertTrue(applied[1] is patch1)
    self.mox.VerifyAll()

  def testCrosGerritDeps(self, cros_internal=True):
    """Test that we can apply changes correctly and respect deps.

    This tests a simple out-of-order change where change1 depends on change3
    but tries to get applied before change2.  What should happen is that
    we should notice change2 is a dep of change1 and apply it first.
    """
    helper_pool = self.MakeHelper(cros_internal=cros_internal, cros=True)
    series = self.GetPatchSeries(helper_pool=helper_pool)

    patch1 = self.MockPatch(remote=constants.EXTERNAL_REMOTE)
    patch2 = self.MockPatch(remote=constants.INTERNAL_REMOTE)
    patch3 = self.MockPatch(remote=constants.EXTERNAL_REMOTE)
    patches = [patch1, patch2, patch3]
    applied_patches = patches[::-1] if cros_internal else [patch3, patch1]

    self.SetPatchDeps(patch1, [patch3.id])
    self.SetPatchDeps(patch2)
    self.SetPatchDeps(patch3, cq=[patch2.id])

    if cros_internal:
      self.SetPatchApply(patch2)
    self.SetPatchApply(patch1)
    self.SetPatchApply(patch3)

    self.mox.ReplayAll()
    self.assertResults(series, patches, applied_patches)
    self.mox.VerifyAll()

  def testExternalCrosGerritDeps(self):
    """Test that we exclude internal deps on external trybot."""
    self.testCrosGerritDeps(cros_internal=False)

  @staticmethod
  def _SetQuery(series, change, query=None):
    helper = series._helper_pool.GetHelper(change.remote)
    query = change.id if query is None else query
    return helper.QuerySingleRecord(query, must_match=True)

  def testApplyMissingDep(self):
    """Test that we don't try to apply a change without met dependencies.

    Patch2 is in the validation pool that depends on Patch1 (which is not)
    Nothing should get applied.
    """
    series = self.GetPatchSeries()

    patch1, patch2 = self.GetPatches(2)

    self.SetPatchDeps(patch2, [patch1.id])
    self._SetQuery(series, patch1).AndReturn(patch1)

    self.mox.ReplayAll()
    self.assertResults(series, [patch2],
                       [], [patch2])
    self.mox.VerifyAll()

  def testApplyWithCommittedDeps(self):
    """Test that we apply a change with dependency already committed."""
    series = self.GetPatchSeries()

    # Use for basic commit check.
    patch1 = self.GetPatches(1, is_merged=True)
    patch2 = self.GetPatches(1)

    self.SetPatchDeps(patch2, [patch1.id])
    self._SetQuery(series, patch1).AndReturn(patch1)
    self.SetPatchApply(patch2)

    # Used to ensure that an uncommitted change put in the lookup cache
    # isn't invalidly pulled into the graph...
    patch3, patch4, patch5 = self.GetPatches(3)

    self._SetQuery(series, patch3).AndReturn(patch3)
    self.SetPatchDeps(patch4, [patch3.id])
    self.SetPatchDeps(patch5, [patch3.id])

    self.mox.ReplayAll()
    self.assertResults(series, [patch2, patch4, patch5], [patch2],
                       [patch4, patch5])
    self.mox.VerifyAll()

  def testCyclicalDeps(self):
    """Verify that the machinery handles cycles correctly."""
    series = self.GetPatchSeries()

    patch1, patch2 = patches = self.GetPatches(2)

    self.SetPatchDeps(patch1, [patch1.id])
    self.SetPatchDeps(patch2, cq=[patch1.id])

    self.SetPatchApply(patch2)
    self.SetPatchApply(patch1)

    self.mox.ReplayAll()
    self.assertResults(series, patches, patches[::-1])

  def testApplyPartialFailures(self):
    """Test that can apply changes correctly when one change fails to apply.

    This tests a simple change order where 1 depends on 2 and 1 fails to apply.
    Only 1 should get tried as 2 will abort once it sees that 1 can't be
    applied.  3 with no dependencies should go through fine.

    Since patch1 fails to apply, we should also get a call to handle the
    failure.
    """
    series = self.GetPatchSeries()

    patch1, patch2, patch3, patch4 = patches = self.GetPatches(4)

    self.SetPatchDeps(patch1)
    self.SetPatchDeps(patch2, [patch1.id])
    self.SetPatchDeps(patch3)
    self.SetPatchDeps(patch4)

    self.SetPatchApply(patch1).AndRaise(
        cros_patch.ApplyPatchException(patch1))

    self.SetPatchApply(patch3)
    self.SetPatchApply(patch4).AndRaise(
        cros_patch.ApplyPatchException(patch1, inflight=True))

    self.mox.ReplayAll()
    self.assertResults(series, patches,
                       [patch3], [patch2, patch1], [patch4])
    self.mox.VerifyAll()

  def testComplexApply(self):
    """More complex deps test.

    This tests a total of 2 change chains where the first change we see
    only has a partial chain with the 3rd change having the whole chain i.e.
    1->2, 3->1->2.  Since we get these in the order 1,2,3,4,5 the order we
    should apply is 2,1,3,4,5.

    This test also checks the patch order to verify that Apply re-orders
    correctly based on the chain.
    """
    series = self.GetPatchSeries()

    patch1, patch2, patch3, patch4, patch5 = patches = self.GetPatches(5)

    self.SetPatchDeps(patch1, [patch2.id])
    self.SetPatchDeps(patch2)
    self.SetPatchDeps(patch3, [patch1.id, patch2.id])
    self.SetPatchDeps(patch4, cq=[patch5.id])
    self.SetPatchDeps(patch5)

    for patch in (patch2, patch1, patch3, patch4, patch5):
      self.SetPatchApply(patch)

    self.mox.ReplayAll()
    self.assertResults(
        series, patches, [patch2, patch1, patch3, patch4, patch5])
    self.mox.VerifyAll()

  def testApplyStandalonePatches(self):
    """Simple apply of two changes with no dependent CL's."""
    series = self.GetPatchSeries()

    patches = self.GetPatches(3)

    for patch in patches:
      self.SetPatchDeps(patch)

    for patch in patches:
      self.SetPatchApply(patch)

    self.mox.ReplayAll()
    self.assertResults(series, patches, patches)
    self.mox.VerifyAll()


def MakePool(overlays=constants.PUBLIC_OVERLAYS, build_number=1,
             builder_name='foon', is_master=True, dryrun=True, **kwargs):
  """Helper for creating ValidationPool objects for tests."""
  kwargs.setdefault('changes', [])
  build_root = kwargs.pop('build_root', '/fake_root')

  pool = validation_pool.ValidationPool(
      overlays, build_root, build_number, builder_name, is_master,
      dryrun, **kwargs)
  return pool


class MockPatchSeries(partial_mock.PartialMock):
  """Mock the PatchSeries functions."""
  TARGET = 'chromite.buildbot.validation_pool.PatchSeries'
  ATTRS = ('GetDepsForChange', '_GetGerritPatch', '_LookupHelper')

  def __init__(self):
    partial_mock.PartialMock.__init__(self)
    self.deps = {}
    self.cq_deps = {}

  def SetGerritDependencies(self, patch, deps):
    """Add |deps| to the Gerrit dependencies of |patch|."""
    self.deps[patch] = deps

  def SetCQDependencies(self, patch, deps):
    """Add |deps| to the CQ dependencies of |patch|."""
    self.cq_deps[patch] = deps

  def GetDepsForChange(self, _inst, patch):
    return self.deps.get(patch, []), self.cq_deps.get(patch, [])

  def _GetGerritPatch(self, _inst, dep, **_kwargs):
    return dep

  _LookupHelper = mock.MagicMock()


class TestSubmitChange(MoxBase):
  """Test suite related to submitting changes."""

  def setUp(self):
    self.orig_timeout = validation_pool.SUBMITTED_WAIT_TIMEOUT
    validation_pool.SUBMITTED_WAIT_TIMEOUT = 4

  def tearDown(self):
    validation_pool.SUBMITTED_WAIT_TIMEOUT = self.orig_timeout

  def _TestSubmitChange(self, results):
    """Test submitting a change with the given results."""
    results = [cros_test_lib.EasyAttr(status=r) for r in results]
    change = self.MockPatch(change_id=12345, patch_number=1)
    pool = self.mox.CreateMock(validation_pool.ValidationPool)
    pool.dryrun = False
    pool._metadata = cbuildbot_metadata.CBuildbotMetadata()
    pool._helper_pool = self.mox.CreateMock(validation_pool.HelperPool)
    helper = self.mox.CreateMock(validation_pool.gerrit.GerritHelper)

    # Prepare replay script.
    pool._helper_pool.ForChange(change).AndReturn(helper)
    helper.SubmitChange(change, dryrun=False)
    for result in results:
      helper.QuerySingleRecord(change.gerrit_number).AndReturn(result)
    self.mox.ReplayAll()

    # Verify results.
    retval = validation_pool.ValidationPool._SubmitChange(pool, change)
    self.mox.VerifyAll()
    return retval

  def testSubmitChangeMerged(self):
    """Submit one change to gerrit, status MERGED."""
    self.assertTrue(self._TestSubmitChange(['MERGED']))

  def testSubmitChangeSubmitted(self):
    """Submit one change to gerrit, stuck on SUBMITTED."""
    # The query will be retried 1 more time than query timeout.
    results = ['SUBMITTED' for _i in
               xrange(validation_pool.SUBMITTED_WAIT_TIMEOUT + 1)]
    self.assertTrue(self._TestSubmitChange(results))

  def testSubmitChangeSubmittedToMerged(self):
    """Submit one change to gerrit, status SUBMITTED then MERGED."""
    results = ['SUBMITTED', 'SUBMITTED', 'MERGED']
    self.assertTrue(self._TestSubmitChange(results))

  def testSubmitChangeFailed(self):
    """Submit one change to gerrit, reported back as NEW."""
    self.assertFalse(self._TestSubmitChange(['NEW']))


class ValidationFailureOrTimeout(MoxBase):
  """Tests that HandleValidationFailure and HandleValidationTimeout functions.

  These tests check that HandleValidationTimeout and HandleValidationFailure
  reject (i.e. zero out the CQ field) of the correct number of patches, under
  various circumstances.
  """

  _PATCH_MESSAGE = 'Your patch failed.'
  _BUILD_MESSAGE = 'Your build failed.'

  def setUp(self):
    self._patches = self.GetPatches(3)
    self._pool = MakePool(changes=self._patches)

    self.PatchObject(
        validation_pool.ValidationPool, 'GetCLStatus',
        return_value=validation_pool.ValidationPool.STATUS_PASSED)
    self.PatchObject(
        validation_pool.CalculateSuspects, 'FindSuspects',
        return_value=self._patches)
    self.PatchObject(
        validation_pool.ValidationPool, '_CreateValidationFailureMessage',
        return_value=self._PATCH_MESSAGE)
    self.PatchObject(validation_pool.ValidationPool, 'SendNotification')
    self.PatchObject(validation_pool.ValidationPool, 'RemoveCommitReady')
    self.PatchObject(validation_pool.ValidationPool, 'UpdateCLStatus')
    self.StartPatcher(parallel_unittest.ParallelMock())


  def testPatchesWereRejectedByFailure(self):
    self._pool.HandleValidationFailure([self._BUILD_MESSAGE])
    self.assertEqual(
        len(self._patches), self._pool.RemoveCommitReady.call_count)

  def testPatchesWereRejectedByTimeout(self):
    self._pool.HandleValidationTimeout()
    self.assertEqual(
        len(self._patches), self._pool.RemoveCommitReady.call_count)

  def testNoSuspectsWithFailure(self):
    self.PatchObject(
        validation_pool.CalculateSuspects, 'FindSuspects',
        return_value=[])
    self._pool.HandleValidationFailure([self._BUILD_MESSAGE])
    self.assertEqual(0, self._pool.RemoveCommitReady.call_count)

  def testPreCQ(self):
    self._pool.pre_cq = True
    self._pool.HandleValidationFailure([self._BUILD_MESSAGE])
    self.assertEqual(0, self._pool.RemoveCommitReady.call_count)

  def testPatchesWereNotRejectedByInsaneFailure(self):
    self._pool.HandleValidationFailure([self._BUILD_MESSAGE], sanity=False)
    self.assertEqual(0, self._pool.RemoveCommitReady.call_count)


class TestCoreLogic(MoxBase):
  """Tests resolution and applying logic of validation_pool.ValidationPool."""

  def setUp(self):
    self.mox.StubOutWithMock(validation_pool.PatchSeries, 'Apply')
    self.patch_mock = self.StartPatcher(MockPatchSeries())
    funcs = ['SendNotification', '_SubmitChange']
    for func in funcs:
      self.mox.StubOutWithMock(validation_pool.ValidationPool, func)
    self.PatchObject(validation_pool.ValidationPool, 'ReloadChanges',
                     side_effect=lambda x: x)
    self.StartPatcher(parallel_unittest.ParallelMock())

  def MakePool(self, *args, **kwargs):
    """Helper for creating ValidationPool objects for Mox tests."""
    handlers = kwargs.pop('handlers', False)
    kwargs['build_root'] = self.build_root
    pool = MakePool(*args, **kwargs)
    funcs = ['_HandleApplySuccess', '_HandleApplyFailure',
             '_HandleCouldNotApply', '_HandleCouldNotSubmit']
    if handlers:
      for func in funcs:
        self.mox.StubOutWithMock(pool, func)
    return pool

  def MakeFailure(self, patch, inflight=True):
    return cros_patch.ApplyPatchException(patch, inflight=inflight)

  def GetPool(self, changes, applied=(), tot=(),
              inflight=(), dryrun=True, **kwargs):
    pool = self.MakePool(changes=changes, **kwargs)
    applied = list(applied)
    tot = [self.MakeFailure(x, inflight=False) for x in tot]
    inflight = [self.MakeFailure(x, inflight=True) for x in inflight]
    # pylint: disable=E1120,E1123
    validation_pool.PatchSeries.Apply(
        changes, dryrun=dryrun, manifest=mox.IgnoreArg()
        ).AndReturn((applied, tot, inflight))

    for patch in applied:
      pool._HandleApplySuccess(patch).AndReturn(None)

    if tot:
      pool._HandleApplyFailure(tot).AndReturn(None)

    # We stash this on the pool object so we can reuse it during validation.
    # We could stash this in the test instances, but that would break
    # for any tests that do multiple pool instances.

    pool._test_data = (changes, applied, tot, inflight)

    return pool

  def runApply(self, pool, result):
    self.assertEqual(result, pool.ApplyPoolIntoRepo())
    self.assertEqual(pool.changes, pool._test_data[1])
    failed_inflight = pool.changes_that_failed_to_apply_earlier
    expected_inflight = set(pool._test_data[3])
    # Intersect the results, since it's possible there were results failed
    # results that weren't related to the ApplyPoolIntoRepo call.
    self.assertEqual(set(failed_inflight).intersection(expected_inflight),
                     expected_inflight)

    self.assertEqual(pool.changes, pool._test_data[1])

  def testPatchSeriesInteraction(self):
    """Verify the interaction between PatchSeries and ValidationPool.

    Effectively, this validates data going into PatchSeries, and coming back
    out; verifies the hand off to _Handle* functions, but no deeper.
    """
    patches = self.GetPatches(3)

    apply_pool = self.GetPool(patches, applied=patches, handlers=True)
    all_inflight = self.GetPool(patches, inflight=patches, handlers=True)
    all_tot = self.GetPool(patches, tot=patches, handlers=True)
    mixed = self.GetPool(patches, tot=patches[0:1], inflight=patches[1:2],
                         applied=patches[2:3], handlers=True)

    self.mox.ReplayAll()
    self.runApply(apply_pool, True)
    self.runApply(all_inflight, False)
    self.runApply(all_tot, False)
    self.runApply(mixed, True)
    self.mox.VerifyAll()

  def testHandleApplySuccess(self):
    """Validate steps taken for successfull application."""
    patch = self.GetPatches(1)
    pool = self.MakePool()
    pool.SendNotification(patch, mox.StrContains('has picked up your change'))
    self.mox.ReplayAll()
    pool._HandleApplySuccess(patch)
    self.mox.VerifyAll()

  def testHandleApplyFailure(self):
    failures = [cros_patch.ApplyPatchException(x) for x in self.GetPatches(4)]

    notified_patches = failures[:2]
    unnotified_patches = failures[2:]
    master_pool = self.MakePool(dryrun=False)
    slave_pool = self.MakePool(is_master=False)

    self.mox.StubOutWithMock(gerrit.GerritHelper, 'RemoveCommitReady')

    for failure in notified_patches:
      master_pool.SendNotification(
          failure.patch,
          mox.StrContains('failed to apply your change'),
          failure=mox.IgnoreArg())
      # This pylint suppressin shouldn't be necessary, but pylint is invalidly
      # thinking that the first arg isn't passed in; we suppress it to suppress
      # the pylnt bug.
      # pylint: disable=E1120
      gerrit.GerritHelper.RemoveCommitReady(failure.patch, dryrun=False)

    self.mox.ReplayAll()
    master_pool._HandleApplyFailure(notified_patches)
    slave_pool._HandleApplyFailure(unnotified_patches)
    self.mox.VerifyAll()

  def _setUpSubmit(self):
    pool = self.MakePool(dryrun=False, handlers=True)
    patches = self.GetPatches(3)
    failed = self.GetPatches(3)
    pool.changes = patches[:]
    # While we don't do anything w/ these patches, that's
    # intentional; we're verifying that it isn't submitted
    # if there is a failure.
    pool.changes_that_failed_to_apply_earlier = failed[:]

    return (pool, patches, failed)

  def testSubmitPoolFailures(self):
    pool, patches, _failed = self._setUpSubmit()
    patch1, patch2, patch3 = patches

    pool._SubmitChange(patch1).AndReturn(True)
    pool._SubmitChange(patch2).AndReturn(False)

    pool._HandleCouldNotSubmit(patch2, mox.IgnoreArg()).InAnyOrder()
    pool._HandleCouldNotSubmit(patch3, mox.IgnoreArg()).InAnyOrder()

    self.mox.ReplayAll()
    self.assertRaises(validation_pool.FailedToSubmitAllChangesException,
                      pool.SubmitPool)
    self.mox.VerifyAll()

  def testSubmitPool(self):
    pool, patches, failed = self._setUpSubmit()

    for patch in patches:
      pool._SubmitChange(patch).AndReturn(True)

    pool._HandleApplyFailure(failed)

    self.mox.ReplayAll()
    pool.SubmitPool()
    self.mox.VerifyAll()

  def testSubmitNonManifestChanges(self):
    """Simple test to make sure we can submit non-manifest changes."""
    pool, patches, _failed = self._setUpSubmit()
    pool.non_manifest_changes = patches[:]

    for patch in patches:
      pool._SubmitChange(patch).AndReturn(True)

    self.mox.ReplayAll()
    pool.SubmitNonManifestChanges()
    self.mox.VerifyAll()

  def testUnhandledExceptions(self):
    """Test that CQ doesn't loop due to unhandled Exceptions."""
    pool, patches, _failed = self._setUpSubmit()

    class MyException(Exception):
      """"Unique Exception used for testing."""

    def VerifyCQError(patch, error):
      cq_error = validation_pool.InternalCQError(patch, error.message)
      return str(error) == str(cq_error)

    # pylint: disable=E1120,E1123
    validation_pool.PatchSeries.Apply(
        patches, dryrun=False, manifest=mox.IgnoreArg()).AndRaise(
        MyException)
    errors = [mox.Func(functools.partial(VerifyCQError, x)) for x in patches]
    pool._HandleApplyFailure(errors).AndReturn(None)

    self.mox.ReplayAll()
    self.assertRaises(MyException, pool.ApplyPoolIntoRepo)
    self.mox.VerifyAll()

  def testFilterDependencyErrors(self):
    """Verify that dependency errors are correctly filtered out."""
    failures = [cros_patch.ApplyPatchException(x) for x in self.GetPatches(2)]
    failures += [cros_patch.DependencyError(x, y) for x, y in
                 zip(self.GetPatches(2), failures)]
    failures[0].patch.approval_timestamp = time.time()
    failures[-1].patch.approval_timestamp = time.time()
    self.mox.ReplayAll()
    result = validation_pool.ValidationPool._FilterDependencyErrors(failures)
    self.assertEquals(set(failures[:-1]), set(result))
    self.mox.VerifyAll()

  def testFilterNonCrosProjects(self):
    """Runs through a filter of own manifest and fake changes.

    This test should filter out the tacos/chromite project as its not real.
    """
    base_func = itertools.cycle(['chromiumos', 'chromeos']).next
    patches = self.GetPatches(8)
    for patch in patches:
      patch.project = '%s/%i' % (base_func(), _GetNumber())
      patch.tracking_branch = str(_GetNumber())

    non_cros_patches = self.GetPatches(2)
    for patch in non_cros_patches:
      patch.project = str(_GetNumber())

    filtered_patches = patches[:4]
    allowed_patches = []
    projects = {}
    for idx, patch in enumerate(patches[4:]):
      fails = bool(idx % 2)
      # Vary the revision so we can validate that it checks the branch.
      revision = ('monkeys' if fails
                  else 'refs/heads/%s' % patch.tracking_branch)
      if fails:
        filtered_patches.append(patch)
      else:
        allowed_patches.append(patch)
      projects.setdefault(patch.project, {})['revision'] = revision

    manifest = MockManifest(self.build_root, projects=projects)
    for patch in allowed_patches:
      patch.GetCheckout = lambda *_args, **_kwargs: True
    for patch in filtered_patches:
      patch.GetCheckout = lambda *_args, **_kwargs: False

    self.mox.ReplayAll()
    results = validation_pool.ValidationPool._FilterNonCrosProjects(
        patches + non_cros_patches, manifest)

    def compare(list1, list2):
      mangle = lambda c:(c.id, c.project, c.tracking_branch)
      self.assertEqual(list1, list2,
        msg="Comparison failed:\n list1: %r\n list2: %r"
            % (map(mangle, list1), map(mangle, list2)))

    compare(results[0], allowed_patches)
    compare(results[1], filtered_patches)


class TestPickling(cros_test_lib.TempDirTestCase):
  """Tests to validate pickling of ValidationPool, covering CQ's needs"""

  def testSelfCompatibility(self):
    """Verify compatibility of current git HEAD against itself."""
    self._CheckTestData(self._GetTestData())

  def testToTCompatibility(self):
    """Validate that ToT can use our pickles, and that we can use ToT's data."""
    repo = os.path.join(self.tempdir, 'chromite')
    reference = os.path.abspath(__file__)
    reference = os.path.normpath(os.path.join(reference, '../../'))

    repository.CloneGitRepo(
        repo,
        '%s/chromiumos/chromite' % constants.EXTERNAL_GOB_URL,
        reference=reference)

    code = """
import sys
from chromite.buildbot import validation_pool_unittest
if not hasattr(validation_pool_unittest, 'TestPickling'):
  sys.exit(0)
sys.stdout.write(validation_pool_unittest.TestPickling.%s)
"""

    # Verify ToT can take our pickle.
    cros_build_lib.RunCommand(
        ['python', '-c', code % '_CheckTestData(sys.stdin.read())'],
        cwd=self.tempdir, print_cmd=False, capture_output=True,
        input=self._GetTestData())

    # Verify we can handle ToT's pickle.
    ret = cros_build_lib.RunCommand(
        ['python', '-c', code % '_GetTestData()'],
        cwd=self.tempdir, print_cmd=False, capture_output=True)

    self._CheckTestData(ret.output)

  @staticmethod
  def _GetCrosInternalPatch(patch_info):
    return cros_patch.GerritPatch(
        patch_info,
        constants.INTERNAL_REMOTE,
        constants.INTERNAL_GERRIT_URL)

  @staticmethod
  def _GetCrosPatch(patch_info):
    return cros_patch.GerritPatch(
        patch_info,
        constants.EXTERNAL_REMOTE,
        constants.EXTERNAL_GERRIT_URL)

  @classmethod
  def _GetTestData(cls):
    ids = [cros_patch.MakeChangeId() for _ in xrange(3)]
    changes = [cls._GetCrosInternalPatch(GetTestJson(ids[0]))]
    non_os = [cls._GetCrosPatch(GetTestJson(ids[1]))]
    conflicting = [cls._GetCrosInternalPatch(GetTestJson(ids[2]))]
    conflicting = [cros_patch.PatchException(x)for x in conflicting]
    pool = validation_pool.ValidationPool(
        constants.PUBLIC_OVERLAYS,
        '/fake/pathway', 1,
        'testing', True, True,
        changes=changes, non_os_changes=non_os,
        conflicting_changes=conflicting)
    return pickle.dumps([pool, changes, non_os, conflicting])

  @staticmethod
  def _CheckTestData(data):
    results = pickle.loads(data)
    pool, changes, non_os, conflicting = results
    def _f(source, value, getter=None):
      if getter is None:
        getter = lambda x: x
      assert len(source) == len(value)
      for s_item, v_item in zip(source, value):
        assert getter(s_item).id == getter(v_item).id
        assert getter(s_item).remote == getter(v_item).remote
    _f(pool.changes, changes)
    _f(pool.non_manifest_changes, non_os)
    _f(pool.changes_that_failed_to_apply_earlier, conflicting,
       getter=lambda s:getattr(s, 'patch', s))
    return ''


class TestFindSuspects(MoxBase):
  """Tests validation_pool.ValidationPool._FindSuspects"""

  def setUp(self):
    overlay = 'chromiumos/overlays/chromiumos-overlay'
    self.overlay_patch = self.GetPatches(project=overlay)
    self.power_manager = 'chromiumos/platform/power_manager'
    self.power_manager_pkg = 'chromeos-base/power_manager'
    self.power_manager_patch = self.GetPatches(project=self.power_manager)
    self.kernel = 'chromiumos/third_party/kernel'
    self.kernel_pkg = 'sys-kernel/chromeos-kernel'
    self.kernel_patch = self.GetPatches(project=self.kernel)
    self.secret = 'chromeos/secret'
    self.secret_patch = self.GetPatches(project=self.secret,
                                        remote=constants.INTERNAL_REMOTE)

  @staticmethod
  def _GetBuildFailure(pkg):
    """Create a PackageBuildFailure for the specified |pkg|.

    Args:
      pkg: Package that failed to build.
    """
    ex = cros_build_lib.RunCommandError('foo', cros_build_lib.CommandResult())
    return results_lib.PackageBuildFailure(ex, 'bar', [pkg])

  def _AssertSuspects(self, patches, suspects, pkgs=(), exceptions=(),
                      internal=False):
    """Run _FindSuspects and verify its output.

    Args:
      patches: List of patches to look at.
      suspects: Expected list of suspects returned by _FindSuspects.
      pkgs: List of packages that failed with exceptions in the build.
      exceptions: List of other exceptions that occurred during the build.
      internal: Whether the failures occurred on an internal bot.
    """
    all_exceptions = list(exceptions) + [self._GetBuildFailure(x) for x in pkgs]
    tracebacks = []
    for ex in all_exceptions:
      tracebacks.append(results_lib.RecordedTraceback('Build', 'Build', ex,
                                                      str(ex)))
    message = validation_pool.ValidationFailedMessage(
        'foo bar %r' % tracebacks, tracebacks, internal)
    results = validation_pool.CalculateSuspects.FindSuspects(patches, [message])
    self.assertEquals(set(suspects), results)

  def testFailSameProject(self):
    """Patches to the package that failed should be marked as failing."""
    suspects = [self.kernel_patch]
    patches = suspects + [self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(patches, suspects, [self.kernel_pkg])

  def testFailSameProjectPlusOverlay(self):
    """Patches to the overlay should be marked as failing."""
    suspects = [self.overlay_patch, self.kernel_patch]
    patches = suspects + [self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(patches, suspects, [self.kernel_pkg])

  def testFailUnknownPackage(self):
    """If no patches changed the package, all patches should fail."""
    suspects = [self.overlay_patch, self.power_manager_patch]
    changes = suspects + [self.secret_patch]
    self._AssertSuspects(changes, suspects, [self.kernel_pkg])

  def testFailUnknownException(self):
    """An unknown exception should cause all [public] patches to fail."""
    suspects = [self.kernel_patch, self.power_manager_patch]
    changes = suspects + [self.secret_patch]
    self._AssertSuspects(changes, suspects, exceptions=[Exception('foo bar')])

  def testFailUnknownInternalException(self):
    """An unknown exception should cause all [internal] patches to fail."""
    suspects = [self.kernel_patch, self.power_manager_patch, self.secret_patch]
    self._AssertSuspects(suspects, suspects, exceptions=[Exception('foo bar')],
                         internal=True)

  def testFailUnknownCombo(self):
    """Unknown exceptions should cause all patches to fail.

    Even if there are also build failures that we can explain.
    """
    suspects = [self.kernel_patch, self.power_manager_patch]
    changes = suspects + [self.secret_patch]
    self._AssertSuspects(changes, suspects, [self.kernel_pkg],
                         [Exception('foo bar')])

  def testFailNoExceptions(self):
    """If there are no exceptions, all patches should be failed."""
    suspects = [self.kernel_patch, self.power_manager_patch]
    changes = suspects + [self.secret_patch]
    self._AssertSuspects(changes, suspects)


class TestCLStatus(MoxBase):
  """Tests methods that get the CL status."""

  def testPrintLinks(self):
    changes = self.GetPatches(3)
    with parallel_unittest.ParallelMock():
      validation_pool.ValidationPool.PrintLinksToChanges(changes)

  def testStatusCache(self):
    validation_pool.ValidationPool._CL_STATUS_CACHE = {}
    changes = self.GetPatches(3)
    with parallel_unittest.ParallelMock():
      validation_pool.ValidationPool.FillCLStatusCache(validation_pool.CQ,
                                                       changes)
      self.assertEqual(len(validation_pool.ValidationPool._CL_STATUS_CACHE), 12)
      validation_pool.ValidationPool.PrintLinksToChanges(changes)
      self.assertEqual(len(validation_pool.ValidationPool._CL_STATUS_CACHE), 12)


class TestCreateValidationFailureMessage(Base):
  """Tests validation_pool.ValidationPool._CreateValidationFailureMessage"""

  def _AssertMessage(self, change, suspects, messages, sanity=True):
    """Call the _CreateValidationFailureMessage method.

    Args:
      change: The change we are commenting on.
      suspects: List of suspected changes.
      messages: List of messages to include in comment.
      sanity: Bool indicating sanity of build, default: True.
    """
    msg = validation_pool.ValidationPool._CreateValidationFailureMessage(
      False, change, set(suspects), messages, sanity=sanity)
    for x in messages:
      self.assertTrue(x in msg)
    return msg

  def testSuspectChange(self):
    """Test case where 1 is the only change and is suspect."""
    patch = self.GetPatches(1)
    self._AssertMessage(patch, [patch], ['%s failed' % patch])

  def testInnocentChange(self):
    """Test case where 1 is innocent."""
    patch1, patch2 = self.GetPatches(2)
    self._AssertMessage(patch1, [patch2], ['%s failed' % patch2])

  def testSuspectChanges(self):
    """Test case where 1 is suspected, but so is 2."""
    patches = self.GetPatches(2)
    self._AssertMessage(patches[0], patches,
                        ['%s and %s failed' % tuple(patches)])

  def testInnocentChangeWithMultipleSuspects(self):
    """Test case where 2 and 3 are suspected."""
    patches = self.GetPatches(3)
    self._AssertMessage(patches[0], patches[1:],
                        ['%s and %s failed' % tuple(patches[1:])])

  def testNoSuspects(self):
    """Test case where there are no suspects."""
    self._AssertMessage(self.GetPatches(1), [], ['Internal error'])

  def testNoMessages(self):
    """Test case where there are no messages."""
    patch1 = self.GetPatches(1)
    self._AssertMessage(patch1, [patch1], [])

  def testInsaneBuild(self):
    patches = self.GetPatches(3)
    self._AssertMessage(
        patches[0], patches, ['sanity check builder',
                              'retry your change automatically'],
        sanity=False)

class TestCreateDisjointTransactions(Base):
  """Test the CreateDisjointTransactions function."""

  def setUp(self):
    self.patch_mock = self.StartPatcher(MockPatchSeries())

  def GetPatches(self, how_many, **kwargs):
    return Base.GetPatches(self, how_many, always_use_list=True, **kwargs)

  def verifyTransactions(self, txns, max_txn_length=None, circular=False):
    """Verify the specified list of transactions are processed correctly.

    Args:
      txns: List of transactions to process.
      max_txn_length: Maximum length of any given transaction. This is passed
        to the CreateDisjointTransactions function.
      circular: Whether the transactions contain circular dependencies.
    """
    remove = self.PatchObject(gerrit.GerritHelper, 'RemoveCommitReady')
    patches = list(itertools.chain.from_iterable(txns))
    expected_plans = txns
    if max_txn_length is not None:
      # When max_txn_length is specified, transactions should be truncated to
      # the specified length, ignoring any remaining patches.
      expected_plans = [txn[:max_txn_length] for txn in txns]

    pool = MakePool(changes=patches)
    plans = pool.CreateDisjointTransactions(None, max_txn_length=max_txn_length)

    # If the dependencies are circular, the order of the patches is not
    # guaranteed, so compare them in sorted order.
    if circular:
      plans = [sorted(plan) for plan in plans]
      expected_plans = [sorted(plan) for plan in expected_plans]

    # Verify the plans match, and that no changes were rejected.
    self.assertEqual(set(map(str, plans)), set(map(str, expected_plans)))
    self.assertEqual(0, remove.call_count)

  def testPlans(self, max_txn_length=None):
    """Verify that independent sets are distinguished."""
    for num in range(0, 5):
      txns = [self.GetPatches(num) for _ in range(0, num)]
      self.verifyTransactions(txns, max_txn_length=max_txn_length)

  def runUnresolvedPlan(self, changes, max_txn_length=None):
    """Helper for testing unresolved plans."""
    notify = self.PatchObject(validation_pool.ValidationPool,
                              'SendNotification')
    remove = self.PatchObject(gerrit.GerritHelper, 'RemoveCommitReady')
    pool = MakePool(changes=changes)
    plans = pool.CreateDisjointTransactions(None, max_txn_length=max_txn_length)
    self.assertEqual(plans, [])
    self.assertEqual(remove.call_count, notify.call_count)
    return remove.call_count

  def testUnresolvedPlan(self):
    """Test plan with old approval_timestamp."""
    changes = self.GetPatches(5)[1:]
    with cros_test_lib.LoggingCapturer():
      call_count = self.runUnresolvedPlan(changes)
    self.assertEqual(4, call_count)

  def testRecentUnresolvedPlan(self):
    """Test plan with recent approval_timestamp."""
    changes = self.GetPatches(5)[1:]
    for change in changes:
      change.approval_timestamp = time.time()
    with cros_test_lib.LoggingCapturer():
      call_count = self.runUnresolvedPlan(changes)
    self.assertEqual(0, call_count)

  def testTruncatedPlan(self):
    """Test that plans can be truncated correctly."""
    # Long lists of patches should be truncated, and we should not see any
    # errors when this happens.
    self.testPlans(max_txn_length=3)

  def testCircularPlans(self):
    """Verify that circular plans are handled correctly."""
    patches = self.GetPatches(5)
    self.patch_mock.SetGerritDependencies(patches[0], [patches[-1]])

    # Verify that all patches can be submitted normally.
    self.verifyTransactions([patches], circular=True)

    # It is not possible to truncate a circular plan. Verify that an error
    # is reported in this case.
    with cros_test_lib.LoggingCapturer():
      call_count = self.runUnresolvedPlan(patches, max_txn_length=3)
    self.assertEqual(5, call_count)


class MockValidationPool(partial_mock.PartialMock):
  """Mock out a ValidationPool instance."""

  TARGET = 'chromite.buildbot.validation_pool.ValidationPool'
  ATTRS = ('ReloadChanges', 'RemoveCommitReady', '_SubmitChange',
           'SendNotification')

  def __init__(self):
    partial_mock.PartialMock.__init__(self)
    self.submit_results = {}
    self.max_submits = None

  def GetSubmittedChanges(self):
    calls = []
    for call in self.patched['_SubmitChange'].call_args_list:
      call_args, _ = call
      calls.append(call_args[1])
    return calls

  def _SubmitChange(self, _inst, change):
    result = self.submit_results.get(change, True)
    if isinstance(result, Exception):
      raise result
    if result and self.max_submits is not None:
      if self.max_submits <= 0:
        return False
      self.max_submits -= 1
    return result

  @classmethod
  def ReloadChanges(cls, changes):
    return changes

  RemoveCommitReady = None
  SendNotification = None


class BaseSubmitPoolTestCase(Base, cros_build_lib_unittest.RunCommandTestCase):
  """Test full ability to submit and reject CL pools."""

  def setUp(self):
    self.pool_mock = self.StartPatcher(MockValidationPool())
    self.patch_mock = self.StartPatcher(MockPatchSeries())
    self.PatchObject(gerrit.GerritHelper, 'QuerySingleRecord')
    self.patches = self.GetPatches(2)

    # By default, don't ignore any errors.
    self.ignores = dict((patch, []) for patch in self.patches)

  def SetUpPatchPool(self, failed_to_apply=False):
    pool = MakePool(changes=self.patches, dryrun=False)
    if failed_to_apply:
      # Create some phony errors and add them to the pool.
      errors = []
      for patch in self.GetPatches(2):
        errors.append(validation_pool.InternalCQError(patch, str('foo')))
      pool.changes_that_failed_to_apply_earlier = errors[:]
    return pool

  def GetTracebacks(self):
    return []

  def SubmitPool(self, submitted=(), rejected=(), **kwargs):
    """Helper function for testing that we can submit a pool successfully.

    Args:
      submitted: List of changes that we expect to be submitted.
      rejected: List of changes that we expect to be rejected.
      **kwargs: Keyword arguments for SetUpPatchPool.
    """
    # self.ignores maps changes to a list of stages to ignore. Use it.
    self.PatchObject(
        validation_pool, 'GetStagesToIgnoreForChange',
        side_effect=lambda _, change: self.ignores[change])

    # Set up our pool and submit the patches.
    pool = self.SetUpPatchPool(**kwargs)
    tracebacks = self.GetTracebacks()
    if tracebacks:
      actually_rejected = sorted(pool.SubmitPartialPool(self.GetTracebacks()))
    else:
      actually_rejected = pool.SubmitChanges(self.patches)

    # Check that the right patches were submitted and rejected.
    self.assertItemsEqual(list(rejected), list(actually_rejected))
    actually_submitted = self.pool_mock.GetSubmittedChanges()
    self.assertEqual(list(submitted), actually_submitted)


class SubmitPoolTest(BaseSubmitPoolTestCase):
  """Test suite related to the Submit Pool."""

  def GetNotifyArg(self, change, key):
    """Look up a call to notify about |change| and grab |key| from it.

    Args:
      change: The change to look up.
      key: The key to look up. If this is an integer, look up a positional
        argument by index. Otherwise, look up a keyword argument.
    """
    names = []
    for call in self.pool_mock.patched['SendNotification'].call_args_list:
      call_args, call_kwargs = call
      if change == call_args[1]:
        if isinstance(key, int):
          return call_args[key]
        return call_kwargs[key]
      names.append(call_args[1])

    # Verify that |change| is present at all. This should always fail.
    self.assertIn(change, names)

  def assertEqualNotifyArg(self, value, change, idx):
    """Verify that |value| equals self.GetNotifyArg(|change|, |idx|)."""
    self.assertEqual(str(value), str(self.GetNotifyArg(change, idx)))

  def testSubmitPool(self):
    """Test that we can submit a pool successfully."""
    self.SubmitPool(submitted=self.patches)

  def testRejectCLs(self):
    """Test that we can reject a CL successfully."""
    self.SubmitPool(submitted=self.patches, failed_to_apply=True)

  def testSubmitCycle(self):
    """Submit a cyclic set of dependencies"""
    self.patch_mock.SetCQDependencies(self.patches[0], [self.patches[1]])
    self.SubmitPool(submitted=self.patches)

  def testSubmitReverseCycle(self):
    """Submit a cyclic set of dependencies, specified in reverse order."""
    self.patch_mock.SetCQDependencies(self.patches[1], [self.patches[0]])
    self.patch_mock.SetGerritDependencies(self.patches[1], [])
    self.patch_mock.SetGerritDependencies(self.patches[0], [self.patches[1]])
    self.SubmitPool(submitted=self.patches[::-1])

  def testRedundantCQDepend(self):
    """Submit a cycle with redundant CQ-DEPEND specifications."""
    self.patches = self.GetPatches(4)
    self.patch_mock.SetCQDependencies(self.patches[0], [self.patches[-1]])
    self.patch_mock.SetCQDependencies(self.patches[1], [self.patches[-1]])
    self.SubmitPool(submitted=self.patches)

  def testSubmitPartialCycle(self):
    """Submit a failed cyclic set of dependencies"""
    self.pool_mock.max_submits = 1
    self.patch_mock.SetCQDependencies(self.patches[0], [self.patches[1]])
    self.SubmitPool(submitted=self.patches, rejected=[self.patches[1]])
    (submitted, rejected) = self.pool_mock.GetSubmittedChanges()
    failed_submit = validation_pool.PatchFailedToSubmit(
        rejected, validation_pool.ValidationPool.INCONSISTENT_SUBMIT_MSG)
    bad_submit = validation_pool.PatchSubmittedWithoutDeps(
        submitted, failed_submit)
    self.assertEqualNotifyArg(failed_submit, rejected, 'error')
    self.assertEqualNotifyArg(bad_submit, submitted, 'failure')

  def testSubmitFailedCycle(self):
    """Submit a failed cyclic set of dependencies"""
    self.pool_mock.max_submits = 0
    self.patch_mock.SetCQDependencies(self.patches[0], [self.patches[1]])
    self.SubmitPool(submitted=[self.patches[0]], rejected=self.patches)
    (attempted,) = self.pool_mock.GetSubmittedChanges()
    (rejected,) = [x for x in self.patches if x != attempted]
    failed_submit = validation_pool.PatchFailedToSubmit(
        attempted, validation_pool.ValidationPool.INCONSISTENT_SUBMIT_MSG)
    dep_failed = cros_patch.DependencyError(rejected, failed_submit)
    self.assertEqualNotifyArg(failed_submit, attempted, 'error')
    self.assertEqualNotifyArg(dep_failed, rejected, 'error')

  def testConflict(self):
    """Submit a change that conflicts with TOT."""
    error = gob_util.GOBError(httplib.CONFLICT, 'Conflict')
    self.pool_mock.submit_results[self.patches[0]] = error
    self.SubmitPool(submitted=[self.patches[0]], rejected=self.patches[::-1])
    notify_error = validation_pool.PatchConflict(self.patches[0])
    self.assertEqualNotifyArg(notify_error, self.patches[0], 'error')

  def testServerError(self):
    """Test case where GOB returns a server error."""
    error = gerrit.GerritException('Internal server error')
    self.pool_mock.submit_results[self.patches[0]] = error
    self.SubmitPool(submitted=[self.patches[0]], rejected=self.patches[::-1])
    notify_error = validation_pool.PatchFailedToSubmit(self.patches[0], error)
    self.assertEqualNotifyArg(notify_error, self.patches[0], 'error')

  def testDraftCL(self):
    """Test that a draft CL is rejected."""
    self.patches[1].patch_dict['currentPatchSet']['draft'] = True
    self.SubmitPool(submitted=self.patches[:1], rejected=self.patches[1:])

  def testNotCommitReady(self):
    """Test that a CL without the commit ready bit is rejected."""
    self.PatchObject(self.patches[1], 'HasApproval', return_value=False)
    self.SubmitPool(submitted=self.patches[:1], rejected=self.patches[1:])

  def testAlreadyMerged(self):
    """Test that a CL that was chumped during the run was not rejected."""
    self.PatchObject(self.patches[0], 'IsAlreadyMerged', return_value=True)
    self.SubmitPool(submitted=self.patches[1:], rejected=[])


class SubmitPartialPoolTest(BaseSubmitPoolTestCase):
  """Test the SubmitPartialPool function."""

  def setUp(self):
    # Set up each patch to be in its own project, so that we can easily
    # request to ignore failures for the specified patch.
    for patch in self.patches:
      patch.project = str(patch)

    self.stage_name = 'MyHWTest'

  def GetTracebacks(self):
    """Return a list containing a single traceback."""
    traceback = results_lib.RecordedTraceback(
        self.stage_name, self.stage_name, Exception(), '')
    return [traceback]

  def IgnoreFailures(self, patch):
    """Set us up to ignore failures for the specified |patch|."""
    self.ignores[patch] = [self.stage_name]

  def testSubmitNone(self):
    """Submit no changes."""
    self.SubmitPool(submitted=(), rejected=self.patches)

  def testSubmitAll(self):
    """Submit all changes."""
    self.IgnoreFailures(self.patches[0])
    self.IgnoreFailures(self.patches[1])
    self.SubmitPool(submitted=self.patches, rejected=[])

  def testSubmitFirst(self):
    """Submit the first change in a series."""
    self.IgnoreFailures(self.patches[0])
    self.SubmitPool(submitted=[self.patches[0]], rejected=[self.patches[1]])

  def testSubmitSecond(self):
    """Attempt to submit the second change in a series."""
    self.IgnoreFailures(self.patches[1])
    self.SubmitPool(submitted=[], rejected=[self.patches[0]])


if __name__ == '__main__':
  cros_test_lib.main()
