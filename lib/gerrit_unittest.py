#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for GerritHelper."""

import mock
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gerrit
from chromite.lib import gob_util


# pylint: disable=W0212,R0904
class GerritHelperTest(cros_test_lib.GerritTestCase):
  """Unittests for GerritHelper."""

  def _GetHelper(self, remote=constants.EXTERNAL_REMOTE):
    return gerrit.GetGerritHelper(remote)

  def test001SimpleQuery(self):
    """Create one independent and three dependent changes, then query them."""
    self.createProject('test001')
    clone_path = self.cloneProject('test001')
    (head_sha1, head_changeid) = self.createCommit(clone_path)
    for idx in xrange(3):
      cros_build_lib.RunCommand(
          ['git', 'checkout', head_sha1], cwd=clone_path, quiet=True)
      self.createCommit(clone_path, fn='test-file-%d.txt' % idx)
      self.uploadChange(clone_path)
    helper = self._GetHelper()
    changes = helper.Query(owner='self')
    self.assertEqual(len(changes), 4)
    changes = helper.Query(head_changeid, project='test001', branch='master')
    self.assertEqual(len(changes), 1)
    self.assertEqual(changes[0].change_id, head_changeid)
    self.assertEqual(changes[0].sha1, head_sha1)
    change = helper.QuerySingleRecord(
        head_changeid, project='test001', branch='master')
    self.assertTrue(change)
    self.assertEqual(change.change_id, head_changeid)
    self.assertEqual(change.sha1, head_sha1)
    change = helper.GrabPatchFromGerrit('test001', head_changeid, head_sha1)
    self.assertTrue(change)
    self.assertEqual(change.change_id, head_changeid)
    self.assertEqual(change.sha1, head_sha1)

  @mock.patch.object(gerrit.GerritHelper, '_GERRIT_MAX_QUERY_RETURN', 20)
  def test002GerritQueryTruncation(self):
    """Verify that we detect gerrit truncating our query, and handle it."""
    self.createProject('test002')
    clone_path = self.cloneProject('test002')
    # Using a shell loop is markedly faster than running a python loop,
    # but gerrit can throw timeout errors when pushing too many changes.
    num_changes = 84
    for shard in range(0, num_changes, 10):
      cmd = ('for ((i=%i; i<%i; i=i+1)); do '
             'echo "Another day, another dollar." > test-file-$i.txt; '
             'git add test-file-$i.txt; '
             'git commit -m "Test commit $i."; '
             'done' % (shard, min(shard+10, num_changes)))
      cros_build_lib.RunCommand(cmd, shell=True, cwd=clone_path, quiet=True)
      self.uploadChange(clone_path)
    helper = self._GetHelper()
    changes = helper.Query(project='test002')
    self.assertEqual(len(changes), num_changes)

  def test003IsChangeCommitted(self):
    """Tests that we can parse a json to check if a change is committed."""
    self.createProject('test003')
    clone_path = self.cloneProject('test003')
    (_, changeid) = self.createCommit(clone_path)
    self.uploadChange(clone_path)
    helper = self._GetHelper()
    gpatch = helper.QuerySingleRecord(
        change=changeid, project='test003', branch='master')
    self.assertEqual(gpatch.change_id, changeid)
    helper.SetReview(gpatch.gerrit_number, labels={'Code-Review':'+2'})
    helper.SubmitChange(gpatch)
    self.assertTrue(helper.IsChangeCommitted(gpatch.gerrit_number))

    (_, changeid) = self.createCommit(clone_path)
    self.uploadChange(clone_path)
    gpatch = helper.QuerySingleRecord(
        change=changeid, project='test003', branch='master')
    self.assertEqual(gpatch.change_id, changeid)
    self.assertFalse(helper.IsChangeCommitted(gpatch.gerrit_number))

  def test004GetLatestSHA1ForBranch(self):
    """Verifies that we can query the tip-of-tree commit in a git repository."""
    self.createProject('test004')
    clone_path = self.cloneProject('test004')
    for _ in xrange(5):
      (master_sha1, _) = self.createCommit(clone_path)
    self.pushBranch(clone_path, 'master')
    for _ in xrange(5):
      (testbranch_sha1, _) = self.createCommit(clone_path)
    self.pushBranch(clone_path, 'testbranch')
    helper = self._GetHelper()
    self.assertEqual(
        helper.GetLatestSHA1ForBranch('test004', 'master'),
        master_sha1)
    self.assertEqual(
        helper.GetLatestSHA1ForBranch('test004', 'testbranch'),
        testbranch_sha1)

  def test005SetReviewers(self):
    """Verify that we can set reviewers on a CL."""
    self.createProject('test005')
    clone_path = self.cloneProject('test005')
    (_, changeid) = self.createCommit(clone_path)
    self.uploadChange(clone_path)
    helper = self._GetHelper()
    gpatch = helper.QuerySingleRecord(
        change=changeid, project='test005', branch='master')
    self.assertEqual(gpatch.change_id, changeid)
    self.createAccount(name='Test005 User 1', email='test005-user-1@test.org')
    self.createAccount(name='Test005 User 2', email='test005-user-2@test.org')
    helper = self._GetHelper()
    helper.SetReviewers(gpatch.gerrit_number, add=(
        'test005-user-1@test.org', 'test005-user-2@test.org'))
    reviewers = gob_util.GetReviewers(helper.host, gpatch.gerrit_number)
    self.assertEqual(len(reviewers), 2)
    self.assertItemsEqual(
        [r['email'] for r in reviewers],
        ['test005-user-1@test.org', 'test005-user-2@test.org'])
    helper.SetReviewers(gpatch.gerrit_number,
                        remove=('test005-user-1@test.org',))
    reviewers = gob_util.GetReviewers(helper.host, gpatch.gerrit_number)
    self.assertEqual(len(reviewers), 1)
    self.assertEqual(reviewers[0]['email'], 'test005-user-2@test.org')

  def test006PatchNotFound(self):
    """Test case where ChangeID isn't found on the server."""
    changeids = ['I' + ('deadbeef' * 5), 'I' + ('beadface' * 5)]
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      changeids)
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      ['*' + cid for cid in changeids])
    gerrit_numbers = ['12345', '54321']
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      gerrit_numbers)
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      ['*' + num for num in gerrit_numbers])

  def test007VagueQuery(self):
    """Verify GerritHelper complains if an ID matches multiple changes."""
    self.createProject('test007')
    clone_path = self.cloneProject('test007')
    (sha1, _) = self.createCommit(clone_path)
    (_, changeid) = self.createCommit(clone_path)
    self.uploadChange(clone_path, 'master')
    cros_build_lib.RunCommand(
        ['git', 'checkout', sha1], cwd=clone_path, quiet=True)
    self.createCommit(clone_path)
    self.pushBranch(clone_path, 'testbranch')
    self.createCommit(
        clone_path, msg='Test commit.\n\nChange-Id: %s' % changeid)
    self.uploadChange(clone_path, 'testbranch')
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      [changeid])

  def test008Queries(self):
    """Verify assorted query operations."""
    self.createProject('test008')
    clone_path = self.cloneProject('test008')
    (sha1, changeid) = self.createCommit(clone_path)
    self.uploadChange(clone_path, 'master')
    helper = self._GetHelper()
    gpatch = helper.QuerySingleRecord(
        change=changeid, project='test008', branch='master')
    self.assertEqual(gpatch.change_id, changeid)

    # Multi-queries with one valid and one invalid term should raise.
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      ['Iab8e1d', changeid])
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      [changeid, 'Iab8e1d'])
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      ['12345', gpatch.gerrit_number])
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      [gpatch.gerrit_number, '12345'])

    # Simple query by project/changeid/sha1.
    patch_info = helper.GrabPatchFromGerrit('test008', changeid, sha1)
    self.assertEqual(patch_info.gerrit_number, gpatch.gerrit_number)
    self.assertEqual(patch_info.remote, constants.EXTERNAL_REMOTE)

    # Simple query by gerrit number to external remote.
    patch_info = gerrit.GetGerritPatchInfo([gpatch.gerrit_number])
    self.assertEqual(patch_info[0].gerrit_number, gpatch.gerrit_number)
    self.assertEqual(patch_info[0].remote, constants.EXTERNAL_REMOTE)

    # Simple query by gerrit number to internal remote.
    patch_info = gerrit.GetGerritPatchInfo(['*' + gpatch.gerrit_number])
    self.assertEqual(patch_info[0].gerrit_number, gpatch.gerrit_number)
    self.assertEqual(patch_info[0].remote, constants.INTERNAL_REMOTE)

    # Query to external server by gerrit number and change-id which refer to
    # the same change should return one result.
    fq_changeid = '~'.join((gpatch.project, 'master', gpatch.change_id))
    patch_info = gerrit.GetGerritPatchInfo([gpatch.gerrit_number, fq_changeid])
    self.assertEqual(len(patch_info), 1)
    self.assertEqual(patch_info[0].gerrit_number, gpatch.gerrit_number)
    self.assertEqual(patch_info[0].remote, constants.EXTERNAL_REMOTE)

    # Query to internal server by gerrit number and change-id which refer to
    # the same change should return one result.
    patch_info = gerrit.GetGerritPatchInfo(
        ['*' + gpatch.gerrit_number, '*' + fq_changeid])
    self.assertEqual(len(patch_info), 1)
    self.assertEqual(patch_info[0].gerrit_number, gpatch.gerrit_number)
    self.assertEqual(patch_info[0].remote, constants.INTERNAL_REMOTE)


if __name__ == '__main__':
  cros_test_lib.main()
