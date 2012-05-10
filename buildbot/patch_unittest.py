#!/usr/bin/python

# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands.  Needs to be run inside of chroot for mox."""

import itertools
import logging
import mox
import os
import sys
import copy
import shutil
import tempfile
import time
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import gerrit_helper

_GetNumber = iter(itertools.count()).next

FAKE_PATCH_JSON = {
  "project":"tacos/chromite", "branch":"master",
  "id":"Iee5c89d929f1850d7d4e1a4ff5f21adda800025f",
  "currentPatchSet": {
    "number":"2", "ref":"refs/changes/72/5172/1",
    "revision":"ff10979dd360e75ff21f5cf53b7f8647578785ef",
  },
  "number":"1112",
  "subject":"chromite commit",
  "owner":{"name":"Chromite Master", "email":"chromite@chromium.org"},
  "url":"http://gerrit.chromium.org/gerrit/1112",
  "lastUpdated":1311024529,
  "sortKey":"00166e8700001052",
  "open": True,
  "status":"NEW",
}

# Change-ID of a known open change in public gerrit.
GERRIT_OPEN_CHANGEID = '8366'
GERRIT_MERGED_CHANGEID = '3'
GERRIT_ABANDONED_CHANGEID = '1'


class _PatchSuppression(object):

  """Mixin to suppress certain behaviour in patch objects for testing."""

  def ProjectDir(self, buildroot):
    # Short circuit this to not require manifest parsing.
    return buildroot

  def _GetUpstreamBranch(self, buildroot):
    # Tests are built around the assumption of this upstream value.
    return 'origin/master'


class TestGitRepoPatch(cros_test_lib.TempDirMixin, unittest.TestCase):

  # No pymox bits are to be used in this class's tests.
  # This needs to actually validate git output, and git behaviour, rather
  # than test our assumptions about git's behaviour/output.

  class patch_kls(_PatchSuppression, cros_patch.GitRepoPatch):
    pass

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
    cros_test_lib.TempDirMixin.setUp(self)
    # Create an empty repo to work from.
    self.source = os.path.join(self.tempdir, 'source.git')
    self._CreateSourceRepo(self.source)
    self.default_cwd = os.path.join(self.tempdir, 'unwritable')
    self.original_cwd = os.getcwd()
    os.mkdir(self.default_cwd)
    os.chdir(self.default_cwd)
    # Disallow write so as to smoke out any invalid writes to
    # cwd.
    os.chmod(self.default_cwd, 0500)

  def tearDown(self):
    os.chdir(self.original_cwd)
    # shutil.rmtree won't reset perms on an unwritable directory; do it
    # ourselves.
    os.chmod(self.default_cwd, 0700)
    cros_test_lib.TempDirMixin.tearDown(self)

  def _MkPatch(self, source, sha1, ref='refs/heads/master', **kwds):
    return self.patch_kls(source, 'chromiumos/chromite', ref,
                    'origin/master', sha1=sha1, **kwds)

  def _run(self, cmd, cwd=None):
    # Note that cwd is intentionally set to a location the user can't write
    # to; this flushes out any bad usage in the tests that would work by
    # fluke of being invoked from w/in a git repo.
    if cwd is None:
      cwd = self.default_cwd
    return cros_lib.RunCommandCaptureOutput(
        cmd, cwd=cwd, print_cmd=False).output.strip()

  def _GetSha1(self, cwd, refspec):
    return self._run(['git', 'rev-list', '-n1', refspec], cwd=cwd)

  def _MakeRepo(self, name, clone, branch='master', alternates=True):
    path = os.path.join(self.tempdir, name)
    cmd = ['git', 'clone', clone, path]
    if alternates:
      cmd += ['--reference', clone]
    self._run(cmd)
    return path

  def _MakeCommit(self, repo, commit=None):
    if commit is None:
      commit = "commit at %s" % (time.time(),)
    self._run(['git', 'commit', '-a', '-m', commit], repo)
    return self._GetSha1(repo, 'HEAD')

  def CommitFile(self, repo, filename, content, commit=None, **kwds):
    osutils.WriteFile(os.path.join(repo, filename), content)
    self._run(['git', 'add', filename], repo)
    sha1 = self._MakeCommit(repo, commit=commit)
    if not self.has_native_change_id:
      kwds.pop('ChangeId', None)
    patch = self._MkPatch(repo, sha1, **kwds)
    self.assertEqual(patch.sha1, sha1)
    return patch

  def _CommonGitSetup(self):
    git1 = self._MakeRepo('git1', self.source)
    git2 = self._MakeRepo('git2', self.source)
    patch = self.CommitFile(git1, 'monkeys', 'foon')
    return git1, git2, patch

  def testFetch(self):
    git1, git2, patch = self._CommonGitSetup()
    patch.Fetch(git2)
    self.assertEqual(patch.sha1, self._GetSha1(git2, 'FETCH_HEAD'))
    # Verify reuse; specifically that Fetch doesn't actually run since
    # the rev is already available locally via alternates.
    patch.project_url = '/dev/null'
    git3 = self._MakeRepo('git3', git2)
    patch.Fetch(git3)
    self.assertEqual(patch.sha1, self._GetSha1(git3, patch.sha1))

  def testAlreadyApplied(self):
    git1 = self._MakeRepo('git1', self.source)
    patch = self.CommitFile(git1, 'monkeys', 'rule')
    # Note that apply switches to a separate branch; thus the
    # double apply.  The first lands the change, the second
    # verifies the machinery doesn't scream when we try
    # landing it a second time.
    patch.Apply(git1)
    patch.Apply(git1)

  def testCleanlyApply(self):
    git1, git2, patch = self._CommonGitSetup()
    # Clone git3 before we modify git2; else we'll just wind up
    # cloning it's master.
    git3 = self._MakeRepo('git3', git2)
    patch.Apply(git2)
    self.assertEqual(patch.sha1, self._GetSha1(git2, 'HEAD'))
    # Verify reuse; specifically that Fetch doesn't actually run since
    # the object is available in alternates.  testFetch partially
    # validates this; the Apply usage here fully validates it via
    # ensuring that the attempted Apply goes boom if it can't get the
    # required sha1.
    patch.project_url='/dev/null'
    patch.Apply(git3)
    self.assertEqual(patch.sha1, self._GetSha1(git3, 'HEAD'))

  def testFailsApply(self):
    git1, git2, patch1 = self._CommonGitSetup()
    patch2 = self.CommitFile(git2, 'monkeys', 'not foon')
    # Note that Apply creates it's own branch, resetting to master
    # thus we have to re-apply (even if it looks stupid, it's right).
    patch2.Apply(git2)
    try:
      patch1.Apply(git2)
    except cros_patch.ApplyPatchException, e:
      self.assertTrue(e.inflight)
    else:
      raise AssertionError("patch1.Apply didn't throw a failing "
                           "exception.")

  def MakeChangeId(self, how_many=1):
    l = [cros_patch.MakeChangeId() for _ in xrange(how_many)]
    if how_many == 1:
      return l[0]
    return l

  def CommitChangeIdFile(self, repo, changeid=None, extra=None,
                         filename='monkeys', content='flinging',
                         raw_changeid_text=None):
    template = self.COMMIT_TEMPLATE
    if changeid is None:
      changeid = self.MakeChangeId()
    if raw_changeid_text is None:
      raw_changeid_text = 'Change-Id: %s' % (changeid,)
    if extra is None:
      extra = ''
    commit = template % {'change-id':raw_changeid_text, 'extra':extra}

    return self.CommitFile(repo, filename, content, commit=commit,
                           ChangeId=changeid)

  def testGerritDependencies(self):
    git1 = self._MakeRepo('git1', self.source)
    cid1, cid2, cid3 = self.MakeChangeId(3)
    patch = self.CommitChangeIdFile(git1, cid1)
    # Since it's parent is ToT, there are no deps.
    self.assertEqual(patch.GerritDependencies(git1), [])
    patch = self.CommitChangeIdFile(git1, cid2, content='poo')
    self.assertEqual(patch.GerritDependencies(git1), [cid1])

    # Check the behaviour for missing ChangeId in a parent next.
    patch = self.CommitChangeIdFile(git1, cid1, content='thus',
                                    raw_changeid_text='')

    # Note the ordering; leftmost needs to be the nearest child of the commit.
    self.assertEqual(patch.GerritDependencies(git1), [cid2, cid1])

    patch = self.CommitChangeIdFile(git1, cid3, content='the glass walls.')
    self.assertRaises(cros_patch.MissingChangeIDException,
                      patch.GerritDependencies, git1)

  def _CheckPaladin(self, repo, master_id, ids, extra):
    patch = self.CommitChangeIdFile(
        repo, master_id, extra=extra,
        filename='paladincheck', content=str(_GetNumber()))
    deps = patch.PaladinDependencies(repo)
    # Assert that are parsing unique'ifies the results.
    self.assertEqual(len(deps), len(set(deps)))
    deps = set(deps)
    ids = set(ids)
    self.assertEqual(ids, deps)
    self.assertEqual(
        set(cros_patch.FormatChangeId(x) for x in deps if x[0] in 'Ii'),
        set(x for x in ids if x[0] in 'Ii'))
    return patch

  def testPaladinDependencies(self):
    git1 = self._MakeRepo('git1', self.source)
    cid1, cid2, cid3, cid4 = self.MakeChangeId(4)
    # Verify it handles nonexistant CQ-DEPEND.
    self._CheckPaladin(git1, cid1, [], '')
    # Single key, single value.
    self._CheckPaladin(git1, cid1, [cid2],
                       'CQ-DEPEND=%s' % cid2)
    # Single key, multiple values
    self._CheckPaladin(git1, cid1, [cid2, cid3],
                       'CQ-DEPEND=%s %s' % (cid2, cid3))
    # Dumb comma behaviour
    self._CheckPaladin(git1, cid1, [cid2, cid3],
                      'CQ-DEPEND=%s, %s,' % (cid2, cid3))
    # Multiple keys.
    self._CheckPaladin(git1, cid1, [cid2, cid3, cid4],
                      'CQ-DEPEND=%s, %s\nCQ-DEPEND=%s' % (cid2, cid3, cid4))

    # Ensure it goes boom on invalid data.
    self.assertRaises(cros_patch.BrokenCQDepends, self._CheckPaladin,
                      git1, cid1, [], 'CQ-DEPEND=monkeys')
    self.assertRaises(cros_patch.BrokenCQDepends, self._CheckPaladin,
                      git1, cid1, [], 'CQ-DEPEND=%s monkeys' % (cid2,))
    # Validate numeric is allowed.
    self._CheckPaladin(git1, cid1, [cid2, '1'], 'CQ-DEPEND=1 %s' % cid2)
    # Validate that it unique'ifies the results.
    self._CheckPaladin(git1, cid1, ['1'], 'CQ-DEPEND=1 1')


class TestLocalPatchGit(TestGitRepoPatch):

  class patch_kls(_PatchSuppression, cros_patch.LocalPatch):
    pass

  def setUp(self):
    TestGitRepoPatch.setUp(self)
    self.sourceroot = os.path.join(self.tempdir, 'sourceroot')


  def _MkPatch(self, source, sha1, ref='refs/heads/master', **kwds):
    return self.patch_kls(source, 'chromiumos/chromite', ref,
                          'origin/master', sha1, self.sourceroot,
                          **kwds)

  def testUpload(self):
    def ProjectDirMock(sourceroot):
      self.assertEqual(sourceroot, self.sourceroot)
      return git1

    git1, git2, patch = self._CommonGitSetup()

    git2_sha1 = self._GetSha1(git2, 'HEAD')

    patch.ProjectDir = ProjectDirMock
    # First suppress carbon copy behaviour so we verify pushing
    # plain works.
    sha1 = patch.sha1
    patch._GetCarbonCopy = lambda: sha1
    patch.Upload('refs/testing/test1', push_url=git2)
    self.assertEqual(self._GetSha1(git2, 'refs/testing/test1'),
                     patch.sha1)

    # Enable CarbonCopy behaviour; verify it lands a different
    # sha1.  Additionally verify it didn't corrupt the patch's sha1 locally.
    del patch._GetCarbonCopy
    patch.Upload('refs/testing/test2', push_url=git2)
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

  PROJECT = 'chromiumos/chromite'
  ORIGINAL_BRANCH = 'original_branch'
  ORIGINAL_SHA1 = 'ffffffff'

  class patch_kls(_PatchSuppression, cros_patch.UploadedLocalPatch):
    pass

  def _MkPatch(self, source, sha1, ref='refs/heads/master', **kwds):
    return self.patch_kls(source, self.PROJECT, ref,
                          'origin/master', self.ORIGINAL_BRANCH,
                          self.ORIGINAL_SHA1, sha1, **kwds)

  def testStringRepresentation(self):
    git1, git2, patch = self._CommonGitSetup()
    str_rep = str(patch).split(':')
    for element in [self.PROJECT, self.ORIGINAL_BRANCH, self.ORIGINAL_SHA1]:
      self.assertTrue(element in str_rep)


class TestGerritPatch(TestGitRepoPatch):

  has_native_change_id = True

  class patch_kls(_PatchSuppression, cros_patch.GerritPatch):
    # Suppress the behaviour pointing the project url at actual gerrit,
    # instead slaving it back to a local repo for tests.
    def _GetProjectUrl(self, project, internal):
      assert hasattr(self, 'patch_dict')
      return self.patch_dict['_unittest_url_bypass']

  def test_GetProjectUrl(self):
    # We test this since we explicitly override the behaviour
    # for all other usage.
    kls = cros_patch.GerritPatch
    self.assertEqual(
        kls._GetProjectUrl('monkeys', False),
        os.path.join(kls._PUBLIC_URL, 'monkeys'))
    self.assertEqual(
        kls._GetProjectUrl('monkeys', True),
        os.path.join(constants.GERRIT_INT_SSH_URL, 'monkeys'))

  @property
  def test_json(self):
    return copy.deepcopy(FAKE_PATCH_JSON)

  def _MkPatch(self, source, sha1, ref='refs/heads/master', **kwds):
    json = self.test_json
    internal = kwds.pop('internal', False)
    suppress_branch = kwds.pop('suppress_branch', False)
    change_id = kwds.pop('ChangeId', None)
    if change_id is None:
      change_id = self.MakeChangeId()
    json.update(kwds)
    change_num, patch_num = _GetNumber(), _GetNumber()
    # Note we intentionally use a gerrit like refspec here; we want to
    # ensure that none of our common code pathways puke on a non head/tag.
    refspec = 'refs/changes/%i/%i/%i' % (
        change_num % 100, change_num + 1000, patch_num)
    json['currentPatchSet'].update(
        dict(number=patch_num, ref=refspec, revision=sha1))
    json['branch'] = os.path.basename(ref)
    json['_unittest_url_bypass'] = source
    json['id'] = change_id

    obj = self.patch_kls(json.copy(), internal)
    self.assertEqual(obj.patch_dict, json)
    self.assertEqual(obj.internal, internal)
    self.assertEqual(obj.project, json['project'])
    self.assertEqual(obj.ref, refspec)
    self.assertEqual(obj.id, change_id)
    # Now make the fetching actually work, if desired.
    if not suppress_branch:
      # Note that a push is needed here, rather than a branch; branch
      # will just make it under refs/heads, we want it literally in
      # refs/changes/
      self._run(['git', 'push', source, '%s:%s' % (sha1, refspec)], source)
    return obj

  def testIsAlreadyMerged(self):
    # Note that these are magic constants- they're known to be
    # merged (and the other abandoned) in public gerrit.
    # If old changes are ever flushed, or something 'special' occurs,
    # then this will break.  That it's an acceptable risk.
    # Note we should be checking a known open one; seems rather likely
    # that'll get closed inadvertantly thus breaking the tests (not
    # an acceptable risk in the authors opinion).
    merged, abandoned, still_open = gerrit_helper.GetGerritPatchInfo(
        [GERRIT_MERGED_CHANGEID, GERRIT_ABANDONED_CHANGEID,
         GERRIT_OPEN_CHANGEID])
    self.assertTrue(merged.IsAlreadyMerged())
    self.assertFalse(abandoned.IsAlreadyMerged())
    self.assertFalse(still_open.IsAlreadyMerged())

  @property
  def test_json(self):
    return copy.deepcopy(FAKE_PATCH_JSON)


class PrepareLocalPatchesTests(cros_test_lib.TempDirMixin, mox.MoxTestBase):

  def setUp(self):
    cros_test_lib.TempDirMixin.setUp(self)
    mox.MoxTestBase.setUp(self)

    self.sourceroot = self.tempdir
    self.patches = ['my/project:mybranch']

    self.mox.StubOutWithMock(cros_lib, 'GetProjectDir')
    self.mox.StubOutWithMock(cros_patch, '_GetRemoteTrackingBranch')
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')

  def tearDown(self):
    cros_test_lib.TempDirMixin.tearDown(self)
    mox.MoxTestBase.tearDown(self)

  def VerifyPatchInfo(self, patch_info, project, branch, tracking_branch):
    """Check the returned GitRepoPatchInfo against golden values."""
    self.assertEquals(patch_info.project, project)
    self.assertEquals(patch_info.ref, branch)
    self.assertEquals(patch_info.tracking_branch, tracking_branch)
    self.assertEquals(patch_info.sourceroot, self.sourceroot)

  def testBranchSpecifiedSuccessRun(self):
    """Test success with branch specified by user."""
    output_obj = self.mox.CreateMock(cros_lib.CommandResult)
    output_obj.output= '12345'
    cros_lib.GetProjectDir(mox.IgnoreArg(), 'my/project').AndReturn('mydir')
    cros_lib.RunCommand(mox.In('m/master..mybranch'),
                        redirect_stdout=mox.IgnoreArg(),
                        cwd='mydir').AndReturn(output_obj)
    cros_patch._GetRemoteTrackingBranch('mydir',
                                 'mybranch').AndReturn('tracking_branch')
    self.mox.ReplayAll()

    patch_info = cros_patch.PrepareLocalPatches(self.sourceroot,
                                                self.patches, 'master')
    self.VerifyPatchInfo(patch_info[0], 'my/project', 'mybranch',
                         'tracking_branch')
    self.mox.VerifyAll()

  def testBranchSpecifiedNoChanges(self):
    """Test when no changes on the branch specified by user."""
    output_obj = self.mox.CreateMock(cros_lib.CommandResult)
    output_obj.output=''
    cros_lib.GetProjectDir(mox.IgnoreArg(), 'my/project').AndReturn('mydir')
    cros_lib.RunCommand(mox.In('m/master..mybranch'),
                        redirect_stdout=mox.IgnoreArg(),
                        cwd='mydir').AndReturn(output_obj)
    self.mox.ReplayAll()

    self.assertRaises(
        cros_patch.PatchException,
        cros_patch.PrepareLocalPatches,
        self.sourceroot,
        self.patches,
        'master')
    self.mox.VerifyAll()

  def testNoTrackingBranch(self):
    """Test when project branch does not track a remote branch."""
    output_obj = self.mox.CreateMock(cros_lib.CommandResult)
    output_obj.output= '12345'
    cros_lib.GetProjectDir(mox.IgnoreArg(), 'my/project').AndReturn('mydir')
    cros_lib.RunCommand(mox.In('m/master..mybranch'),
                        redirect_stdout=mox.IgnoreArg(),
                        cwd='mydir').AndReturn(output_obj)
    cros_patch._GetRemoteTrackingBranch(
        'mydir',
        'mybranch').AndRaise(cros_lib.NoTrackingBranchException('error'))
    self.mox.ReplayAll()

    self.assertRaises(cros_patch.PatchException, cros_patch.PrepareLocalPatches,
                      self.sourceroot, self.patches, 'master')
    self.mox.VerifyAll()


class ApplyLocalPatchesTests(mox.MoxTestBase):

  def testWrongTrackingBranch(self):
    """When the original patch branch does not track buildroot's branch."""

    patch = cros_patch.GitRepoPatch('/path/to/my/project.git',
                                    'my/project', 'mybranch',
                                    'master')
    self.assertRaises(cros_patch.PatchException, patch.ApplyIntoGitRepo,
                      '/tmp/notadirectory', 'origin/R19')

if __name__ == '__main__':
  logging_format = '%(asctime)s - %(filename)s - %(levelname)-8s: %(message)s'
  date_format = constants.LOGGER_DATE_FMT
  logging.basicConfig(level=logging.DEBUG, format=logging_format,

                      datefmt=date_format)
  unittest.main()
