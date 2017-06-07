# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains unittests for validation_pool module."""

from __future__ import print_function

import collections
import contextlib
import copy
import functools
import httplib
import itertools
import mock
import mox
import os
import pickle
import tempfile
import time

from chromite.cbuildbot import patch_series
from chromite.cbuildbot import repository
from chromite.cbuildbot import validation_pool
from chromite.lib import cidb
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import gob_util
from chromite.lib import gs_unittest
from chromite.lib import metadata_lib
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.lib import patch as cros_patch
from chromite.lib import patch_unittest
from chromite.lib import timeout_util
from chromite.lib import tree_status
from chromite.lib import triage_lib


site_config = config_lib.GetConfig()


_GetNumber = iter(itertools.count()).next
# Without this some lambda's defined in constants will not be the same as
# constants defined in this module. For comparisons, lambdas must be the same
# function.
validation_pool.constants = constants

def GetTestJson(change_id=None):
  """Get usable fake Gerrit patch json data

  Args:
    change_id: If given, force this ChangeId
  """
  data = copy.deepcopy(patch_unittest.FAKE_PATCH_JSON)
  if change_id is not None:
    data['id'] = str(change_id)
  return data

def MakePool(overlays=constants.PUBLIC_OVERLAYS, build_number=1,
             builder_name='foon', is_master=True, dryrun=True,
             fake_db=None, **kwargs):
  """Helper for creating ValidationPool objects for tests."""
  kwargs.setdefault('candidates', [])
  build_root = kwargs.pop('build_root', '/fake_root')

  builder_run = FakeBuilderRun(fake_db)
  if fake_db:
    build_id = fake_db.InsertBuild(
        builder_name, constants.WATERFALL_INTERNAL, build_number,
        'build-config', 'bot hostname')
    builder_run.attrs.metadata.UpdateWithDict({'build_id': build_id})


  pool = validation_pool.ValidationPool(
      overlays, build_root, build_number, builder_name, is_master,
      dryrun, builder_run=builder_run, **kwargs)
  return pool


class MockManifest(object):
  """Helper class for Mocking Manifest objects."""

  def __init__(self, path, **kwargs):
    self.root = path
    for key, attr in kwargs.iteritems():
      setattr(self, key, attr)


class FakeBuilderRun(object):
  """A lightweight partial implementation of BuilderRun.

  validation_pool.ValidationPool makes use of a BuilderRun to access
  cidb and metadata, but does not need to make use of the extensive
  other BuilderRun features. This lightweight partial reimplementation
  allows unit tests to be much faster.
  """
  def __init__(self, fake_db=None):
    self.fake_db = fake_db
    metadata_dict = {'buildbot-master-name': constants.WATERFALL_INTERNAL}
    FakeAttrs = collections.namedtuple('FakeAttrs', ['metadata'])
    self.attrs = FakeAttrs(metadata=metadata_lib.CBuildbotMetadata(
        metadata_dict=metadata_dict))
    FakeConfig = collections.namedtuple('FakeConfig', ['name'])
    self.config = FakeConfig(name='master-paladin')
    self.GetBuildbotUrl = lambda: constants.WATERFALL_INTERNAL
    self.GetWaterfall = lambda: constants.WATERFALL_INTERNAL

  def GetCIDBHandle(self):
    """Get the build_id and cidb handle, if available.

    Returns:
      A (build_id, CIDBConnection) tuple if fake_db is set up and a build_id is
      known in metadata. Otherwise, (None, None).
    """
    try:
      build_id = self.attrs.metadata.GetValue('build_id')
    except KeyError:
      return (None, None)

    if build_id is not None and self.fake_db:
      return (build_id, self.fake_db)

    return (None, None)


# pylint: disable=protected-access
class MoxBase(patch_unittest.MockPatchBase, cros_test_lib.MoxTestCase):
  """Base class for other test suites with numbers mocks patched in."""

  def setUp(self):
    self.build_root = 'fakebuildroot'
    self.manager = parallel.Manager()
    self.PatchObject(gob_util, 'CreateHttpConn',
                     side_effect=AssertionError('Test should not contact GoB'))
    self.PatchObject(gob_util, 'CheckChange')
    self.PatchObject(tree_status, 'IsTreeOpen', return_value=True)
    self.PatchObject(tree_status, 'WaitForTreeStatus',
                     return_value=constants.TREE_OPEN)
    self.fake_db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)
    # Suppress all gerrit access; having this occur is generally a sign
    # the code is either misbehaving, or that the tests are bad.
    self.mox.StubOutWithMock(gerrit.GerritHelper, 'Query')
    self.gs_mock = self.StartPatcher(gs_unittest.GSContextMock())

  def tearDown(self):
    cidb.CIDBConnectionFactory.ClearMock()

  def MakeHelper(self, cros_internal=None, cros=None):
    # pylint: disable=W0201
    if cros_internal:
      cros_internal = self.mox.CreateMock(gerrit.GerritHelper)
      cros_internal.version = '2.2'
      cros_internal.remote = site_config.params.INTERNAL_REMOTE
    if cros:
      cros = self.mox.CreateMock(gerrit.GerritHelper)
      cros.remote = site_config.params.EXTERNAL_REMOTE
      cros.version = '2.2'
    return patch_series.HelperPool(cros_internal=cros_internal,
                                   cros=cros)

class MockPatchSeries(partial_mock.PartialMock):
  """Mock the PatchSeries functions."""
  TARGET = 'chromite.cbuildbot.patch_series.PatchSeries'
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


class FakeValidationPool(partial_mock.PartialMock):
  """Mocks out ValidationPool's interaction with cidb."""
  TARGET = 'chromite.cbuildbot.validation_pool.ValidationPool'
  ATTRS = ['_InsertCLActionToDatabase']

  def _InsertCLActionToDatabase(self, *args, **kwargs):
    pass


class TestSubmitChange(MoxBase):
  """Test suite related to submitting changes."""

  def setUp(self):
    self.orig_timeout = validation_pool.SUBMITTED_WAIT_TIMEOUT
    self.pool_mock = self.StartPatcher(FakeValidationPool())
    validation_pool.SUBMITTED_WAIT_TIMEOUT = 4

  def tearDown(self):
    validation_pool.SUBMITTED_WAIT_TIMEOUT = self.orig_timeout

  def _TestSubmitChange(self, results, build_id=31337):
    """Test submitting a change with the given results."""
    results = [cros_test_lib.EasyAttr(status=r) for r in results]
    change = self.MockPatch(change_id=12345, patch_number=1)
    pool = validation_pool.ValidationPool(
        constants.VALID_OVERLAYS[0],
        build_root=None,
        build_number=0,
        builder_name='',
        is_master=False,
        dryrun=False)
    pool._run = FakeBuilderRun(self.fake_db)
    pool._run.attrs.metadata.UpdateWithDict({'build_id': build_id})
    pool._helper_pool = self.mox.CreateMock(patch_series.HelperPool)
    helper = self.mox.CreateMock(validation_pool.gerrit.GerritHelper)
    pool._helper_pool.host = ''
    helper.host = ''

    # Prepare replay script.
    pool._helper_pool.ForChange(change).AndReturn(helper)
    pool._helper_pool.ForChange(change).AndReturn(helper)
    helper.SubmitChange(change, dryrun=False)
    pool._InsertCLActionToDatabase(change, mox.IgnoreArg(), mox.IgnoreArg())
    for result in results:
      helper.QuerySingleRecord(change.gerrit_number).AndReturn(result)
    if results[-1]['status'] == 'SUBMITTED':
      helper.SetReview(change, msg=mox.IgnoreArg())
    self.mox.ReplayAll()

    # Verify results.
    retval = validation_pool.ValidationPool._SubmitChangeUsingGerrit(
        pool, change, reason=mox.IgnoreArg())
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
    self._pool = MakePool(applied=self._patches, fake_db=self.fake_db)

    suspects = triage_lib.SuspectChanges({
        x: constants.SUSPECT_REASON_UNKNOWN for x in self._patches})
    self.PatchObject(
        triage_lib.CalculateSuspects, 'FindSuspects',
        return_value=suspects)
    self.PatchObject(validation_pool.ValidationPool, 'SendNotification')
    self.remove = self.PatchObject(gerrit.GerritHelper, 'RemoveReady')
    self.PatchObject(gerrit, 'GetGerritPatchInfoWithPatchQueries',
                     lambda x: x)
    self.PatchObject(triage_lib.CalculateSuspects, 'OnlyLabFailures',
                     return_value=False)
    self.PatchObject(triage_lib.CalculateSuspects, 'OnlyInfraFailures',
                     return_value=False)
    self.StartPatcher(parallel_unittest.ParallelMock())
    self._AssertActions(self._patches, [])

  def _AssertActions(self, changes, actions):
    """Assert that each change in |changes| has |actions|."""
    for change in changes:
      action_history = self.fake_db.GetActionsForChanges([change])
      self.assertEqual([x.action for x in action_history], actions)

  def testGetAppliedPatches(self):
    """Test GetAppliedPatches."""
    self.assertTrue(isinstance(self._pool.GetAppliedPatches(),
                               patch_series.PatchSeries))

    mock_patches = mock.Mock()
    self._pool.applied_patches = mock_patches
    self.assertEqual(self._pool.GetAppliedPatches(), mock_patches)

  def testPatchesWereRejectedByFailure(self):
    """Tests that all patches are rejected by failure."""
    self._pool.HandleValidationFailure([self._BUILD_MESSAGE])
    self.assertEqual(len(self._patches), self.remove.call_count)
    self._AssertActions(self._patches, [constants.CL_ACTION_KICKED_OUT])

  def testPatchesWereRejectedByTimeout(self):
    self._pool.HandleValidationTimeout()
    self.assertEqual(len(self._patches), self.remove.call_count)
    self._AssertActions(self._patches, [constants.CL_ACTION_KICKED_OUT])

  def testOnlyChromitePatchesWereRejectedByTimeout(self):
    self._patches[-1].project = 'chromiumos/tacos'
    self._pool.HandleValidationTimeout()
    self.assertEqual(len(self._patches) - 1, self.remove.call_count)
    self._AssertActions(self._patches[:-1], [constants.CL_ACTION_KICKED_OUT])
    self._AssertActions(self._patches[-1:], [constants.CL_ACTION_FORGIVEN])

  def testNoSuspectsWithFailure(self):
    """Tests no change is blamed when there is no suspect."""
    self.PatchObject(triage_lib.CalculateSuspects, 'FindSuspects',
                     return_value=triage_lib.SuspectChanges())
    self._pool.HandleValidationFailure([self._BUILD_MESSAGE])
    self.assertEqual(0, self.remove.call_count)
    self._AssertActions(self._patches, [constants.CL_ACTION_FORGIVEN])

  def testPreCQ(self):
    for change in self._patches:
      self._pool.UpdateCLPreCQStatus(change, constants.CL_STATUS_PASSED)
    self._pool.pre_cq_trybot = True
    self._pool.HandleValidationFailure([self._BUILD_MESSAGE])
    self.assertEqual(0, self.remove.call_count)
    self._AssertActions(self._patches, [constants.CL_ACTION_PRE_CQ_PASSED])

  def testPatchesWereNotRejectedByInsaneFailure(self):
    self._pool.HandleValidationFailure([self._BUILD_MESSAGE], sanity=False)
    self.assertEqual(0, self.remove.call_count)
    self._AssertActions(self._patches, [constants.CL_ACTION_FORGIVEN])


class TestCoreLogic(MoxBase):
  """Tests resolution and applying logic of validation_pool.ValidationPool."""

  def setUp(self):
    self.mox.StubOutWithMock(patch_series.PatchSeries, 'Apply')
    self.mox.StubOutWithMock(patch_series.PatchSeries, 'ApplyChange')
    self.mox.StubOutWithMock(patch_series.PatchSeries, 'FetchChanges')
    self.patch_mock = self.StartPatcher(MockPatchSeries())
    funcs = ['SendNotification', '_SubmitChangeUsingGerrit']
    for func in funcs:
      self.mox.StubOutWithMock(validation_pool.ValidationPool, func)
    self.PatchObject(gerrit, 'GetGerritPatchInfoWithPatchQueries',
                     side_effect=lambda x: x)
    self.StartPatcher(parallel_unittest.ParallelMock())

  def MakePool(self, *args, **kwargs):
    """Helper for creating ValidationPool objects for Mox tests."""
    handlers = kwargs.pop('handlers', False)
    kwargs['build_root'] = self.build_root
    pool = MakePool(*args, **kwargs)
    funcs = ['HandleApplySuccess', '_HandleApplyFailure',
             '_HandleCouldNotApply', '_HandleCouldNotSubmit',
             '_HandleFailedToApplyDueToInflightConflict']
    if handlers:
      for func in funcs:
        self.mox.StubOutWithMock(pool, func)
    return pool

  def MakeFailure(self, patch, inflight=True):
    return cros_patch.ApplyPatchException(patch, inflight=inflight)

  def GetPool(self, changes, applied=(), tot=(), inflight=(), **kwargs):
    pool = self.MakePool(
        candidates=changes, applied=[], fake_db=self.fake_db, **kwargs)
    applied = list(applied)
    tot = [self.MakeFailure(x, inflight=False) for x in tot]
    inflight = [self.MakeFailure(x, inflight=True) for x in inflight]
    # pylint: disable=E1120,E1123
    patch_series.PatchSeries.Apply(
        changes, manifest=mox.IgnoreArg()).AndReturn((applied, tot, inflight))

    for patch in applied:
      pool.HandleApplySuccess(patch, mox.IgnoreArg()).AndReturn(None)

    if tot:
      pool._HandleApplyFailure(tot).AndReturn(None)

    for failure in inflight:
      pool._HandleFailedToApplyDueToInflightConflict(
          failure.patch).AndReturn(None)

    # We stash this on the pool object so we can reuse it during validation.
    # We could stash this in the test instances, but that would break
    # for any tests that do multiple pool instances.

    pool._test_data = (changes, applied, tot, inflight)

    return pool

  def testApplySlavePool(self):
    """Verifies that slave calls ApplyChange() directly for each patch."""
    slave_pool = self.MakePool(is_master=False)
    patches = self.GetPatches(3)
    slave_pool.candidates = patches
    # pylint: disable=E1120, E1123
    patch_series.PatchSeries.FetchChanges(patches, manifest=mox.IgnoreArg())
    for patch in patches:
      patch_series.PatchSeries.ApplyChange(patch, manifest=mox.IgnoreArg())

    self.mox.ReplayAll()
    self.assertEqual(True, slave_pool.ApplyPoolIntoRepo())
    self.mox.VerifyAll()

  def runApply(self, pool, result):
    self.assertEqual(result, pool.ApplyPoolIntoRepo())
    self.assertEqual(pool.applied, pool._test_data[1])
    failed_inflight = pool.changes_that_failed_to_apply_earlier
    expected_inflight = set(pool._test_data[3])
    # Intersect the results, since it's possible there were results failed
    # results that weren't related to the ApplyPoolIntoRepo call.
    self.assertEqual(set(failed_inflight).intersection(expected_inflight),
                     expected_inflight)

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
    pool = self.MakePool(fake_db=self.fake_db)
    pool.SendNotification(patch, mox.StrContains('has picked up your change'),
                          build_log=mox.IgnoreArg())
    self.mox.ReplayAll()
    pool.HandleApplySuccess(patch, build_log=mox.IgnoreArg())
    self.mox.VerifyAll()

  def testHandleApplyFailure(self):
    failures = [cros_patch.ApplyPatchException(x) for x in self.GetPatches(4)]

    notified_patches = failures[:2]
    unnotified_patches = failures[2:]
    master_pool = self.MakePool(dryrun=False)
    slave_pool = self.MakePool(is_master=False)

    self.mox.StubOutWithMock(gerrit.GerritHelper, 'RemoveReady')

    for failure in notified_patches:
      master_pool.SendNotification(
          failure.patch,
          mox.StrContains('failed to apply your change'),
          failure=mox.IgnoreArg())
      # This pylint suppressin shouldn't be necessary, but pylint is invalidly
      # thinking that the first arg isn't passed in; we suppress it to suppress
      # the pylnt bug.
      # pylint: disable=E1120
      gerrit.GerritHelper.RemoveReady(failure.patch, dryrun=False)

    self.mox.ReplayAll()
    master_pool._HandleApplyFailure(notified_patches)
    slave_pool._HandleApplyFailure(unnotified_patches)
    self.mox.VerifyAll()

  def _setUpSubmit(self):
    pool = self.MakePool(dryrun=False, handlers=True)
    patches = self.GetPatches(3)
    failed = self.GetPatches(3)
    pool.applied = patches[:]
    # While we don't do anything w/ these patches, that's
    # intentional; we're verifying that it isn't submitted
    # if there is a failure.
    pool.changes_that_failed_to_apply_earlier = failed[:]

    return (pool, patches, failed)

  def testSubmitPoolFailures(self):
    """Tests that a fatal exception is raised."""
    pool, patches, _failed = self._setUpSubmit()
    patch1, patch2, patch3 = patches

    pool._SubmitChangeUsingGerrit(patch1, reason=None).AndReturn(True)
    pool._SubmitChangeUsingGerrit(patch2, reason=None).AndReturn(False)

    pool._HandleCouldNotSubmit(patch2, mox.IgnoreArg()).InAnyOrder()
    pool._HandleCouldNotSubmit(patch3, mox.IgnoreArg()).InAnyOrder()

    # pylint: disable=E1120,E1123
    patch_series.PatchSeries.Apply(set()).AndReturn(([], [], []))
    self.mox.ReplayAll()

    mock_manifest = mock.MagicMock()
    with mock.patch.object(git.ManifestCheckout, 'Cached', new=mock_manifest):
      self.assertRaises(validation_pool.FailedToSubmitAllChangesException,
                        pool.SubmitPool)
    self.mox.VerifyAll()

  def testSubmitPool(self):
    """Tests that we can submit a pool of patches."""
    pool, patches, failed = self._setUpSubmit()
    reason = 'fake reason'

    for patch in patches:
      pool._SubmitChangeUsingGerrit(patch, reason=reason).AndReturn(True)

    pool._HandleApplyFailure(failed)

    # pylint: disable=E1120,E1123
    patch_series.PatchSeries.Apply(set()).AndReturn(([], [], []))
    self.mox.ReplayAll()
    mock_manifest = mock.MagicMock()
    with mock.patch.object(git.ManifestCheckout, 'Cached', new=mock_manifest):
      pool.SubmitPool(reason=reason)
    self.mox.VerifyAll()

  def testSubmitNonManifestChanges(self):
    """Simple test to make sure we can submit non-manifest changes."""
    pool, patches, _failed = self._setUpSubmit()
    pool.non_manifest_changes = patches[:]
    reason = 'fake reason'

    for patch in patches:
      pool._SubmitChangeUsingGerrit(patch, reason=reason).AndReturn(True)

    # pylint: disable=E1120,E1123
    patch_series.PatchSeries.Apply(set()).AndReturn(([], [], []))

    mock_manifest = mock.MagicMock()
    self.mox.ReplayAll()
    with mock.patch.object(git.ManifestCheckout, 'Cached', new=mock_manifest):
      pool.SubmitNonManifestChanges(reason=reason)
    self.mox.VerifyAll()

  def testSubmitAccumulation(self):
    """Tests ValidationPool.SubmitChanges.

    Tests that it accumulates a mix of local and remote changes that were
    submitted and rejected.
    """
    pool, patches, _failed = self._setUpSubmit()
    pool.non_manifest_changes = patches[:1]
    reason = 'fake reason'

    # pylint: disable=E1120,E1123
    error = mock.Mock(patch=patches[1])
    patch_series.PatchSeries.Apply(
        set(patches[1:])).AndReturn(
            ([patches[2]],
             [error],
             []))

    self.mox.StubOutWithMock(patch_series.PatchSeries, 'GetGitRepoForChange')
    for i, patch in enumerate(patches):
      # pylint: disable=E1120,E1123
      patch_series.PatchSeries.GetGitRepoForChange(
          mox.IgnoreArg(), strict=False
          ).AndReturn('foo_repo' if i > 0 else None)

    self.mox.StubOutWithMock(validation_pool.ValidationPool,
                             'SubmitLocalChanges')
    pool.SubmitLocalChanges(
        {'foo_repo': {patches[2]:reason}}
        ).AndReturn((set((patches[2],)), {}))

    for patch in pool.non_manifest_changes:
      pool._SubmitChangeUsingGerrit(patch, reason=reason).AndReturn(True)

    pool._HandleCouldNotSubmit(patches[1], error)

    mock_manifest = mock.MagicMock()
    self.mox.ReplayAll()
    verified_cls = {c:reason for c in patches}
    with mock.patch.object(git.ManifestCheckout, 'Cached', new=mock_manifest):
      submitted, errors = pool.SubmitChanges(verified_cls)

    self.assertEqual(submitted, set((patches[0], patches[2])))
    self.assertEqual(errors, {patches[1]: error})
    self.mox.VerifyAll()

  def testPushRepoBranchPushesOnce(self):
    """Tests that PushRepoBranch pushes once if there is no error."""
    pool, patches, _failed = self._setUpSubmit()

    repo = '/fake/path/aoeuidhtns'
    tracking_branch = git.RemoteRef('cros', 'to_branch')

    context = contextlib.nested(
        mock.patch.object(git, 'SyncPushBranch'),
        mock.patch.object(git, 'GitPush'),
        mock.patch.object(git, 'GetTrackingBranch',
                          new=lambda _: tracking_branch))

    with context as (sync_func, push_func, _):
      errors = pool.PushRepoBranch(repo, set(patches), 'from_branch')
      self.assertEqual({}, errors)
      self.assertEqual(1, sync_func.call_count)
      self.assertEqual(1, push_func.call_count)

  def testUnhandledExceptions(self):
    """Test that CQ doesn't loop due to unhandled Exceptions."""
    pool, patches, _failed = self._setUpSubmit()

    pool.candidates = pool.applied
    pool.applied = []

    class MyException(Exception):
      """"Unique Exception used for testing."""

    def VerifyCQError(patch, error):
      cq_error = validation_pool.InternalCQError(patch, error.message)
      return str(error) == str(cq_error)

    # pylint: disable=E1120,E1123
    patch_series.PatchSeries.Apply(
        patches, manifest=mox.IgnoreArg()).AndRaise(MyException)
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
    pool = self.MakePool()
    pool.filtered_set = set(self.GetPatches(4))
    result = pool._FilterDependencyErrors(failures)
    self.assertEquals(set(failures[:-1]), set(result))
    self.mox.VerifyAll()

  def testFilterSpeculativeErrors(self):
    """Filter out dependency errors for speculative patches."""
    failures = [cros_patch.ApplyPatchException(x) for x in self.GetPatches(2)]
    failures += [cros_patch.DependencyError(x, y) for x, y in
                 zip(self.GetPatches(2), failures)]
    self.PatchObject(failures[-1].patch, 'HasReadyFlag', return_value=False)
    self.mox.ReplayAll()
    pool = self.MakePool()
    pool.filtered_set = set(self.GetPatches(2))
    result = pool._FilterDependencyErrors(failures)
    self.assertEquals(set(failures[:-1]), set(result))
    self.mox.VerifyAll()

  def testFilterDependencyErrorsOnFilteredChanges(self):
    """Test FilterDependencyErrors on filtered changes."""
    p_1, p_2, p_3, p_4 = self.GetPatches(4)
    e_1 = patch_series.PatchNotEligible(p_1)
    e_2 = cros_patch.DependencyError(p_2, e_1)
    e_3 = cros_patch.DependencyError(p_3, e_2)
    e_4 = cros_patch.ApplyPatchException(p_4)
    errors = [e_2, e_3, e_4]

    pool = self.MakePool()
    pool.filtered_set = set([p_1])

    result = pool._FilterDependencyErrors(errors)
    self.assertItemsEqual(result, [e_4])

  def testFilterNonCrosProjects(self):
    """Runs through a filter of own manifest and fake changes.

    This test should filter out the tacos/chromite project as its not real.
    """
    base_func = itertools.cycle(['chromiumos', 'chromeos']).next
    patches = self.GetPatches(10)
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

    # Mark the last two patches as not commit ready.
    for p in patches[-2:]:
      p.IsMergeable = lambda *_args, **_kwargs: False

    # Non-manifest patches that aren't commit ready should be skipped.
    filtered_patches = filtered_patches[:-1]

    self.mox.ReplayAll()
    results = validation_pool.ValidationPool._FilterNonCrosProjects(
        patches + non_cros_patches, manifest)

    def compare(list1, list2):
      mangle = lambda c: (c.id, c.project, c.tracking_branch)
      self.assertEqual(
          list1, list2,
          msg=('Comparison failed:\n list1: %r\n list2: %r'
               % (map(mangle, list1), map(mangle, list2))))

    compare(results[0], allowed_patches)
    compare(results[1], filtered_patches)

  def testAcquirePool(self):
    """Various tests for the AcquirePool method."""
    directory = '/tmp/dontmattah'
    repo = repository.RepoRepository(directory, directory, 'master', depth=1)
    self.mox.StubOutWithMock(repo, 'Sync')
    self.mox.StubOutWithMock(validation_pool.ValidationPool, 'AcquireChanges')
    self.mox.StubOutWithMock(time, 'sleep')
    self.mox.StubOutWithMock(tree_status, 'WaitForTreeStatus')

    # 1) Test, tree open -> get changes and finish.
    tree_status.WaitForTreeStatus(
        period=mox.IgnoreArg(),
        throttled_ok=mox.IgnoreArg(),
        timeout=mox.IgnoreArg()).AndReturn(constants.TREE_OPEN)
    repo.Sync()
    # pylint: disable=no-value-for-parameter
    validation_pool.ValidationPool.AcquireChanges(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(True)

    self.mox.ReplayAll()

    query = constants.CQ_READY_QUERY
    pool = validation_pool.ValidationPool.AcquirePool(
        constants.PUBLIC_OVERLAYS, repo, 1, 'buildname', query, dryrun=False,
        check_tree_open=True)

    self.assertTrue(pool.tree_was_open)
    self.mox.VerifyAll()
    self.mox.ResetAll()

    # 2) Test, tree open -> need to loop at least once to get changes.
    tree_status.WaitForTreeStatus(
        period=mox.IgnoreArg(),
        throttled_ok=mox.IgnoreArg(),
        timeout=mox.IgnoreArg()).AndReturn(constants.TREE_OPEN)
    repo.Sync()
    validation_pool.ValidationPool.AcquireChanges(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(False)
    time.sleep(validation_pool.ValidationPool.SLEEP_TIMEOUT)
    tree_status.WaitForTreeStatus(
        period=mox.IgnoreArg(),
        throttled_ok=mox.IgnoreArg(),
        timeout=mox.IgnoreArg()).AndReturn(constants.TREE_OPEN)
    repo.Sync()
    validation_pool.ValidationPool.AcquireChanges(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(True)
    self.mox.ReplayAll()

    query = constants.CQ_READY_QUERY
    pool = validation_pool.ValidationPool.AcquirePool(
        constants.PUBLIC_OVERLAYS, repo, 1, 'buildname', query, dryrun=False,
        check_tree_open=True)

    self.assertTrue(pool.tree_was_open)
    self.mox.VerifyAll()
    self.mox.ResetAll()

    # 3) Test, tree throttled -> use exponential fallback logic.
    tree_status.WaitForTreeStatus(
        period=mox.IgnoreArg(),
        throttled_ok=mox.IgnoreArg(),
        timeout=mox.IgnoreArg()).AndReturn(constants.TREE_THROTTLED)
    repo.Sync()
    validation_pool.ValidationPool.AcquireChanges(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(True)

    self.mox.ReplayAll()

    query = constants.CQ_READY_QUERY
    pool = validation_pool.ValidationPool.AcquirePool(
        constants.PUBLIC_OVERLAYS, repo, 1, 'buildname', query, dryrun=False,
        check_tree_open=True)

    self.assertFalse(pool.tree_was_open)


  def testGetFailStreak(self):
    """Tests that we're correctly able to calculate a fail streak."""
    # Leave first build as inflight.
    builder_name = 'master-paladin'
    slave_pool = self.MakePool(builder_name=builder_name, fake_db=self.fake_db)
    self.fake_db.buildTable[0]['status'] = constants.BUILDER_STATUS_INFLIGHT
    self.fake_db.buildTable[0]['build_config'] = builder_name
    self.assertEqual(slave_pool._GetFailStreak(), 0)

    # Create a passing build.
    for i in range(2):
      self.fake_db.InsertBuild(
          builder_name, None, i, builder_name, 'abcdelicious',
          status=constants.BUILDER_STATUS_PASSED)

    self.assertEqual(slave_pool._GetFailStreak(), 0)

    # Add a fail streak.
    for i in range(3, 6):
      self.fake_db.InsertBuild(
          builder_name, None, i, builder_name, 'abcdelicious',
          status=constants.BUILDER_STATUS_FAILED)

    self.assertEqual(slave_pool._GetFailStreak(), 3)

    # Add another success and failure.
    self.fake_db.InsertBuild(
        builder_name, None, 6, builder_name, 'abcdelicious',
        status=constants.BUILDER_STATUS_PASSED)
    self.fake_db.InsertBuild(
        builder_name, None, 7, builder_name, 'abcdelicious',
        status=constants.BUILDER_STATUS_FAILED)

    self.assertEqual(slave_pool._GetFailStreak(), 1)

    # Finally just add one last pass and make sure fail streak is wiped.
    self.fake_db.InsertBuild(
        builder_name, None, 8, builder_name, 'abcdelicious',
        status=constants.BUILDER_STATUS_PASSED)

    self.assertEqual(slave_pool._GetFailStreak(), 0)

  def testFilterChangesForThrottledTree(self):
    """Tests that we can correctly apply exponential fallback."""
    patches = self.GetPatches(4)
    self.mox.StubOutWithMock(validation_pool.ValidationPool, '_GetFailStreak')

    #
    # Test when tree is open.
    #
    self.mox.ReplayAll()

    # Perform test.
    slave_pool = self.MakePool(candidates=patches, tree_was_open=True)
    slave_pool.FilterChangesForThrottledTree()

    # Validate results.
    self.assertEqual(len(slave_pool.candidates), 4)
    self.assertIsNone(slave_pool.filtered_set)
    self.mox.VerifyAll()
    self.mox.ResetAll()

    #
    # Test when tree is closed with a streak of 1.
    #

    # pylint: disable=no-value-for-parameter
    validation_pool.ValidationPool._GetFailStreak().AndReturn(1)
    self.mox.ReplayAll()

    # Perform test.
    slave_pool = self.MakePool(candidates=patches, tree_was_open=False)
    slave_pool.FilterChangesForThrottledTree()

    # Validate results.
    self.assertEqual(len(slave_pool.candidates), 2)
    self.assertEqual(len(slave_pool.filtered_set), 2)
    self.mox.VerifyAll()
    self.mox.ResetAll()

    #
    # Test when tree is closed with a streak of 2.
    #

    # pylint: disable=no-value-for-parameter
    validation_pool.ValidationPool._GetFailStreak().AndReturn(2)
    self.mox.ReplayAll()

    # Perform test.
    slave_pool = self.MakePool(candidates=patches, tree_was_open=False)
    slave_pool.FilterChangesForThrottledTree()

    # Validate results.
    self.assertEqual(len(slave_pool.candidates), 1)
    self.assertEqual(len(slave_pool.filtered_set), 3)
    self.mox.VerifyAll()
    self.mox.ResetAll()

    #
    # Test when tree is closed with a streak of many.
    #

    # pylint: disable=no-value-for-parameter
    validation_pool.ValidationPool._GetFailStreak().AndReturn(200)
    self.mox.ReplayAll()

    # Perform test.
    slave_pool = self.MakePool(candidates=patches, tree_was_open=False)
    slave_pool.FilterChangesForThrottledTree()

    # Validate results.
    self.assertEqual(len(slave_pool.candidates), 1)
    self.assertEqual(len(slave_pool.filtered_set), 3)
    self.mox.VerifyAll()

  def _UpdatedDependencyMap(self, dependency_map):
    pool = self.MakePool()

    directs = dict((k, set(v)) for (k, v) in dependency_map.iteritems())

    keys = dependency_map.keys()
    for change in keys:
      visited = pool._DepthFirstSearch(directs, change)
      visited.remove(change)
      if visited:
        dependency_map[change] = visited

  def test_UpdateDependencyMapOnTransitiveDependency(self):
    """Test _UpdateDependencyMap on transitive dependency."""
    dep_map = {
        'A': {'B'},
        'B': {'C'},
        'C': {'D'}
    }

    self._UpdatedDependencyMap(dep_map)

    expect_map = {
        'A': {'B', 'C', 'D'},
        'B': {'C', 'D'},
        'C': {'D'}
    }
    self.assertDictEqual(dep_map, expect_map)

  def test_UpdateDependencyMapOnMutualDependency(self):
    """Test _UpdateDependencyMap on mutual dependency."""
    dep_map = {
        'A': {'B', 'D', 'E'},
        'B': {'A'}
    }

    self._UpdatedDependencyMap(dep_map)

    expect_map = {
        'A': {'B', 'D', 'E'},
        'B': {'A', 'D', 'E'}
    }
    self.assertDictEqual(dep_map, expect_map)

  def test_UpdateDependencyMapOnCircularDependency(self):
    """Test _UpdateDependencyMap on circular dependency."""
    dep_map = {
        'A': {'B'},
        'B': {'C'},
        'C': {'D'},
        'D': {'A'}
    }

    self._UpdatedDependencyMap(dep_map)

    expect_map = {
        'A': {'B', 'C', 'D'},
        'B': {'A', 'C', 'D'},
        'C': {'A', 'B', 'D'},
        'D': {'A', 'B', 'C'}
    }
    self.assertDictEqual(dep_map, expect_map)

  def test_UpdateDependencyMapMix(self):
    """Test _UpdateDependencyMap on mixed dependencies."""
    dep_map = {
        'B': {'A'},
        'C': {'B'},
        'F': {'E'},
        'E': {'D', 'G'},
        'H': {'I'},
        'I': {'H'}
    }

    self._UpdatedDependencyMap(dep_map)

    expect_map = {
        'B': {'A'},
        'C': {'A', 'B'},
        'E': {'D', 'G'},
        'F': {'D', 'E', 'G'},
        'H': {'I'},
        'I': {'H'}
    }
    self.assertDictEqual(dep_map, expect_map)

  def testGetDependMapForChangesWithTimeoutError(self):
    """Test GetDependMapForChanges with TimeoutError."""
    self.PatchObject(validation_pool.ValidationPool, 'GetTransitiveDependMap',
                     side_effect=timeout_util.TimeoutError())

    pool = self.MakePool()
    patches = patch_series.PatchSeries('path')
    p = self.GetPatches(how_many=3)

    for patch in p:
      self.patch_mock.SetGerritDependencies(patch, [])

    # p0 -> p1, p1 -> p2, p2 -> p0
    self.patch_mock.SetCQDependencies(p[0], [p[1]])
    self.patch_mock.SetCQDependencies(p[1], [p[2]])
    self.patch_mock.SetCQDependencies(p[2], [p[0]])

    dep_map = pool.GetDependMapForChanges(p, patches)
    expected_map = {
        p[0]: {p[2]},
        p[1]: {p[0]},
        p[2]: {p[1]}
    }
    self.assertDictEqual(dep_map, expected_map)

  def testGetDependMapForChangesOnNoDependency(self):
    """Test GetDependMapForChanges on no dependency."""
    pool = self.MakePool()
    patches = patch_series.PatchSeries('path')
    p = self.GetPatches(how_many=3)

    for patch in p:
      self.patch_mock.SetGerritDependencies(patch, [])
      self.patch_mock.SetCQDependencies(patch, [])

    dep_map = pool.GetDependMapForChanges(p, patches)
    expected_map = {}

    self.assertDictEqual(dep_map, expected_map)

  def testGetDependMapForChangesOnMutualDependency(self):
    """Test GetDependMapForChanges on mutual dependency."""
    pool = self.MakePool()
    patches = patch_series.PatchSeries('path')
    p = self.GetPatches(how_many=2)

    for patch in p:
      self.patch_mock.SetGerritDependencies(patch, [])

    # p1 -> p0, p0 -> p1
    self.patch_mock.SetCQDependencies(p[0], [p[1]])
    self.patch_mock.SetCQDependencies(p[1], [p[0]])

    dep_map = pool.GetDependMapForChanges(p, patches)
    expected_map = {
        p[0]: {p[1]},
        p[1]: {p[0]}
    }
    self.assertDictEqual(dep_map, expected_map)

  def testGetDependMapForChangesOnCircularDependency(self):
    """Test GetDependMapForChanges on circular dependency."""
    pool = self.MakePool()
    patches = patch_series.PatchSeries('path')
    p = self.GetPatches(how_many=3)

    for patch in p:
      self.patch_mock.SetGerritDependencies(patch, [])

    # p0 -> p2, p1 -> p2, p2 -> p0
    self.patch_mock.SetCQDependencies(p[0], [p[1]])
    self.patch_mock.SetCQDependencies(p[1], [p[2]])
    self.patch_mock.SetCQDependencies(p[2], [p[0]])

    dep_map = pool.GetDependMapForChanges(p, patches)
    expected_map = {
        p[0]: {p[1], p[2]},
        p[1]: {p[0], p[2]},
        p[2]: {p[0], p[1]}
    }
    self.assertDictEqual(dep_map, expected_map)

  def testGetDependMapForChangesOnTransitiveDependency(self):
    """Test GetDependMapForChanges on transitive dependency."""
    pool = self.MakePool()
    patches = patch_series.PatchSeries('path')
    p = self.GetPatches(how_many=4)

    for patch in p:
      self.patch_mock.SetGerritDependencies(patch, [])

    # p1 -> p0, p2 -> p1
    self.patch_mock.SetCQDependencies(p[0], [])
    self.patch_mock.SetCQDependencies(p[1], [p[0]])
    self.patch_mock.SetCQDependencies(p[2], [p[1]])
    self.patch_mock.SetCQDependencies(p[3], [])

    dep_map = pool.GetDependMapForChanges(p, patches)
    expected_map = {
        p[0]: {p[1], p[2]},
        p[1]: {p[2]}
    }
    self.assertDictEqual(dep_map, expected_map)

  def testGetDependMapForDeepGraph(self):
    r"""Create a graph which would take exponential time naively.

    The graph looks like this:
        a_1
       /   \
      b_1  c_1
       \  /
        a_2
       /   \
      b_2  c_2
       \  /
        a_3
        ...100 similar chains
       /  \
      b_100 c_100
    """
    pool = self.MakePool()
    patches = patch_series.PatchSeries('path')
    p = self.GetPatches(how_many=3 * 100)

    for patch in p:
      self.patch_mock.SetGerritDependencies(patch, [])

    p_iter = iter(p)

    a, b, c = None, None, None
    chunks = []

    while True:
      a = next(p_iter, None)
      if not a:
        break
      # Chain the previous block to this block.
      if c:
        self.patch_mock.SetCQDependencies(b, [a])
        self.patch_mock.SetCQDependencies(c, [a])
      b, c = next(p_iter), next(p_iter)
      self.patch_mock.SetCQDependencies(a, [b, c])
      chunks.append((a, b, c))

    transitive_dependencies = pool.GetDependMapForChanges(p, patches)

    for i in range(len(chunks) - 1, -1, -1):
      a, b, c = chunks[i]
      nodes_above = set(p for chunk in chunks[0:i] for p in chunk)
      self.assertEqual(nodes_above,
                       transitive_dependencies.get(a, set()))


  def testGetDependMapForChangesComplex(self):
    """Test GetDependMapForChanges on complex graph.

    This should not hang.
    """
    pool = self.MakePool()
    patches = patch_series.PatchSeries('path')
    p = self.GetPatches(how_many=53)

    for patch in p:
      self.patch_mock.SetGerritDependencies(patch, [])

    self.patch_mock.SetCQDependencies(p[0], [])
    self.patch_mock.SetCQDependencies(p[1], [p[2]])
    self.patch_mock.SetCQDependencies(p[2], [p[1]])
    self.patch_mock.SetCQDependencies(p[3], [])
    self.patch_mock.SetCQDependencies(p[4], [])
    self.patch_mock.SetCQDependencies(p[5], [])
    self.patch_mock.SetCQDependencies(p[6], [p[7]])
    self.patch_mock.SetCQDependencies(p[7], [p[22]])
    self.patch_mock.SetCQDependencies(p[8], [p[9]])
    self.patch_mock.SetCQDependencies(p[9], [p[29]])
    self.patch_mock.SetCQDependencies(p[10], [p[11]])
    self.patch_mock.SetCQDependencies(p[11], [p[8]])
    self.patch_mock.SetCQDependencies(p[12], [p[13]])
    self.patch_mock.SetCQDependencies(p[13], [p[30]])
    self.patch_mock.SetCQDependencies(p[14], [p[12]])
    self.patch_mock.SetCQDependencies(p[15], [p[16]])
    self.patch_mock.SetCQDependencies(p[16], [p[17]])
    self.patch_mock.SetCQDependencies(p[17], [p[18]])
    self.patch_mock.SetCQDependencies(p[18], [p[14]])
    self.patch_mock.SetCQDependencies(p[19], [p[20]])
    self.patch_mock.SetCQDependencies(p[20], [p[48], p[31]])
    self.patch_mock.SetCQDependencies(p[21], [p[19]])
    self.patch_mock.SetCQDependencies(p[22], [p[21]])
    self.patch_mock.SetCQDependencies(p[23], [p[6]])
    self.patch_mock.SetCQDependencies(p[24], [p[23]])
    self.patch_mock.SetCQDependencies(p[25], [p[24]])
    self.patch_mock.SetCQDependencies(p[26], [p[25]])
    self.patch_mock.SetCQDependencies(p[27], [p[26]])
    self.patch_mock.SetCQDependencies(p[28], [p[27]])
    self.patch_mock.SetCQDependencies(p[29], [p[42]])
    self.patch_mock.SetCQDependencies(p[30], [p[10]])
    self.patch_mock.SetCQDependencies(p[31], [p[15]])
    self.patch_mock.SetCQDependencies(p[32], [p[31]])
    self.patch_mock.SetCQDependencies(p[33], [p[32]])
    self.patch_mock.SetCQDependencies(p[34], [p[33]])
    self.patch_mock.SetCQDependencies(p[35], [p[34]])
    self.patch_mock.SetCQDependencies(p[36], [])
    self.patch_mock.SetCQDependencies(p[37], [])
    self.patch_mock.SetCQDependencies(p[38], [])
    self.patch_mock.SetCQDependencies(p[39], [p[12]])
    self.patch_mock.SetCQDependencies(p[40], [p[46], p[12]])
    self.patch_mock.SetCQDependencies(p[41], [p[40], p[14]])
    self.patch_mock.SetCQDependencies(p[42], [p[28]])
    self.patch_mock.SetCQDependencies(p[43], [p[49]])
    self.patch_mock.SetCQDependencies(p[44], [p[43], p[35]])
    self.patch_mock.SetCQDependencies(p[45], [p[44], p[18]])
    self.patch_mock.SetCQDependencies(p[46], [p[45], p[13]])
    self.patch_mock.SetCQDependencies(p[47], [p[41], p[11]])
    self.patch_mock.SetCQDependencies(p[48], [p[47], p[17]])
    self.patch_mock.SetCQDependencies(p[49], [p[35]])
    self.patch_mock.SetCQDependencies(p[50], [])
    self.patch_mock.SetCQDependencies(p[51], [])
    self.patch_mock.SetCQDependencies(p[52], [p[37]])

    m = pool.GetDependMapForChanges(p, patches)

    self.assertEqual(None, m.get(p[0]))
    self.assertEqual(set([p[2]]), m[p[1]])
    self.assertEqual(set([p[1]]), m[p[2]])
    self.assertEqual(set([p[1]]), m[p[2]])

    loop = set([6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 39, 40, 41,
                42, 43, 44, 45, 46, 47, 48, 49])

    # patch 39 links into the loop, but doesn't get depended on by anything.
    self.assertEqual(None, m.get(p[39]))

    # All the other patches are in a strongly connected component, so their
    # transitive dependency set contains every other patch in the loop.
    for i in loop - set([39]):
      self.assertEqual(m[p[i]], set(p[j] for j in loop if j != i))

  def testHasPickedUpCLs(self):
    """Test HasPickedUpCLs."""
    pool = self.MakePool()
    self.assertFalse(pool.HasPickedUpCLs())
    pool.has_chump_cls = True
    self.assertTrue(pool.HasPickedUpCLs())


class TestPickling(cros_test_lib.TempDirTestCase):
  """Tests to validate pickling of ValidationPool, covering CQ's needs"""

  def testSelfCompatibility(self):
    """Verify compatibility of current git HEAD against itself."""
    self._CheckTestData(self._GetTestData())

  @cros_test_lib.NetworkTest()
  def testToTCompatibility(self):
    """Validate that ToT can use our pickles, and that we can use ToT's data."""
    repo = os.path.join(self.tempdir, 'chromite')
    reference = os.path.abspath(__file__)
    reference = os.path.normpath(os.path.join(reference, '../../'))

    repository.CloneGitRepo(
        repo,
        '%s/chromiumos/chromite' % site_config.params.EXTERNAL_GOB_URL,
        reference=reference)

    code = """
import sys
from chromite.cbuildbot import validation_pool_unittest
if not hasattr(validation_pool_unittest, 'TestPickling'):
  sys.exit(0)
sys.stdout.write(validation_pool_unittest.TestPickling.%s)
"""

    # Verify ToT can take our pickle.
    cros_build_lib.RunCommand(
        ['python2', '-c', code % '_CheckTestData(sys.stdin.read())'],
        cwd=self.tempdir, print_cmd=False, capture_output=True,
        input=self._GetTestData())

    # Verify we can handle ToT's pickle.
    ret = cros_build_lib.RunCommand(
        ['python2', '-c', code % '_GetTestData()'],
        cwd=self.tempdir, print_cmd=False, capture_output=True)

    self._CheckTestData(ret.output)

  @staticmethod
  def _GetCrosInternalPatch(patch_info):
    return cros_patch.GerritPatch(
        patch_info,
        site_config.params.INTERNAL_REMOTE,
        site_config.params.INTERNAL_GERRIT_URL)

  @staticmethod
  def _GetCrosPatch(patch_info):
    return cros_patch.GerritPatch(
        patch_info,
        site_config.params.EXTERNAL_REMOTE,
        site_config.params.EXTERNAL_GERRIT_URL)

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
        candidates=changes, non_os_changes=non_os,
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
    _f(pool.candidates, changes)
    _f(pool.non_manifest_changes, non_os)
    _f(pool.changes_that_failed_to_apply_earlier, conflicting,
       getter=lambda s: getattr(s, 'patch', s))
    return ''


class TestPrintLinks(MoxBase):
  """Tests that change links can be printed."""
  def testPrintLinks(self):
    changes = self.GetPatches(3)
    changes[1].owner_email = 'foo@bar.com'
    changes[2].owner_email = None
    # TODO: Validate the output, don't just print it out to inspect by hand.
    with parallel_unittest.ParallelMock():
      validation_pool.ValidationPool.PrintLinksToChanges(changes)


class TestCreateDisjointTransactions(MoxBase):
  """Test the CreateDisjointTransactions function."""

  def setUp(self):
    self.patch_mock = self.StartPatcher(MockPatchSeries())

  def GetPatches(self, how_many, **kwargs):
    return super(TestCreateDisjointTransactions, self).GetPatches(
        how_many, always_use_list=True, **kwargs)

  def verifyTransactions(self, txns, max_txn_length=None, circular=False):
    """Verify the specified list of transactions are processed correctly.

    Args:
      txns: List of transactions to process.
      max_txn_length: Maximum length of any given transaction. This is passed
        to the CreateDisjointTransactions function.
      circular: Whether the transactions contain circular dependencies.
    """
    remove = self.PatchObject(gerrit.GerritHelper, 'RemoveReady')
    patches = list(itertools.chain.from_iterable(txns))
    expected_plans = txns
    if max_txn_length is not None:
      # When max_txn_length is specified, transactions should be truncated to
      # the specified length, ignoring any remaining patches.
      expected_plans = [txn[:max_txn_length] for txn in txns]

    pool = MakePool(candidates=patches)
    plans = pool.CreateDisjointTransactions(None, pool.candidates,
                                            max_txn_length=max_txn_length)

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
    remove = self.PatchObject(gerrit.GerritHelper, 'RemoveReady')
    pool = MakePool(candidates=changes)
    plans = pool.CreateDisjointTransactions(None, changes,
                                            max_txn_length=max_txn_length)
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

  TARGET = 'chromite.cbuildbot.validation_pool.ValidationPool'
  ATTRS = ('RemoveReady', '_SubmitChangeUsingGerrit', 'SendNotification')

  def __init__(self, manager):
    partial_mock.PartialMock.__init__(self)
    self.submit_results = {}
    self.max_submits = manager.Value('i', -1)
    self.submitted = manager.list()
    self.notification_calls = manager.list()

  def PreStart(self):
    self.PatchObject(gerrit, 'GetGerritPatchInfoWithPatchQueries',
                     side_effect=lambda x: x)

  def GetSubmittedChanges(self):
    return list(self.submitted)

  # pylint: disable=unused-argument
  def _SubmitChangeUsingGerrit(self, _inst, change, reason=None):
    result = self.submit_results.get(change, True)
    self.submitted.append(change)
    if isinstance(result, Exception):
      raise result
    if result and self.max_submits.value != -1:
      if self.max_submits.value <= 0:
        return False
      self.max_submits.value -= 1
    return result

  def SendNotification(self, *args, **kwargs):
    self.notification_calls.append((args, kwargs))

  RemoveReady = None


class BaseSubmitPoolTestCase(MoxBase):
  """Test full ability to submit and reject CL pools."""

  # Whether all slave builds passed. This would affect the submission
  # logic.
  ALL_BUILDS_PASSED = True

  def setUp(self):
    self.pool_mock = self.StartPatcher(MockValidationPool(self.manager))
    self.patch_mock = self.StartPatcher(MockPatchSeries())
    self.PatchObject(gerrit.GerritHelper, 'QuerySingleRecord')
    # This is patched out for performance, not correctness.
    self.PatchObject(patch_series.PatchSeries, 'ReapplyChanges',
                     lambda self, by_repo: (by_repo, {}))
    self.patches = self.GetPatches(2)

  def SetUpPatchPool(self, failed_to_apply=False):
    pool = MakePool(candidates=self.patches, dryrun=False)
    if failed_to_apply:
      # Create some phony errors and add them to the pool.
      errors = []
      for patch in self.GetPatches(2):
        errors.append(validation_pool.InternalCQError(patch, str('foo')))
      pool.changes_that_failed_to_apply_earlier = errors[:]
    return pool

  def SubmitPool(self, submitted=(), rejected=(), reason=None, **kwargs):
    """Helper function for testing that we can submit a pool successfully.

    Args:
      submitted: List of changes that we expect to be submitted.
      rejected: List of changes that we expect to be rejected.
      reason: Expected reason for submitting changes.
      **kwargs: Keyword arguments for SetUpPatchPool.
    """
    # Set up our pool and submit the patches.
    pool = self.SetUpPatchPool(**kwargs)
    mock_manifest = mock.MagicMock()
    with mock.patch.object(git.ManifestCheckout, 'Cached', new=mock_manifest):
      if not self.ALL_BUILDS_PASSED:
        actually_rejected = sorted(pool.SubmitPartialPool(
            pool.candidates, mock.ANY, dict(), dict(), dict(), [], [], []))
      else:
        verified_cls = {c:reason for c in self.patches}
        _, actually_rejected = pool.SubmitChanges(verified_cls)

    # Check that the right patches were submitted and rejected.
    self.assertItemsEqual(map(str, rejected), map(str, actually_rejected))
    actually_submitted = self.pool_mock.GetSubmittedChanges()
    self.assertEqual(map(str, submitted), map(str, actually_submitted))

  def GetNotifyArg(self, change, key):
    """Look up a call to notify about |change| and grab |key| from it.

    Args:
      change: The change to look up.
      key: The key to look up. If this is an integer, look up a positional
        argument by index. Otherwise, look up a keyword argument.
    """
    names = []
    for call in self.pool_mock.notification_calls:
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


class SubmitPoolTest(BaseSubmitPoolTestCase):
  """Test suite related to the Submit Pool."""

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

  def testSubmitEmptyDeps(self):
    """Submit when one patch depends directly on many independent patches."""
    # patches[4] depends on patches[0:3], but there are no other dependencies.
    self.patches = self.GetPatches(5)
    for p in self.patches[:-1]:
      self.patch_mock.SetGerritDependencies(p, [])
    self.patch_mock.SetGerritDependencies(self.patches[4], self.patches[::-1])
    self.pool_mock.max_submits.value = 1
    submitted = [self.patches[2], self.patches[1], self.patches[3],
                 self.patches[0]]
    rejected = self.patches[:2] + self.patches[3:]
    self.SubmitPool(submitted=submitted, rejected=rejected)
    for p in rejected[:-1]:
      p_failed_submit = validation_pool.PatchFailedToSubmit(
          p, validation_pool.ValidationPool.INCONSISTENT_SUBMIT_MSG)
      self.assertEqualNotifyArg(p_failed_submit, p, 'error')
    failed_submit = validation_pool.PatchFailedToSubmit(
        self.patches[1], validation_pool.ValidationPool.INCONSISTENT_SUBMIT_MSG)
    dep_failed = cros_patch.DependencyError(self.patches[4], failed_submit)
    self.assertEqualNotifyArg(dep_failed, self.patches[4], 'error')

  def testRedundantCQDepend(self):
    """Submit a cycle with redundant CQ-DEPEND specifications."""
    self.patches = self.GetPatches(4)
    self.patch_mock.SetCQDependencies(self.patches[0], [self.patches[-1]])
    self.patch_mock.SetCQDependencies(self.patches[1], [self.patches[-1]])
    self.SubmitPool(submitted=self.patches)

  def testSubmitPartialCycle(self):
    """Submit a failed cyclic set of dependencies"""
    self.pool_mock.max_submits.value = 1
    self.patch_mock.SetCQDependencies(self.patches[0], self.patches)
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
    self.pool_mock.max_submits.value = 0
    self.patch_mock.SetCQDependencies(self.patches[0], [self.patches[1]])
    self.SubmitPool(submitted=[self.patches[0]], rejected=self.patches)
    (attempted,) = self.pool_mock.GetSubmittedChanges()
    (rejected,) = [x for x in self.patches if x.id != attempted.id]
    failed_submit = validation_pool.PatchFailedToSubmit(
        attempted, validation_pool.ValidationPool.INCONSISTENT_SUBMIT_MSG)
    dep_failed = cros_patch.DependencyError(rejected, failed_submit)
    self.assertEqualNotifyArg(failed_submit, attempted, 'error')
    self.assertEqualNotifyArg(dep_failed, rejected, 'error')

  def testConflict(self):
    """Submit a change that conflicts with TOT."""
    error = gob_util.GOBError(http_status=httplib.CONFLICT, reason='Conflict')
    self.pool_mock.submit_results[self.patches[0]] = error
    self.SubmitPool(submitted=[self.patches[0]], rejected=self.patches[::-1])
    notify_error = validation_pool.PatchConflict(self.patches[0])
    self.assertEqualNotifyArg(notify_error, self.patches[0], 'error')

  def testConflictAlreadyMerged(self):
    """Submit a change that conflicts with TOT because it was already merged."""
    error = gob_util.GOBError(http_status=httplib.CONFLICT,
                              reason=gob_util.GOB_ERROR_REASON_CLOSED_CHANGE)
    self.pool_mock.submit_results[self.patches[0]] = error
    self.SubmitPool(submitted=self.patches, rejected=())

  def testServerError(self):
    """Test case where GOB returns a server error."""
    error = gerrit.GerritException('Internal server error')
    self.pool_mock.submit_results[self.patches[0]] = error
    self.SubmitPool(submitted=[self.patches[0]], rejected=self.patches[::-1])
    notify_error = validation_pool.PatchFailedToSubmit(self.patches[0], error)
    self.assertEqualNotifyArg(notify_error, self.patches[0], 'error')

  def testNotCommitReady(self):
    """Test that a CL is rejected if its approvals were pulled."""
    def _ReloadPatches(patches):
      reloaded = copy.deepcopy(patches)
      approvals = {('VRIF', '1'): False}
      backup = reloaded[1].HasApproval
      self.PatchObject(
          reloaded[1], 'HasApproval',
          side_effect=lambda *args: approvals.get(args, backup(*args)))
      return reloaded
    self.PatchObject(gerrit, 'GetGerritPatchInfoWithPatchQueries',
                     _ReloadPatches)
    self.SubmitPool(submitted=self.patches[:1], rejected=self.patches[1:])
    message = 'CL:2 is not marked Verified=+1.'
    self.assertEqualNotifyArg(message, self.patches[1], 'error')

  def testAlreadyMerged(self):
    """Test that a CL that was chumped during the run was not rejected."""
    self.PatchObject(self.patches[0], 'IsAlreadyMerged', return_value=True)
    self.SubmitPool(submitted=self.patches[1:], rejected=[])

  def testModified(self):
    """Test that a CL that was modified during the run is rejected."""
    def _ReloadPatches(patches):
      reloaded = copy.deepcopy(patches)
      reloaded[1].patch_number += 1
      return reloaded
    self.PatchObject(gerrit, 'GetGerritPatchInfoWithPatchQueries',
                     _ReloadPatches)
    self.SubmitPool(submitted=self.patches[:1], rejected=self.patches[1:])
    error = validation_pool.PatchModified(self.patches[1],
                                          self.patches[1].patch_number + 1)
    self.assertEqualNotifyArg(error, self.patches[1], 'error')


class SubmitPartialPoolTest(BaseSubmitPoolTestCase):
  """Test the SubmitPartialPool function."""

  # Whether all slave builds passed. This would affect the submission
  # logic.
  ALL_BUILDS_PASSED = False

  def setUp(self):
    # Set up each patch to be in its own project, so that we can easily
    # request to ignore failures for the specified patch.
    for patch in self.patches:
      patch.project = str(patch)

    self.verified_mock = self.PatchObject(
        triage_lib.CalculateSuspects, 'GetFullyVerifiedChanges',
        return_value={})

  def _MarkPatchesVerified(self, patches):
    """Set up to mark |patches| as verified."""
    verified_patches = {c:None for c in patches}
    self.verified_mock.return_value = verified_patches

  def testSubmitNone(self):
    """Submit no changes."""
    self.SubmitPool(submitted=(), rejected=self.patches)

  def testSubmitAll(self):
    """Submit all changes."""
    self._MarkPatchesVerified(self.patches[:2])
    self.SubmitPool(submitted=self.patches, rejected=[])

  def testSubmitFirst(self):
    """Submit the first change in a series."""
    self._MarkPatchesVerified([self.patches[0]])
    self.SubmitPool(submitted=[self.patches[0]], rejected=[self.patches[1]])
    self.assertEqual(len(self.pool_mock.notification_calls), 0)

  def testSubmitSecond(self):
    """Attempt to submit the second change in a series."""
    self._MarkPatchesVerified([self.patches[1]])
    self.SubmitPool(submitted=[], rejected=[self.patches[0]])
    error = patch_series.PatchRejected(self.patches[0])
    dep_error = cros_patch.DependencyError(self.patches[1], error)
    self.assertEqualNotifyArg(dep_error, self.patches[1], 'error')
    self.assertEqual(len(self.pool_mock.notification_calls), 1)


class LoadManifestTest(cros_test_lib.TempDirTestCase):
  """Tests loading the manifest."""

  manifest_content = (
      '<?xml version="1.0" ?><manifest>'
      '<pending_commit branch="master" '
      'change_id="Ieeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee1" '
      'commit="1ddddddddddddddddddddddddddddddddddddddd" '
      'fail_count="2" gerrit_number="17000" owner_email="foo@chromium.org" '
      'pass_count="0" patch_number="2" project="chromiumos/taco/bar" '
      'project_url="https://base_url/chromiumos/taco/bar" '
      'ref="refs/changes/51/17000/2" remote="cros" total_fail_count="3"/>'
      '</manifest>')

  def setUp(self):
    """Sets up a pool."""
    self.pool = MakePool()

  def testAddPendingCommitsIntoPool(self):
    """Test reading the pending commits and add them into the pool."""
    with tempfile.NamedTemporaryFile() as f:
      f.write(self.manifest_content)
      f.flush()
      self.pool.AddPendingCommitsIntoPool(f.name)

    self.assertEqual(self.pool.candidates[0].owner_email, 'foo@chromium.org')
    self.assertEqual(self.pool.candidates[0].tracking_branch, 'master')
    self.assertEqual(self.pool.candidates[0].remote, 'cros')
    self.assertEqual(self.pool.candidates[0].gerrit_number, '17000')
    self.assertEqual(self.pool.candidates[0].project, 'chromiumos/taco/bar')
    self.assertEqual(self.pool.candidates[0].project_url,
                     'https://base_url/chromiumos/taco/bar')
    self.assertEqual(self.pool.candidates[0].change_id,
                     'Ieeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee1')
    self.assertEqual(self.pool.candidates[0].commit,
                     '1ddddddddddddddddddddddddddddddddddddddd')
    self.assertEqual(self.pool.candidates[0].fail_count, 2)
    self.assertEqual(self.pool.candidates[0].pass_count, 0)
    self.assertEqual(self.pool.candidates[0].total_fail_count, 3)


class RemoveReadyTest(cros_test_lib.MockTempDirTestCase):
  """Tests for RemoveReady."""

  def setUp(self):
    """Sets up a pool."""
    self.pool = MakePool()

  def testRemoveReadyRaisesException(self):
    """Test RemoveReady which raises exception."""
    helper_mock = mock.Mock()
    helper_mock.ForChange.return_value.RemoveReady.side_effect = (
        gob_util.GOBError(http_status=409, reason="test"))
    self.pool._helper_pool = helper_mock

    self.assertRaises(gob_util.GOBError, self.pool.RemoveReady,
                      mock.Mock)

  def testRemoveReadyDoesNotRaiseException(self):
    """Test RemoveReady which does not raise exception."""
    helper_mock = mock.Mock()
    helper_mock.ForChange.return_value.RemoveReady.side_effect = (
        gob_util.GOBError(http_status=409,
                          reason=gob_util.GOB_ERROR_REASON_CLOSED_CHANGE))
    self.pool._helper_pool = helper_mock

    self.pool._run = None
    self.PatchObject(validation_pool.ValidationPool,
                     '_InsertCLActionToDatabase')

    self.pool.RemoveReady(mock.Mock())
