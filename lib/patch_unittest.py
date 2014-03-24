#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chromite.lib.patch."""

import copy
import itertools
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import patch as cros_patch

import mock

_GetNumber = iter(itertools.count()).next

FAKE_PATCH_JSON = {
  "project":"tacos/chromite", "branch":"master",
  "id":"Iee5c89d929f1850d7d4e1a4ff5f21adda800025f",
  "currentPatchSet": {
    "number":"2", "ref":gerrit.GetChangeRef(1112, 2),
    "revision":"ff10979dd360e75ff21f5cf53b7f8647578785ef",
  },
  "number":"1112",
  "subject":"chromite commit",
  "owner":{"name":"Chromite Master", "email":"chromite@chromium.org"},
  "url":"https://chromium-review.googlesource.com/1112",
  "lastUpdated":1311024529,
  "sortKey":"00166e8700001052",
  "open": True,
  "status":"NEW",
}

# Change-ID of a known open change in public gerrit.
GERRIT_OPEN_CHANGEID = '8366'
GERRIT_MERGED_CHANGEID = '3'
GERRIT_ABANDONED_CHANGEID = '2'


class TestGitRepoPatch(cros_test_lib.TempDirTestCase):
  """Unittests for git patch related methods."""

  # No mock bits are to be used in this class's tests.
  # This needs to actually validate git output, and git behaviour, rather
  # than test our assumptions about git's behaviour/output.

  patch_kls = cros_patch.GitRepoPatch

  COMMIT_TEMPLATE = (
"""commit abcdefgh

Author: Fake person
Date:  Tue Oct 99

I am the first commit.

%(extra)s

%(change-id)s
"""
  )

  # Boolean controlling whether the target class natively knows its
  # ChangeId; only GerritPatches do.
  has_native_change_id = False

  DEFAULT_TRACKING = 'refs/remotes/%s/master' % constants.EXTERNAL_REMOTE

  def _CreateSourceRepo(self, path):
    """Generate a new repo with a single commit."""
    tmp_path = '%s-tmp' % path
    os.mkdir(path)
    os.mkdir(tmp_path)
    self._run(['git', 'init', '--separate-git-dir', path], cwd=tmp_path)

    # Add an initial commit then wipe the working tree.
    self._run(['git', 'commit', '--allow-empty', '-m', 'initial commit'],
              cwd=tmp_path)
    shutil.rmtree(tmp_path)

  def setUp(self):
    # Create an empty repo to work from.
    self.source = os.path.join(self.tempdir, 'source.git')
    self._CreateSourceRepo(self.source)
    self.default_cwd = os.path.join(self.tempdir, 'unwritable')
    self.original_cwd = os.getcwd()
    os.mkdir(self.default_cwd)
    os.chdir(self.default_cwd)
    # Disallow write so as to smoke out any invalid writes to
    # cwd.
    os.chmod(self.default_cwd, 0o500)

  def tearDown(self):
    if hasattr(self, 'original_cwd'):
      os.chdir(self.original_cwd)

  def _MkPatch(self, source, sha1, ref='refs/heads/master', **kwargs):
    return self.patch_kls(source, 'chromiumos/chromite', ref,
                          '%s/master' % constants.EXTERNAL_REMOTE,
                          kwargs.pop('remote', constants.EXTERNAL_REMOTE),
                          sha1=sha1, **kwargs)

  def _run(self, cmd, cwd=None):
    # Note that cwd is intentionally set to a location the user can't write
    # to; this flushes out any bad usage in the tests that would work by
    # fluke of being invoked from w/in a git repo.
    if cwd is None:
      cwd = self.default_cwd
    return cros_build_lib.RunCommand(
        cmd, cwd=cwd, print_cmd=False, capture_output=True).output.strip()

  def _GetSha1(self, cwd, refspec):
    return self._run(['git', 'rev-list', '-n1', refspec], cwd=cwd)

  def _MakeRepo(self, name, clone, remote=None, alternates=True):
    path = os.path.join(self.tempdir, name)
    cmd = ['git', 'clone', clone, path]
    if alternates:
      cmd += ['--reference', clone]
    if remote is None:
      remote = constants.EXTERNAL_REMOTE
    cmd += ['--origin', remote]
    self._run(cmd)
    return path

  def _MakeCommit(self, repo, commit=None):
    if commit is None:
      commit = "commit at %s" % (time.time(),)
    self._run(['git', 'commit', '-a', '-m', commit], repo)
    return self._GetSha1(repo, 'HEAD')

  def CommitFile(self, repo, filename, content, commit=None, **kwargs):
    osutils.WriteFile(os.path.join(repo, filename), content)
    self._run(['git', 'add', filename], repo)
    sha1 = self._MakeCommit(repo, commit=commit)
    if not self.has_native_change_id:
      kwargs.pop('ChangeId', None)
    patch = self._MkPatch(repo, sha1, **kwargs)
    self.assertEqual(patch.sha1, sha1)
    return patch

  def _CommonGitSetup(self):
    git1 = self._MakeRepo('git1', self.source)
    git2 = self._MakeRepo('git2', self.source)
    patch = self.CommitFile(git1, 'monkeys', 'foon')
    return git1, git2, patch

  def testGetDiffStatus(self):
    git1, _, patch1 = self._CommonGitSetup()
    # Ensure that it can work on the first commit, even if it
    # doesn't report anything (no delta; it's the first files).
    patch1 = self._MkPatch(git1, self._GetSha1(git1, self.DEFAULT_TRACKING))
    self.assertEqual({}, patch1.GetDiffStatus(git1))
    patch2 = self.CommitFile(git1, 'monkeys', 'blah')
    self.assertEqual({'monkeys': 'M'}, patch2.GetDiffStatus(git1))
    git.RunGit(git1, ['mv', 'monkeys', 'monkeys2'])
    patch3 = self._MkPatch(git1, self._MakeCommit(git1, commit="mv"))
    self.assertEqual({'monkeys': 'D', 'monkeys2': 'A'},
                     patch3.GetDiffStatus(git1))
    patch4 = self.CommitFile(git1, 'monkey2', 'blah')
    self.assertEqual({'monkey2': 'A'}, patch4.GetDiffStatus(git1))

  def testFetch(self):
    _, git2, patch = self._CommonGitSetup()
    patch.Fetch(git2)
    self.assertEqual(patch.sha1, self._GetSha1(git2, 'FETCH_HEAD'))
    # Verify reuse; specifically that Fetch doesn't actually run since
    # the rev is already available locally via alternates.
    patch.project_url = '/dev/null'
    git3 = self._MakeRepo('git3', git2)
    patch.Fetch(git3)
    self.assertEqual(patch.sha1, self._GetSha1(git3, patch.sha1))

  def testFetchFirstPatchInSeries(self):
    git1, git2, patch = self._CommonGitSetup()
    self.CommitFile(git1, 'monkeys', 'foon2')
    patch.Fetch(git2)

  def testFetchWithoutSha1(self):
    git1, git2, _ = self._CommonGitSetup()
    patch2 = self.CommitFile(git1, 'monkeys', 'foon2')
    sha1, patch2.sha1 = patch2.sha1, None
    patch2.Fetch(git2)
    self.assertEqual(sha1, patch2.sha1)

  def testAlreadyApplied(self):
    git1 = self._MakeRepo('git1', self.source)
    patch1 = self._MkPatch(git1, self._GetSha1(git1, 'HEAD'))
    self.assertRaises2(cros_patch.PatchAlreadyApplied, patch1.Apply, git1,
                       self.DEFAULT_TRACKING, check_attrs={'inflight':False})
    patch2 = self.CommitFile(git1, 'monkeys', 'rule')
    self.assertRaises2(cros_patch.PatchAlreadyApplied, patch2.Apply, git1,
                       self.DEFAULT_TRACKING, check_attrs={'inflight':True})

  def testDeleteEbuildTwice(self):
    """Test that double-deletes of ebuilds are flagged as conflicts."""
    # Create monkeys.ebuild for testing.
    git1 = self._MakeRepo('git1', self.source)
    patch1 = self.CommitFile(git1, 'monkeys.ebuild', 'rule')
    git.RunGit(git1, ['rm', 'monkeys.ebuild'])
    patch2 = self._MkPatch(git1, self._MakeCommit(git1, commit='rm'))

    # Delete an ebuild that does not exist in TOT.
    check_attrs = {'inflight': False, 'files': ('monkeys.ebuild',)}
    self.assertRaises2(cros_patch.EbuildConflict, patch2.Apply, git1,
                       self.DEFAULT_TRACKING, check_attrs=check_attrs)

    # Delete an ebuild that exists in TOT, but does not exist in the current
    # patch series.
    check_attrs['inflight'] = True
    self.assertRaises2(cros_patch.EbuildConflict, patch2.Apply, git1,
                       patch1.sha1, check_attrs=check_attrs)

  def testCleanlyApply(self):
    _, git2, patch = self._CommonGitSetup()
    # Clone git3 before we modify git2; else we'll just wind up
    # cloning it's master.
    git3 = self._MakeRepo('git3', git2)
    patch.Apply(git2, self.DEFAULT_TRACKING)
    self.assertEqual(patch.sha1, self._GetSha1(git2, 'HEAD'))
    # Verify reuse; specifically that Fetch doesn't actually run since
    # the object is available in alternates.  testFetch partially
    # validates this; the Apply usage here fully validates it via
    # ensuring that the attempted Apply goes boom if it can't get the
    # required sha1.
    patch.project_url = '/dev/null'
    patch.Apply(git3, self.DEFAULT_TRACKING)
    self.assertEqual(patch.sha1, self._GetSha1(git3, 'HEAD'))

  def testFailsApply(self):
    _, git2, patch1 = self._CommonGitSetup()
    patch2 = self.CommitFile(git2, 'monkeys', 'not foon')
    # Note that Apply creates it's own branch, resetting to master
    # thus we have to re-apply (even if it looks stupid, it's right).
    patch2.Apply(git2, self.DEFAULT_TRACKING)
    self.assertRaises2(cros_patch.ApplyPatchException,
                       patch1.Apply, git2, self.DEFAULT_TRACKING,
                       exact_kls=True, check_attrs={'inflight':True})

  def testTrivial(self):
    _, git2, patch1 = self._CommonGitSetup()
    # Throw in a bunch of newlines so that content-merging would work.
    content = 'not foon%s' % ('\n' * 100)
    patch1 = self._MkPatch(git2, self._GetSha1(git2, 'HEAD'))
    patch1 = self.CommitFile(git2, 'monkeys', content)
    git.RunGit(
        git2, ['update-ref', self.DEFAULT_TRACKING, patch1.sha1])
    patch2 = self.CommitFile(git2, 'monkeys', '%sblah' % content)
    patch3 = self.CommitFile(git2, 'monkeys', '%sblahblah' % content)
    # Get us a back to the basic, then derive from there; this is used to
    # verify that even if content merging works, trivial is flagged.
    self.CommitFile(git2, 'monkeys', 'foon')
    patch4 = self.CommitFile(git2, 'monkeys', content)
    patch5 = self.CommitFile(git2, 'monkeys', '%sfoon' % content)
    # Reset so we derive the next changes from patch1.
    git.RunGit(git2, ['reset', '--hard', patch1.sha1])
    patch6 = self.CommitFile(git2, 'blah', 'some-other-file')
    self.CommitFile(git2, 'monkeys',
                    '%sblah' % content.replace('not', 'bot'))

    self.assertRaises2(cros_patch.PatchAlreadyApplied,
                       patch1.Apply, git2, self.DEFAULT_TRACKING, trivial=True,
                       check_attrs={'inflight':False, 'trivial':False})

    # Now test conflicts since we're still at ToT; note that this is an actual
    # conflict because the fuzz anchors have changed.
    self.assertRaises2(cros_patch.ApplyPatchException,
                       patch3.Apply, git2, self.DEFAULT_TRACKING, trivial=True,
                       check_attrs={'inflight':False, 'trivial':False},
                       exact_kls=True)

    # Now test trivial conflict; this would've merged fine were it not for
    # trivial.
    self.assertRaises2(cros_patch.PatchAlreadyApplied,
                       patch4.Apply, git2, self.DEFAULT_TRACKING, trivial=True,
                       check_attrs={'inflight':False, 'trivial':False},
                       exact_kls=True)

    # Move us into inflight testing.
    patch2.Apply(git2, self.DEFAULT_TRACKING, trivial=True)

    # Repeat the tests from above; should still be the same.
    self.assertRaises2(cros_patch.PatchAlreadyApplied,
                       patch4.Apply, git2, self.DEFAULT_TRACKING, trivial=True,
                       check_attrs={'inflight':False, 'trivial':False})

    # Actual conflict merge conflict due to inflight; non trivial induced.
    self.assertRaises2(cros_patch.ApplyPatchException,
                       patch5.Apply, git2, self.DEFAULT_TRACKING, trivial=True,
                       check_attrs={'inflight':True, 'trivial':False},
                       exact_kls=True)

    self.assertRaises2(cros_patch.PatchAlreadyApplied,
                       patch1.Apply, git2, self.DEFAULT_TRACKING, trivial=True,
                       check_attrs={'inflight':False})

    self.assertRaises2(cros_patch.ApplyPatchException,
                       patch5.Apply, git2, self.DEFAULT_TRACKING, trivial=True,
                       check_attrs={'inflight':True, 'trivial':False},
                       exact_kls=True)

    # And this should apply without issue, despite the differing history.
    patch6.Apply(git2, self.DEFAULT_TRACKING, trivial=True)

  def _assertLookupAliases(self, remote):
    git1 = self._MakeRepo('git1', self.source)
    patch = self.CommitChangeIdFile(git1, remote=remote)
    prefix = '*' if patch.internal else ''
    vals = [patch.sha1, getattr(patch, 'gerrit_number', None),
            getattr(patch, 'original_sha1', None)]
    # Append full Change-ID if it exists.
    if patch.project and patch.tracking_branch and patch.change_id:
      vals.append('%s~%s~%s' % (
          patch.project, patch.tracking_branch, patch.change_id))
    vals = [x for x in vals if x is not None]
    self.assertEqual(set(prefix + x for x in vals), set(patch.LookupAliases()))

  def testExternalLookupAliases(self):
    self._assertLookupAliases(constants.EXTERNAL_REMOTE)

  def testInternalLookupAliases(self):
    self._assertLookupAliases(constants.INTERNAL_REMOTE)

  def MakeChangeId(self, how_many=1):
    l = [cros_patch.MakeChangeId() for _ in xrange(how_many)]
    if how_many == 1:
      return l[0]
    return l

  def CommitChangeIdFile(self, repo, changeid=None, extra=None,
                         filename='monkeys', content='flinging',
                         raw_changeid_text=None, **kwargs):
    template = self.COMMIT_TEMPLATE
    if changeid is None:
      changeid = self.MakeChangeId()
    if raw_changeid_text is None:
      raw_changeid_text = 'Change-Id: %s' % (changeid,)
    if extra is None:
      extra = ''
    commit = template % {'change-id': raw_changeid_text, 'extra':extra}

    return self.CommitFile(repo, filename, content, commit=commit,
                           ChangeId=changeid, **kwargs)

  def _CheckPaladin(self, repo, master_id, ids, extra):
    patch = self.CommitChangeIdFile(
        repo, master_id, extra=extra,
        filename='paladincheck', content=str(_GetNumber()))
    deps = patch.PaladinDependencies(repo)
    # Assert that our parsing unique'ifies the results.
    self.assertEqual(len(deps), len(set(deps)))
    # Verify that we have the correct dependencies.
    dep_ids = []
    dep_ids += [(dep.remote, dep.change_id) for dep in deps
                if dep.change_id is not None]
    dep_ids += [(dep.remote, dep.gerrit_number) for dep in deps
                if dep.gerrit_number is not None]
    dep_ids += [(dep.remote, dep.sha1) for dep in deps
                if dep.sha1 is not None]
    for input_id in ids:
      change_tuple = cros_patch.StripPrefix(input_id)
      self.assertTrue(change_tuple in dep_ids)

    return patch

  def testPaladinDependencies(self):
    git1 = self._MakeRepo('git1', self.source)
    cid1, cid2, cid3, cid4 = self.MakeChangeId(4)
    # Verify it handles nonexistant CQ-DEPEND.
    self._CheckPaladin(git1, cid1, [], '')
    # Single key, single value.
    self._CheckPaladin(git1, cid1, [cid2],
                       'CQ-DEPEND=%s' % cid2)
    # Single key, gerrit number.
    self._CheckPaladin(git1, cid1, ['123'],
                       'CQ-DEPEND=%s' % 123)
    # Single key, gerrit number.
    self._CheckPaladin(git1, cid1, ['123456'],
                       'CQ-DEPEND=%s' % 123456)
    # Single key, gerrit number; ensure it
    # cuts off before a million changes (this
    # is done to avoid collisions w/ sha1 when
    # we're using shortened versions).
    self.assertRaises(cros_patch.BrokenCQDepends,
                      self._CheckPaladin, git1, cid1,
                      ['1234567'], 'CQ-DEPEND=%s' % '1234567')
    # Single key, gerrit number, internal.
    self._CheckPaladin(git1, cid1, ['*123'],
                       'CQ-DEPEND=%s' % '*123')
    # Ensure SHA1's aren't allowed.
    sha1 = '0' * 40
    self.assertRaises(cros_patch.BrokenCQDepends,
                      self._CheckPaladin, git1, cid1,
                      [sha1], 'CQ-DEPEND=%s' % sha1)

    # Single key, multiple values
    self._CheckPaladin(git1, cid1, [cid2, '1223'],
                       'CQ-DEPEND=%s %s' % (cid2, '1223'))
    # Dumb comma behaviour
    self._CheckPaladin(git1, cid1, [cid2, cid3],
                      'CQ-DEPEND=%s, %s,' % (cid2, cid3))
    # Multiple keys.
    self._CheckPaladin(git1, cid1, [cid2, '*245', cid4],
                      'CQ-DEPEND=%s, %s\nCQ-DEPEND=%s' % (cid2, '*245', cid4))

    # Ensure it goes boom on invalid data.
    self.assertRaises(cros_patch.BrokenCQDepends, self._CheckPaladin,
                      git1, cid1, [], 'CQ-DEPEND=monkeys')
    self.assertRaises(cros_patch.BrokenCQDepends, self._CheckPaladin,
                      git1, cid1, [], 'CQ-DEPEND=%s monkeys' % (cid2,))
    # Validate numeric is allowed.
    self._CheckPaladin(git1, cid1, [cid2, '1'], 'CQ-DEPEND=1 %s' % cid2)
    # Validate that it unique'ifies the results.
    self._CheckPaladin(git1, cid1, ['1'], 'CQ-DEPEND=1 1')

    # Invalid syntax
    self.assertRaises(cros_patch.BrokenCQDepends, self._CheckPaladin,
                      git1, cid1, [], 'CQ-DEPENDS=1')
    self.assertRaises(cros_patch.BrokenCQDepends, self._CheckPaladin,
                      git1, cid1, [], 'CQ_DEPEND=1')


class TestLocalPatchGit(TestGitRepoPatch):
  """Test Local patch handling."""

  patch_kls = cros_patch.LocalPatch

  def setUp(self):
    self.sourceroot = os.path.join(self.tempdir, 'sourceroot')


  def _MkPatch(self, source, sha1, ref='refs/heads/master', **kwargs):
    remote = kwargs.pop('remote', constants.EXTERNAL_REMOTE)
    return self.patch_kls(source, 'chromiumos/chromite', ref,
                          '%s/master' % remote, remote, sha1, **kwargs)

  def testUpload(self):
    def ProjectDirMock(_sourceroot):
      return git1

    git1, git2, patch = self._CommonGitSetup()

    git2_sha1 = self._GetSha1(git2, 'HEAD')

    patch.ProjectDir = ProjectDirMock
    # First suppress carbon copy behaviour so we verify pushing
    # plain works.
    # pylint: disable=E1101
    sha1 = patch.sha1
    patch._GetCarbonCopy = lambda: sha1
    patch.Upload(git2, 'refs/testing/test1')
    self.assertEqual(self._GetSha1(git2, 'refs/testing/test1'),
                     patch.sha1)

    # Enable CarbonCopy behaviour; verify it lands a different
    # sha1.  Additionally verify it didn't corrupt the patch's sha1 locally.
    del patch._GetCarbonCopy
    patch.Upload(git2, 'refs/testing/test2')
    self.assertNotEqual(self._GetSha1(git2, 'refs/testing/test2'),
                        patch.sha1)
    self.assertEqual(patch.sha1, sha1)
    # Ensure the carbon creation didn't damage the target repo.
    self.assertEqual(self._GetSha1(git1, 'HEAD'), sha1)

    # Ensure we didn't damage the target repo's state at all.
    self.assertEqual(git2_sha1, self._GetSha1(git2, 'HEAD'))
    # Ensure the content is the same.
    base = ['git', 'show']
    self.assertEqual(
        self._run(base + ['refs/testing/test1:monkeys'], git2),
        self._run(base + ['refs/testing/test2:monkeys'], git2))
    base = ['git', 'log', '--format=%B', '-n1']
    self.assertEqual(
        self._run(base + ['refs/testing/test1'], git2),
        self._run(base + ['refs/testing/test2'], git2))


class TestUploadedLocalPatch(TestGitRepoPatch):
  """Test uploading of local git patches."""

  PROJECT = 'chromiumos/chromite'
  ORIGINAL_BRANCH = 'original_branch'
  ORIGINAL_SHA1 = 'ffffffff'.ljust(40, '0')

  patch_kls = cros_patch.UploadedLocalPatch

  def _MkPatch(self, source, sha1, ref='refs/heads/master', **kwargs):
    return self.patch_kls(source, self.PROJECT, ref,
                          '%s/master' % constants.EXTERNAL_REMOTE,
                          self.ORIGINAL_BRANCH,
                          self.ORIGINAL_SHA1,
                          kwargs.pop('remote', constants.EXTERNAL_REMOTE),
                          carbon_copy_sha1=sha1, **kwargs)

  def testStringRepresentation(self):
    _, _, patch = self._CommonGitSetup()
    str_rep = str(patch).split(':')
    for element in [self.PROJECT, self.ORIGINAL_BRANCH, self.ORIGINAL_SHA1[:8]]:
      self.assertTrue(element in str_rep,
                      msg="Couldn't find %s in %s" % (element, str_rep))


class TestGerritPatch(TestGitRepoPatch):
  """Test Gerrit patch handling."""

  has_native_change_id = True

  class patch_kls(cros_patch.GerritPatch):
    """Test helper class to suppress pointing to actual gerrit."""
    # Suppress the behaviour pointing the project url at actual gerrit,
    # instead slaving it back to a local repo for tests.
    def __init__(self, *args, **kwargs):
      cros_patch.GerritPatch.__init__(self, *args, **kwargs)
      assert hasattr(self, 'patch_dict')
      self.project_url = self.patch_dict['_unittest_url_bypass']

  @property
  def test_json(self):
    return copy.deepcopy(FAKE_PATCH_JSON)

  def _MkPatch(self, source, sha1, ref='refs/heads/master', **kwargs):
    json = self.test_json
    remote = kwargs.pop('remote', constants.EXTERNAL_REMOTE)
    url_prefix = kwargs.pop('url_prefix', constants.EXTERNAL_GERRIT_URL)
    suppress_branch = kwargs.pop('suppress_branch', False)
    change_id = kwargs.pop('ChangeId', None)
    if change_id is None:
      change_id = self.MakeChangeId()
    json.update(kwargs)
    change_num, patch_num = _GetNumber(), _GetNumber()
    # Note we intentionally use a gerrit like refspec here; we want to
    # ensure that none of our common code pathways puke on a non head/tag.
    refspec = gerrit.GetChangeRef(change_num + 1000, patch_num)
    json['currentPatchSet'].update(
        dict(number=patch_num, ref=refspec, revision=sha1))
    json['branch'] = os.path.basename(ref)
    json['_unittest_url_bypass'] = source
    json['id'] = change_id

    obj = self.patch_kls(json.copy(), remote, url_prefix)
    self.assertEqual(obj.patch_dict, json)
    self.assertEqual(obj.remote, remote)
    self.assertEqual(obj.url_prefix, url_prefix)
    self.assertEqual(obj.project, json['project'])
    self.assertEqual(obj.ref, refspec)
    self.assertEqual(obj.change_id, change_id)
    self.assertEqual(obj.id, '%s%s~%s~%s' % (
        constants.CHANGE_PREFIX[remote], json['project'],
        json['branch'], change_id))

    # Now make the fetching actually work, if desired.
    if not suppress_branch:
      # Note that a push is needed here, rather than a branch; branch
      # will just make it under refs/heads, we want it literally in
      # refs/changes/
      self._run(['git', 'push', source, '%s:%s' % (sha1, refspec)], source)
    return obj

  def testApprovalTimestamp(self):
    """Test that the approval timestamp is correctly extracted from JSON."""
    repo = self._MakeRepo('git', self.source)
    for approvals, expected in [(None, 0), ([], 0), ([1], 1), ([1, 3, 2], 3)]:
      currentPatchSet = copy.deepcopy(FAKE_PATCH_JSON['currentPatchSet'])
      if approvals is not None:
        currentPatchSet['approvals'] = [{'grantedOn': x} for x in approvals]
      patch = self._MkPatch(repo, self._GetSha1(repo, self.DEFAULT_TRACKING),
                            currentPatchSet=currentPatchSet)
      msg = 'Expected %r, but got %r (approvals=%r)' % (
          expected, patch.approval_timestamp, approvals)
      self.assertEqual(patch.approval_timestamp, expected, msg)

  def _assertGerritDependencies(self, remote=constants.EXTERNAL_REMOTE):
    convert = str
    if remote == constants.INTERNAL_REMOTE:
      convert = lambda val: '*%s' % (val,)
    git1 = self._MakeRepo('git1', self.source, remote=remote)
    patch = self._MkPatch(git1, self._GetSha1(git1, 'HEAD'), remote=remote)
    cid1, cid2 = '1', '2'

    # Test cases with no dependencies, 1 dependency, and 2 dependencies.
    self.assertEqual(patch.GerritDependencies(), [])
    patch.patch_dict['dependsOn'] = [{'number': cid1}]
    self.assertEqual(
        [cros_patch.AddPrefix(x, x.gerrit_number)
         for x in patch.GerritDependencies()],
        [convert(cid1)])
    patch.patch_dict['dependsOn'].append({'number': cid2})
    self.assertEqual(
        [cros_patch.AddPrefix(x, x.gerrit_number)
         for x in patch.GerritDependencies()],
        [convert(cid1), convert(cid2)])

  def testExternalGerritDependencies(self):
    self._assertGerritDependencies()

  def testInternalGerritDependencies(self):
    self._assertGerritDependencies(constants.INTERNAL_REMOTE)


class PrepareRemotePatchesTest(cros_test_lib.TestCase):
  """Test preparing remote patches."""

  def MkRemote(self,
               project='my/project', original_branch='my-local',
               ref='refs/tryjobs/elmer/patches', tracking_branch='master',
               internal=False):

    l = [project, original_branch, ref, tracking_branch,
         getattr(constants, '%s_PATCH_TAG' % (
            'INTERNAL' if internal else 'EXTERNAL'))]
    return ':'.join(l)

  def assertRemote(self, patch, project='my/project',
                   original_branch='my-local',
                   ref='refs/tryjobs/elmer/patches', tracking_branch='master',
                   internal=False):
    self.assertEqual(patch.project, project)
    self.assertEqual(patch.original_branch, original_branch)
    self.assertEqual(patch.ref, ref)
    self.assertEqual(patch.tracking_branch, tracking_branch)
    self.assertEqual(patch.internal, internal)

  def test(self):
    # Check handling of a single patch...
    patches = cros_patch.PrepareRemotePatches([self.MkRemote()])
    self.assertEqual(len(patches), 1)
    self.assertRemote(patches[0])

    # Check handling of a multiple...
    patches = cros_patch.PrepareRemotePatches(
        [self.MkRemote(), self.MkRemote(project='foon')])
    self.assertEqual(len(patches), 2)
    self.assertRemote(patches[0])
    self.assertRemote(patches[1], project='foon')

    # Ensure basic validation occurs:
    chunks = self.MkRemote().split(':')
    self.assertRaises(ValueError, cros_patch.PrepareRemotePatches,
                      ':'.join(chunks[:-1]))
    self.assertRaises(ValueError, cros_patch.PrepareRemotePatches,
                      ':'.join(chunks[:-1] + ['monkeys']))
    self.assertRaises(ValueError, cros_patch.PrepareRemotePatches,
                      ':'.join(chunks + [':']))


class PrepareLocalPatchesTests(cros_build_lib_unittest.RunCommandTestCase):
  """Test preparing local patches."""

  def setUp(self):
    self.path, self.project, self.branch = 'mydir', 'my/project', 'mybranch'
    self.tracking_branch = 'kernel'
    self.patches = ['%s:%s' % (self.project, self.branch)]
    self.manifest = mock.MagicMock()
    attrs = dict(tracking_branch=self.tracking_branch,
                 local_path=self.path,
                 remote='cros')
    checkout = git.ProjectCheckout(attrs)
    self.PatchObject(
        self.manifest, 'FindCheckouts', return_value=[checkout]
    )

  def PrepareLocalPatches(self, output):
    """Check the returned GitRepoPatchInfo against golden values."""
    output_obj = mock.MagicMock()
    output_obj.output = output
    self.PatchObject(cros_patch.LocalPatch, 'Fetch', return_value=output_obj)
    self.PatchObject(git, 'RunGit', return_value=output_obj)
    patch_info, = cros_patch.PrepareLocalPatches(self.manifest, self.patches)
    self.assertEquals(patch_info.project, self.project)
    self.assertEquals(patch_info.ref, self.branch)
    self.assertEquals(patch_info.tracking_branch, self.tracking_branch)

  def testBranchSpecifiedSuccessRun(self):
    """Test success with branch specified by user."""
    self.PrepareLocalPatches('12345'.rjust(40, '0'))

  def testBranchSpecifiedNoChanges(self):
    """Test when no changes on the branch specified by user."""
    self.assertRaises(SystemExit, self.PrepareLocalPatches, '')


class TestFormatting(cros_test_lib.TestCase):
  """Test formatting of output."""

  def _assertResult(self, functor, value, expected=None, raises=False,
                    **kwargs):
    if raises:
      self.assertRaises2(ValueError, functor, value,
                         msg="%s(%r) did not throw a ValueError"
                         % (functor.__name__, value),  **kwargs)
    else:
      self.assertEqual(functor(value, **kwargs), expected,
                       msg="failed: %s(%r) != %r"
                       % (functor.__name__, value, expected))

  def _assertBad(self, functor, values, **kwargs):
    for value in values:
      self._assertResult(functor, value, raises=True, **kwargs)

  def _assertGood(self, functor, values, **kwargs):
    for value, expected in values:
      self._assertResult(functor, value, expected, **kwargs)


  def TestGerritNumber(self):
    """Tests that we can pasre a Gerrit number."""
    self._assertGood(cros_patch.ParseGerritNumber,
        [('12345',) * 2, ('12',) * 2, ('123',) * 2])

    self._assertBad(
        cros_patch.ParseGerritNumber,
        ['is', 'i1325', '01234567', '012345a', '**12345', '+123', '/0123'],
        error_ok=False)

  def TestChangeID(self):
    """Tests that we can parse a change-ID."""
    self._assertGood(cros_patch.ParseChangeID,
        [('I47ea30385af60ae4cc2acc5d1a283a46423bc6e1',) * 2])

    # Change-IDs too short/long, with unexpected characters in it.
    self._assertBad(
        cros_patch.ParseChangeID,
        ['is', '**i1325', 'i134'.ljust(41, '0'), 'I1234+'.ljust(41, '0'),
         'I123'.ljust(42, '0')],
        error_ok=False)

  def TestSHA1(self):
    """Tests that we can parse a SHA1 hash."""
    self._assertGood(cros_patch.ParseSHA1,
                     [('1' * 40,) * 2,
                      ('a' * 40,) * 2,
                      ('1a7e034'.ljust(40, '0'),) *2])

    self._assertBad(
        cros_patch.ParseSHA1,
        ['0abcg', 'Z', '**a', '+123', '1234ab' * 10],
        error_ok=False)

  def TestFullChangeID(self):
    """Tests that we can parse a full change-ID."""
    change_id = 'I47ea30385af60ae4cc2acc5d1a283a46423bc6e1'
    self._assertGood(cros_patch.ParseFullChangeID,
        [('foo~bar~%s' % change_id, ('foo', 'bar', change_id)),
         ('foo/bar/baz~refs/heads/_my-branch_~%s' % change_id,
          ('foo/bar/baz', '_my-branch_', change_id))])

    self._assertBad(
        cros_patch.ParseFullChangeID,
        ['foo', 'foo~bar', 'foo~bar~baz', 'foo~refs/bar~%s' % change_id],
        error_ok=False)

  def testParsePatchDeps(self):
    """Tests that we can parse the dependency specified by the user."""
    change_id = 'I47ea30385af60ae4cc2acc5d1a283a46423bc6e1'
    vals = ['CL:12345', 'project~branch~%s' % change_id, change_id,
            change_id[1:]]
    for val in vals:
      self.assertTrue(cros_patch.ParsePatchDep(val) is not None)

    self._assertBad(cros_patch.ParsePatchDep,
                    ['1454623', 'I47ea3', 'i47ea3'.ljust(41, '0')])


if __name__ == '__main__':
  cros_test_lib.main()
