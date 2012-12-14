#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for lkgm_manager. Needs to be run inside of chroot for mox."""

import mock
import mox
import os
import sys
import tempfile
from xml.dom import minidom

if __name__ == '__main__':
  import constants
  sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import repository
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils


# pylint: disable=E1120,W0212,R0904
FAKE_VERSION_STRING = '1.2.4-rc3'
FAKE_VERSION_STRING_NEXT = '1.2.4-rc4'
CHROME_BRANCH = '13'

FAKE_VERSION = """
CHROMEOS_BUILD=1
CHROMEOS_BRANCH=2
CHROMEOS_PATCH=4
CHROME_BRANCH=13
"""


class LKGMCandidateInfoTest(cros_test_lib.TestCase):
  """Test methods testing methods in _LKGMCandidateInfo class."""

  def testLoadFromString(self):
    """Tests whether we can load from a string."""
    info = lkgm_manager._LKGMCandidateInfo(version_string=FAKE_VERSION_STRING,
                                           chrome_branch=CHROME_BRANCH)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testIncrementVersionPatch(self):
    """Tests whether we can increment a lkgm info."""
    info = lkgm_manager._LKGMCandidateInfo(version_string=FAKE_VERSION_STRING,
                                           chrome_branch=CHROME_BRANCH)
    info.IncrementVersion()
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING_NEXT)

  def testVersionCompare(self):
    """Tests whether our comparision method works."""
    info1 = lkgm_manager._LKGMCandidateInfo('1.2.3-rc1')
    info2 = lkgm_manager._LKGMCandidateInfo('1.2.3-rc2')
    info3 = lkgm_manager._LKGMCandidateInfo('1.2.200-rc1')
    info4 = lkgm_manager._LKGMCandidateInfo('1.4.3-rc1')

    self.assertTrue(info2 > info1)
    self.assertTrue(info3 > info1)
    self.assertTrue(info3 > info2)
    self.assertTrue(info4 > info1)
    self.assertTrue(info4 > info2)
    self.assertTrue(info4 > info3)


class LKGMManagerTest(cros_test_lib.MoxTempDirTestCase):
  """Tests for the BuildSpecs manager."""

  def _CreateFakeManifest(self, internal, entries, commits):
    """Creates a fake manifest with some internal projects."""
    tmp_manifest = tempfile.mktemp('manifest')
    # Create fake but empty manifest file.
    new_doc = minidom.getDOMImplementation().createDocument(None, 'manifest',
                                                            None)
    m_element = new_doc.getElementsByTagName('manifest')[0]
    for idx in xrange(entries):
      internal_element = idx % 2 == 0 and internal
      new_element = minidom.Element('project')
      new_element.setAttribute('name', 'project_%d' % idx)
      new_element.setAttribute('path', 'some_path/to/project_%d' % idx)
      new_element.setAttribute('revision', 'revision_%d' % idx)
      if internal_element:
        new_element.setAttribute('remote', 'cros-internal')

      m_element.appendChild(new_element)

    for idx in xrange(commits):
      internal_element = idx % 2 == 0 and internal
      new_element = minidom.Element('pending_commit')
      new_element.setAttribute('project', 'project_%d' % idx)
      new_element.setAttribute('change_id', 'changeid_%d' % idx)
      new_element.setAttribute('commit', 'commit_%d' % idx)
      m_element.appendChild(new_element)

    with open(tmp_manifest, 'w+') as manifest_file:
      new_doc.writexml(manifest_file, newl='\n')

    return tmp_manifest

  def setUp(self):
    self.mox.StubOutWithMock(git, 'CreatePushBranch')

    self.source_repo = 'ssh://source/repo'
    self.manifest_repo = 'ssh://manifest/repo'
    self.version_file = 'version-file.sh'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'branch'

    # Create tmp subdirs based on the one provided TempDirMixin.
    self.tmpdir = os.path.join(self.tempdir, "base")
    osutils.SafeMakedirs(self.tmpdir)
    self.tmpmandir = os.path.join(self.tempdir, "man")
    osutils.SafeMakedirs(self.tmpmandir)

    repo = repository.RepoRepository(
      self.source_repo, self.tmpdir, self.branch, depth=1)
    self.manager = lkgm_manager.LKGMManager(
      repo, self.manifest_repo, self.build_name, constants.PFQ_TYPE, 'branch',
      force=False, branch=self.branch, dry_run=True)
    self.manager.manifest_dir = self.tmpmandir
    self.manager.lkgm_path = os.path.join(self.tmpmandir,
                                          self.manager.LKGM_PATH)

    self.manager.all_specs_dir = '/LKGM/path'
    manifest_dir = self.manager.manifest_dir
    self.manager.specs_for_builder = os.path.join(manifest_dir,
                                                  self.manager.rel_working_dir,
                                                  'build-name', '%(builder)s')
    self.manager.SLEEP_TIMEOUT = 0

  def _GetPathToManifest(self, info):
    return os.path.join(self.manager.all_specs_dir, '%s.xml' %
                        info.VersionString())

  def testCreateNewCandidate(self):
    """Tests that we can create a new candidate and uprev an old rc."""
    # Let's stub out other LKGMManager calls cause they're already
    # unit tested.
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'CheckoutSourceCode')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'RefreshManifestCheckout')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'InitializeManifestVariables')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'HasCheckoutBeenBuilt')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'CreateManifest')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'PublishManifest')

    my_info = lkgm_manager._LKGMCandidateInfo('1.2.3')
    most_recent_candidate = lkgm_manager._LKGMCandidateInfo('1.2.3-rc12')
    self.manager.latest = most_recent_candidate.VersionString()

    new_candidate = lkgm_manager._LKGMCandidateInfo('1.2.3-rc13')
    new_manifest = 'some_manifest'

    lkgm_manager.LKGMManager.CheckoutSourceCode()
    lkgm_manager.LKGMManager.CreateManifest().AndReturn(new_manifest)
    lkgm_manager.LKGMManager.HasCheckoutBeenBuilt().AndReturn(False)

    # Do manifest refresh work.
    lkgm_manager.LKGMManager.RefreshManifestCheckout()
    git.CreatePushBranch(mox.IgnoreArg(), mox.IgnoreArg(), sync=False)
    lkgm_manager.LKGMManager.GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager.InitializeManifestVariables(my_info)

    # Publish new candidate.
    lkgm_manager.LKGMManager.PublishManifest(new_manifest,
                                             new_candidate.VersionString())

    self.mox.ReplayAll()
    candidate_path = self.manager.CreateNewCandidate()
    self.assertEqual(candidate_path, self._GetPathToManifest(new_candidate))
    self.mox.VerifyAll()

  def testCreateFromManifest(self):
    """Tests that we can create a new candidate from another manifest."""
    # Let's stub out other LKGMManager calls cause they're already
    # unit tested.
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'RefreshManifestCheckout')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'InitializeManifestVariables')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             '_FilterCrosInternalProjectsFromManifest')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'PublishManifest')

    version = '2010.0.0-rc7'
    my_info = lkgm_manager._LKGMCandidateInfo('2010.0.0')
    new_candidate = lkgm_manager._LKGMCandidateInfo(version)
    manifest = ('/tmp/manifest-versions-internal/paladin/buildspecs/'
                '20/%s.xml' % version)
    new_manifest = '/path/to/tmp/file.xml'

    lkgm_manager.LKGMManager._FilterCrosInternalProjectsFromManifest(
        manifest).AndReturn(new_manifest)

    # Do manifest refresh work.
    lkgm_manager.LKGMManager.GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager.RefreshManifestCheckout()
    lkgm_manager.LKGMManager.InitializeManifestVariables(my_info)
    git.CreatePushBranch(mox.IgnoreArg(), mox.IgnoreArg(), sync=False)

    # Publish new candidate.
    lkgm_manager.LKGMManager.PublishManifest(new_manifest, version)

    self.mox.ReplayAll()
    candidate_path = self.manager.CreateFromManifest(manifest)
    self.assertEqual(candidate_path, self._GetPathToManifest(new_candidate))
    self.assertEqual(self.manager.current_version, version)
    self.mox.VerifyAll()

  def testCreateNewCandidateReturnNoneIfNoWorkToDo(self):
    """Tests that we return nothing if there is nothing to create."""
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'CheckoutSourceCode')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'HasCheckoutBeenBuilt')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'CreateManifest')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'RefreshManifestCheckout')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'InitializeManifestVariables')

    new_manifest = 'some_manifest'
    my_info = lkgm_manager._LKGMCandidateInfo('1.2.3')
    lkgm_manager.LKGMManager.CheckoutSourceCode()
    lkgm_manager.LKGMManager.CreateManifest().AndReturn(new_manifest)
    lkgm_manager.LKGMManager.RefreshManifestCheckout()
    lkgm_manager.LKGMManager.GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager.InitializeManifestVariables(my_info)
    lkgm_manager.LKGMManager.HasCheckoutBeenBuilt().AndReturn(True)

    self.mox.ReplayAll()
    candidate = self.manager.CreateNewCandidate()
    self.assertEqual(candidate, None)
    self.mox.VerifyAll()

  def testGetLatestCandidate(self):
    """Makes sure we can get the latest created candidate manifest."""
    self.mox.StubOutWithMock(repository.RepoRepository, 'Sync')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'RefreshManifestCheckout')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'InitializeManifestVariables')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'CheckoutSourceCode')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'PushSpecChanges')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'SetInFlight')

    my_info = lkgm_manager._LKGMCandidateInfo('1.2.3')
    most_recent_candidate = lkgm_manager._LKGMCandidateInfo('1.2.3-rc12')

    # Do manifest refresh work.
    lkgm_manager.LKGMManager.CheckoutSourceCode()
    lkgm_manager.LKGMManager.RefreshManifestCheckout()
    lkgm_manager.LKGMManager.GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager.InitializeManifestVariables(my_info)

    lkgm_manager.LKGMManager.SetInFlight(most_recent_candidate.VersionString())
    repository.RepoRepository.Sync(
        self._GetPathToManifest(most_recent_candidate))

    self.manager.latest_unprocessed = '1.2.3-rc12'
    self.mox.ReplayAll()
    candidate = self.manager.GetLatestCandidate()
    self.assertEqual(candidate, self._GetPathToManifest(most_recent_candidate))
    self.mox.VerifyAll()

  def testGetLatestCandidateOneRetry(self):
    """Makes sure we can get the latest candidate even on retry."""
    self.mox.StubOutWithMock(repository.RepoRepository, 'Sync')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'RefreshManifestCheckout')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'InitializeManifestVariables')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'CheckoutSourceCode')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'PushSpecChanges')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'SetInFlight')

    my_info = lkgm_manager._LKGMCandidateInfo('1.2.4')
    most_recent_candidate = lkgm_manager._LKGMCandidateInfo('1.2.4-rc12',
                                                            CHROME_BRANCH)

    lkgm_manager.LKGMManager.CheckoutSourceCode()
    lkgm_manager.LKGMManager.RefreshManifestCheckout()
    lkgm_manager.LKGMManager.GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager.InitializeManifestVariables(my_info)

    lkgm_manager.LKGMManager.SetInFlight(most_recent_candidate.VersionString())
    repository.RepoRepository.Sync(
        self._GetPathToManifest(most_recent_candidate))

    self.manager.latest_unprocessed = '1.2.4-rc12'
    self.mox.ReplayAll()
    candidate = self.manager.GetLatestCandidate()
    self.assertEqual(candidate, self._GetPathToManifest(most_recent_candidate))
    self.mox.VerifyAll()

  def testGetLatestCandidateNone(self):
    """Makes sure we get nothing if there is no work to be done."""
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'RefreshManifestCheckout')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             'InitializeManifestVariables')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'CheckoutSourceCode')

    my_info = lkgm_manager._LKGMCandidateInfo('1.2.4')
    lkgm_manager.LKGMManager.CheckoutSourceCode()
    lkgm_manager.LKGMManager.RefreshManifestCheckout()
    lkgm_manager.LKGMManager.GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager.InitializeManifestVariables(my_info)

    self.mox.ReplayAll()
    self.manager.SLEEP_TIMEOUT = 0.2
    self.manager.LONG_MAX_TIMEOUT_SECONDS = 0.1 # Only run once.
    candidate = self.manager.GetLatestCandidate()
    self.assertEqual(candidate, None)
    self.mox.VerifyAll()

  def _CreateManifest(self):
    """Returns a created test manifest in tmpdir with its dir_pfx."""
    self.manager.current_version = '1.2.4-rc21'
    dir_pfx = CHROME_BRANCH
    manifest = os.path.join(self.manager.manifest_dir,
                            self.manager.rel_working_dir, 'buildspecs',
                            dir_pfx, '1.2.4-rc21.xml')
    osutils.Touch(manifest)
    return manifest, dir_pfx

  def _GetBuildersStatus(self, builders, status_runs):
    """Test a call to LKGMManager.GetBuildersStatus.

    Args:
      builders: List of builders to get status for.
      status_runs: List of expected (builder, status) tuples.
    """
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'GetBuildStatus')
    for builder, status in status_runs:
      # GetBuildStatus returns None if the builder has not even started yet
      # (e.g. because the builder is down.)
      if status is not None:
        status = manifest_version.BuilderStatus(status, None)
      lkgm_manager.LKGMManager.GetBuildStatus(
          builder, mox.IgnoreArg()).AndReturn(status)

    self.mox.ReplayAll()
    statuses = self.manager.GetBuildersStatus(builders)
    self.mox.VerifyAll()
    return statuses

  def testGetBuildersStatusBothFinished(self):
    """Tests GetBuilderStatus where both builds have finished."""
    status_runs = [('build1', 'fail'), ('build2', 'pass')]
    statuses = self._GetBuildersStatus(['build1', 'build2'], status_runs)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())

  def testGetBuildersStatusLoop(self):
    """Tests GetBuilderStatus where builds are inflight."""
    status_runs = [('build1', 'inflight'), ('build2', None),
                   ('build1', 'fail'), ('build2', 'inflight'),
                   ('build2', 'pass')]
    statuses = self._GetBuildersStatus(['build1', 'build2'], status_runs)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())

  def testGenerateBlameListSinceLKGM(self):
    """Tests that we can generate a blamelist from two commit messages.

    This test tests the functionality of generating a blamelist for a git log.
    Note in this test there are two commit messages, one commited by the
    Commit Queue and another from Non-Commit Queue.  We test the correct
    handling in both cases.
    """
    fake_git_log = """Author: Sammy Sosa <fake@fake.com>
    Commit: Chris Sosa <sosa@chromium.org>

    Date:   Mon Aug 8 14:52:06 2011 -0700

    Add in a test for cbuildbot

    TEST=So much testing
    BUG=chromium-os:99999

    Change-Id: Ib72a742fd2cee3c4a5223b8easwasdgsdgfasdf
    Reviewed-on: http://gerrit.chromium.org/gerrit/1234
    Reviewed-by: Fake person <fake@fake.org>
    Tested-by: Sammy Sosa <fake@fake.com>
    Author: Sammy Sosa <fake@fake.com>
    Commit: Gerrit <chrome-bot@chromium.org>

    Date:   Mon Aug 8 14:52:06 2011 -0700

    Add in a test for cbuildbot

    TEST=So much testing
    BUG=chromium-os:99999

    Change-Id: Ib72a742fd2cee3c4a5223b8easwasdgsdgfasdf
    Reviewed-on: http://gerrit.chromium.org/gerrit/1235
    Reviewed-by: Fake person <fake@fake.org>
    Tested-by: Sammy Sosa <fake@fake.com>
    """
    self.manager.incr_type = 'build'
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotLink')

    fake_revision = '1234567890'
    fake_project_handler = self.mox.CreateMock(git.Manifest)
    fake_project_handler.projects = { 'fake/repo': { 'name': 'fake/repo',
                                                     'path': 'fake/path',
                                                     'revision': fake_revision,
                                                   }
                                    }
    fake_result = self.mox.CreateMock(cros_build_lib.CommandResult)
    fake_result.output = fake_git_log

    self.mox.StubOutWithMock(git, 'Manifest', use_mock_anything=True)

    git.Manifest(
        self.tmpmandir + '/LKGM/lkgm.xml').AndReturn(fake_project_handler)
    os.path.exists(mox.StrContains('fake/path')).AndReturn(True)
    cros_build_lib.RunCommand(['git', 'log', '--pretty=full',
                               '%s..HEAD' % fake_revision],
                              print_cmd=False, redirect_stdout=True,
                              cwd=self.tmpdir + '/fake/path').AndReturn(
                                  fake_result)
    cros_build_lib.PrintBuildbotLink('CHUMP fake:1234',
                                     'http://gerrit.chromium.org/gerrit/1234')
    cros_build_lib.PrintBuildbotLink('fake:1235',
                                     'http://gerrit.chromium.org/gerrit/1235')
    self.mox.ReplayAll()
    self.manager._GenerateBlameListSinceLKGM()
    self.mox.VerifyAll()

  def testAddPatchesToManifest(self):
    """Tests whether we can add a fake patch to an empty manifest file.

    This test creates an empty xml file with just manifest/ tag in it then
    runs the AddPatchesToManifest with one mocked out GerritPatch and ensures
    the newly generated manifest has the correct patch information afterwards.
    """
    tmp_manifest = tempfile.mktemp('manifest')
    try:
      # Create fake but empty manifest file.
      new_doc = minidom.getDOMImplementation().createDocument(None, 'manifest',
                                                              None)
      with open(tmp_manifest, 'w+') as manifest_file:
        print new_doc.toxml()
        new_doc.writexml(manifest_file)

      gerrit_patch = mock.MagicMock()
      gerrit_patch.project = 'chromite/tacos'
      gerrit_patch.change_id = '1234567890'
      gerrit_patch.commit = '0987654321'
      self.manager._AddPatchesToManifest(tmp_manifest, [gerrit_patch])

      new_doc = minidom.parse(tmp_manifest)
      element = new_doc.getElementsByTagName(
          lkgm_manager.PALADIN_COMMIT_ELEMENT)[0]
      self.assertEqual(element.getAttribute(
          lkgm_manager.PALADIN_CHANGE_ID_ATTR), '1234567890')
      self.assertEqual(element.getAttribute(
          lkgm_manager.PALADIN_COMMIT_ATTR), '0987654321')
      self.assertEqual(element.getAttribute(lkgm_manager.PALADIN_PROJECT_ATTR),
                       'chromite/tacos')
    finally:
      os.remove(tmp_manifest)

  def testFilterProjectsFromManifest(self):
    """Tests whether or not we can correctly remove internal projects from a
    manifest."""
    fake_manifest = None
    fake_new_manifest = None
    try:
      fake_manifest = self._CreateFakeManifest(internal=True, entries=100,
                                               commits=4)
      fake_new_manifest = \
          lkgm_manager.LKGMManager._FilterCrosInternalProjectsFromManifest(
              fake_manifest)

      new_dom = minidom.parse(fake_new_manifest)
      projects = new_dom.getElementsByTagName('project')
      for p in projects:
        self.assertFalse(p.getAttribute('remote') == 'cros-internal')

      # Check commits.  All commits with project_X where X % 2 == 0 are
      # internal commits.
      commits = new_dom.getElementsByTagName('pending_commit')
      for c in commits:
        index = c.getAttribute('project').split('_')[-1]
        self.assertTrue(int(index) % 2 != 0)

    finally:
      if fake_manifest: os.remove(fake_manifest)
      if fake_new_manifest: os.remove(fake_new_manifest)

  def testFilterProjectsFromExternalManifest(self):
    """Tests filtering on a project where no filtering is needed."""
    fake_manifest = None
    fake_new_manifest = None
    try:
      fake_manifest = self._CreateFakeManifest(internal=False, entries=100,
                                               commits=20)
      fake_new_manifest = \
          lkgm_manager.LKGMManager._FilterCrosInternalProjectsFromManifest(
              fake_manifest)

      new_dom = minidom.parse(fake_new_manifest)
      projects = new_dom.getElementsByTagName('project')
      self.assertEqual(len(projects), 100)
      commits = new_dom.getElementsByTagName('pending_commit')
      self.assertEqual(len(commits), 20)

    finally:
      if fake_manifest: os.remove(fake_manifest)
      if fake_new_manifest: os.remove(fake_new_manifest)


if __name__ == '__main__':
  cros_test_lib.main()
