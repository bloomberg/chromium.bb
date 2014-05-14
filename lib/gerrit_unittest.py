#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for GerritHelper."""

import getpass
import httplib
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gerrit
from chromite.lib import gob_util

import mock


# pylint: disable=W0212,R0904
class GerritHelperTest(cros_test_lib.GerritTestCase):
  """Unittests for GerritHelper."""

  def _GetHelper(self, remote=constants.EXTERNAL_REMOTE):
    return gerrit.GetGerritHelper(remote)

  def createPatch(self, clone_path, project, amend=False):
    """Create a patch in the given git checkout and upload it to gerrit.

    Args:
      clone_path: The directory on disk of the git clone.
      project: The associated project.
      amend: Whether to amend an existing patch. If set, we will amend the
        HEAD commit in the checkout and upload that patch.

    Returns:
      A GerritPatch object.
    """
    (revision, changeid) = self.createCommit(clone_path, amend=amend)
    self.uploadChange(clone_path)
    gpatch = self._GetHelper().QuerySingleRecord(
        change=changeid, project=project, branch='master')
    self.assertEqual(gpatch.change_id, changeid)
    self.assertEqual(gpatch.revision, revision)
    return gpatch

  @cros_test_lib.NetworkTest()
  def test001SimpleQuery(self):
    """Create one independent and three dependent changes, then query them."""
    project = self.createProject('test001')
    clone_path = self.cloneProject(project)
    (head_sha1, head_changeid) = self.createCommit(clone_path)
    for idx in xrange(3):
      cros_build_lib.RunCommand(
          ['git', 'checkout', head_sha1], cwd=clone_path, quiet=True)
      self.createCommit(clone_path, fn='test-file-%d.txt' % idx)
      self.uploadChange(clone_path)
    helper = self._GetHelper()
    changes = helper.Query(owner='self', project=project)
    self.assertEqual(len(changes), 4)
    changes = helper.Query(head_changeid, project=project, branch='master')
    self.assertEqual(len(changes), 1)
    self.assertEqual(changes[0].change_id, head_changeid)
    self.assertEqual(changes[0].sha1, head_sha1)
    change = helper.QuerySingleRecord(
        head_changeid, project=project, branch='master')
    self.assertTrue(change)
    self.assertEqual(change.change_id, head_changeid)
    self.assertEqual(change.sha1, head_sha1)
    change = helper.GrabPatchFromGerrit(project, head_changeid, head_sha1)
    self.assertTrue(change)
    self.assertEqual(change.change_id, head_changeid)
    self.assertEqual(change.sha1, head_sha1)

  @cros_test_lib.NetworkTest()
  @mock.patch.object(gerrit.GerritHelper, '_GERRIT_MAX_QUERY_RETURN', 2)
  def test002GerritQueryTruncation(self):
    """Verify that we detect gerrit truncating our query, and handle it."""
    project = self.createProject('test002')
    clone_path = self.cloneProject(project)
    # Using a shell loop is markedly faster than running a python loop.
    num_changes = 5
    cmd = ('for ((i=0; i<%i; i=i+1)); do '
           'echo "Another day, another dollar." > test-file-$i.txt; '
           'git add test-file-$i.txt; '
           'git commit -m "Test commit $i."; '
           'done' % num_changes)
    cros_build_lib.RunCommand(cmd, shell=True, cwd=clone_path, quiet=True)
    self.uploadChange(clone_path)
    helper = self._GetHelper()
    changes = helper.Query(project=project)
    self.assertEqual(len(changes), num_changes)

  @cros_test_lib.NetworkTest()
  def test003IsChangeCommitted(self):
    """Tests that we can parse a json to check if a change is committed."""
    project = self.createProject('test003')
    clone_path = self.cloneProject(project)
    gpatch = self.createPatch(clone_path, project)
    helper = self._GetHelper()
    helper.SetReview(gpatch.gerrit_number, labels={'Code-Review':'+2'})
    helper.SubmitChange(gpatch)
    self.assertTrue(helper.IsChangeCommitted(gpatch.gerrit_number))

    gpatch = self.createPatch(clone_path, project)
    self.assertFalse(helper.IsChangeCommitted(gpatch.gerrit_number))

  @cros_test_lib.NetworkTest()
  def test004GetLatestSHA1ForBranch(self):
    """Verifies that we can query the tip-of-tree commit in a git repository."""
    project = self.createProject('test004')
    clone_path = self.cloneProject(project)
    for _ in xrange(5):
      (master_sha1, _) = self.createCommit(clone_path)
    self.pushBranch(clone_path, 'master')
    for _ in xrange(5):
      (testbranch_sha1, _) = self.createCommit(clone_path)
    self.pushBranch(clone_path, 'testbranch')
    helper = self._GetHelper()
    self.assertEqual(
        helper.GetLatestSHA1ForBranch(project, 'master'),
        master_sha1)
    self.assertEqual(
        helper.GetLatestSHA1ForBranch(project, 'testbranch'),
        testbranch_sha1)

  def _ChooseReviewers(self):
    all_reviewers = set(['dborowitz@google.com', 'sop@google.com',
                         'jrn@google.com'])
    ret = list(all_reviewers.difference(['%s@google.com' % getpass.getuser()]))
    self.assertGreaterEqual(len(ret), 2)
    return ret

  @cros_test_lib.NetworkTest()
  def test005SetReviewers(self):
    """Verify that we can set reviewers on a CL."""
    project = self.createProject('test005')
    clone_path = self.cloneProject(project)
    gpatch = self.createPatch(clone_path, project)
    emails = self._ChooseReviewers()
    helper = self._GetHelper()
    helper.SetReviewers(gpatch.gerrit_number, add=(
        emails[0], emails[1]))
    reviewers = gob_util.GetReviewers(helper.host, gpatch.gerrit_number)
    self.assertEqual(len(reviewers), 2)
    self.assertItemsEqual(
        [r['email'] for r in reviewers],
        [emails[0], emails[1]])
    helper.SetReviewers(gpatch.gerrit_number,
                        remove=(emails[0],))
    reviewers = gob_util.GetReviewers(helper.host, gpatch.gerrit_number)
    self.assertEqual(len(reviewers), 1)
    self.assertEqual(reviewers[0]['email'], emails[1])

  @cros_test_lib.NetworkTest()
  def test006PatchNotFound(self):
    """Test case where ChangeID isn't found on the server."""
    changeids = ['I' + ('deadbeef' * 5), 'I' + ('beadface' * 5)]
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      changeids)
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      ['*' + cid for cid in changeids])
    # Change ID sequence starts at 1000.
    gerrit_numbers = ['123', '543']
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      gerrit_numbers)
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      ['*' + num for num in gerrit_numbers])

  @cros_test_lib.NetworkTest()
  def test007VagueQuery(self):
    """Verify GerritHelper complains if an ID matches multiple changes."""
    project = self.createProject('test007')
    clone_path = self.cloneProject(project)
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

  @cros_test_lib.NetworkTest()
  def test008Queries(self):
    """Verify assorted query operations."""
    project = self.createProject('test008')
    clone_path = self.cloneProject(project)
    gpatch = self.createPatch(clone_path, project)
    helper = self._GetHelper()

    # Multi-queries with one valid and one invalid term should raise.
    invalid_change_id = 'I1234567890123456789012345678901234567890'
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      [invalid_change_id, gpatch.change_id])
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      [gpatch.change_id, invalid_change_id])
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      ['12345', gpatch.gerrit_number])
    self.assertRaises(gerrit.GerritException, gerrit.GetGerritPatchInfo,
                      [gpatch.gerrit_number, '12345'])

    # Simple query by project/changeid/sha1.
    patch_info = helper.GrabPatchFromGerrit(gpatch.project, gpatch.change_id,
                                            gpatch.sha1)
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

  @cros_test_lib.NetworkTest()
  def test009SubmitOutdatedCommit(self):
    """Tests that we can parse a json to check if a change is committed."""
    project = self.createProject('test009')
    clone_path = self.cloneProject(project)

    # Create a change.
    gpatch1 = self.createPatch(clone_path, project)

    # Update the change.
    gpatch2 = self.createPatch(clone_path, project, amend=True)

    # Make sure we're talking about the same change.
    self.assertEqual(gpatch1.change_id, gpatch2.change_id)

    # Try submitting the out-of-date change.
    helper = self._GetHelper()
    helper.SetReview(gpatch1.gerrit_number, labels={'Code-Review':'+2'})
    with self.assertRaises(gob_util.GOBError) as ex:
      helper.SubmitChange(gpatch1)
    self.assertEqual(ex.exception.http_status, httplib.CONFLICT)
    self.assertFalse(helper.IsChangeCommitted(gpatch1.gerrit_number))

    # Try submitting the up-to-date change.
    helper.SubmitChange(gpatch2)
    self.assertTrue(helper.IsChangeCommitted(gpatch2.gerrit_number))


if __name__ == '__main__':
  cros_test_lib.main()
