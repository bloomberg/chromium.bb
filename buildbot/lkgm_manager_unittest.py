#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for lkgm_manager. Needs to be run inside of chroot for mox."""

import mox
import os
import shutil
import sys
import tempfile
import threading
import time
import unittest
from xml.dom import minidom

if __name__ == '__main__':
  import constants
  sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import manifest_version_unittest
from chromite.buildbot import patch
from chromite.lib import cros_build_lib as cros_lib


FAKE_VERSION_STRING = '1.2.3.4-rc3'
FAKE_VERSION_STRING_NEXT = '1.2.3.4-rc4'


class LKGMCandidateInfoTest(mox.MoxTestBase):
  """Test methods testing methods in _LKGMCandidateInfo class."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.tmpdir = tempfile.mkdtemp()

  def testLoadFromString(self):
    """Tests whether we can load from a string."""
    info = lkgm_manager._LKGMCandidateInfo(version_string=FAKE_VERSION_STRING)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testIncrementVersionPatch(self):
    """Tests whether we can increment a lkgm info."""
    info = lkgm_manager._LKGMCandidateInfo(version_string=FAKE_VERSION_STRING)
    info.IncrementVersion()
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING_NEXT)

  def testVersionCompare(self):
    """Tests whether our comparision method works."""
    info1 = lkgm_manager._LKGMCandidateInfo('1.2.3.4-rc1')
    info2 = lkgm_manager._LKGMCandidateInfo('1.2.3.4-rc2')
    info3 = lkgm_manager._LKGMCandidateInfo('1.2.200.4-rc1')
    info4 = lkgm_manager._LKGMCandidateInfo('1.4.3.4-rc1')

    self.assertTrue(info2 > info1)
    self.assertTrue(info3 > info1)
    self.assertTrue(info3 > info2)
    self.assertTrue(info4 > info1)
    self.assertTrue(info4 > info2)
    self.assertTrue(info4 > info3)

  def tearDown(self):
    shutil.rmtree(self.tmpdir)


class LKGMManagerTest(mox.MoxTestBase):
  """Tests for the BuildSpecs manager."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)

    self.tmpdir = tempfile.mkdtemp()
    self.source_repo = 'ssh://source/repo'
    self.manifest_repo = 'ssh://manifest/repo'
    self.version_file = 'version-file.sh'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'patch'

    # Change default to something we clean up.
    self.tmpmandir = tempfile.mkdtemp()
    manifest_version.BuildSpecsManager._TMP_MANIFEST_DIR = self.tmpmandir

    self.manager = lkgm_manager.LKGMManager(
      self.tmpdir, self.source_repo, self.manifest_repo, self.branch,
      self.build_name, 'binary', dry_run=True)

    self.manager.all_specs_dir = '/LKGM/path'
    self.manager.specs_for_builder = os.path.join(self.manager.GetManifestDir(),
                                                  self.manager.lkgm_subdir,
                                                  'build-name', '%(builder)s')
    self.manager.SLEEP_TIMEOUT = 1

  def _CommonTestLatestCandidateByVersion(self, version, expected_candidate,
                                          no_all=False):
    """Common helper function for latest candidate tests.

    Helper function to test given a version whether we can the right candidate
    back.

    Args:
      version: The Chrome OS version to look for the latest candidate of.
      expected_candidate: What we expect to come back.
    """
    if not no_all:
      self.manager.all = ['1.2.3.4-rc1',
                          '1.2.3.4-rc2',
                          '1.2.3.4-rc9',
                          '1.2.3.5-rc1',
                          '1.2.3.6-rc2',
                          '1.2.4.3-rc1',
                          ]

    info_for_test = manifest_version.VersionInfo(version)
    candidate = self.manager._GetLatestCandidateByVersion(info_for_test)
    self.assertEqual(candidate.VersionString(), expected_candidate)

  def testGetLatestCandidateByVersionCommonCase(self):
    """Tests whether we can get the latest candidate under the common case.

    This test tests whether or not we get the right candidate when we have
    many of the same candidate version around but with different rc's.
    """
    self._CommonTestLatestCandidateByVersion('1.2.3.4', '1.2.3.4-rc9')

  def testGetLatestCandidateByVersionOnlyOne(self):
    """Tests whether we can get the latest candidate with only one rc."""
    self._CommonTestLatestCandidateByVersion('1.2.3.6', '1.2.3.6-rc2')

  def testGetLatestCandidateByVersionNone(self):
    """Tests whether we can get the latest candidate with no rc's."""
    self._CommonTestLatestCandidateByVersion('1.2.5.7', '1.2.5.7-rc1')

  def testGetLatestCandidateByVersionNoneNoAll(self):
    """Tests whether we can get the latest candidate with no rc's at all."""
    self._CommonTestLatestCandidateByVersion('10.0.1.5', '10.0.1.5-rc1', True)

  def _GetPathToManifest(self, info):
    return os.path.join(self.manager.all_specs_dir, '%s.xml' %
                        info.VersionString())

  def testCreateNewCandidate(self):
    """Tests that we can create a new candidate and uprev and old rc."""
    # Let's stub out other LKGMManager calls cause they're already
    # unit tested.
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_LoadSpecs')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             '_GetLatestCandidateByVersion')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_CreateNewBuildSpec')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_SetInFlight')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_PrepSpecChanges')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_PushSpecChanges')

    my_info = manifest_version.VersionInfo('1.2.3.4')
    most_recent_candidate = lkgm_manager._LKGMCandidateInfo('1.2.3.4-rc12')
    new_candidate = lkgm_manager._LKGMCandidateInfo('1.2.3.4-rc13')

    lkgm_manager.LKGMManager._GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager._LoadSpecs(my_info)
    lkgm_manager.LKGMManager._GetLatestCandidateByVersion(my_info).AndReturn(
        most_recent_candidate)
    lkgm_manager.LKGMManager._PrepSpecChanges()
    lkgm_manager.LKGMManager._CreateNewBuildSpec(
        most_recent_candidate).AndReturn(new_candidate.VersionString())
    lkgm_manager.LKGMManager._SetInFlight()
    lkgm_manager.LKGMManager._PushSpecChanges(
        mox.StrContains(new_candidate.VersionString()))

    self.mox.ReplayAll()
    candidate_path = self.manager.CreateNewCandidate()
    self.assertEqual(candidate_path, self._GetPathToManifest(new_candidate))
    self.mox.VerifyAll()

  def testCreateNewCandidateReturnNoneIfNoWorkToDo(self):
    """Tests that we return nothing if there is nothing to create."""
    # Let's stub out other LKGMManager calls cause they're already
    # unit tested.
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_LoadSpecs')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager,
                             '_GetLatestCandidateByVersion')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_CreateNewBuildSpec')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_SetInFlight')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_PrepSpecChanges')


    my_info = manifest_version.VersionInfo('1.2.3.4')
    most_recent_candidate = lkgm_manager._LKGMCandidateInfo('1.2.3.4-rc12')

    lkgm_manager.LKGMManager._GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager._LoadSpecs(my_info)
    lkgm_manager.LKGMManager._GetLatestCandidateByVersion(my_info).AndReturn(
        most_recent_candidate)
    lkgm_manager.LKGMManager._PrepSpecChanges()
    lkgm_manager.LKGMManager._CreateNewBuildSpec(
        most_recent_candidate).AndReturn(None)

    self.mox.ReplayAll()
    candidate = self.manager.CreateNewCandidate()
    self.assertEqual(candidate, None)
    self.mox.VerifyAll()

  def testGetLatestCandidate(self):
    """Makes sure we can get the latest created candidate manifest."""
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_LoadSpecs')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_SetInFlight')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_PrepSpecChanges')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_PushSpecChanges')

    my_info = manifest_version.VersionInfo('1.2.3.4')
    most_recent_candidate = lkgm_manager._LKGMCandidateInfo('1.2.3.4-rc12')

    lkgm_manager.LKGMManager._GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager._LoadSpecs(my_info)
    lkgm_manager.LKGMManager._PrepSpecChanges()
    lkgm_manager.LKGMManager._SetInFlight()
    lkgm_manager.LKGMManager._PushSpecChanges(
        mox.StrContains(most_recent_candidate.VersionString()))

    self.mox.ReplayAll()
    self.manager.latest_unprocessed = '1.2.3.4-rc12'
    candidate = self.manager.GetLatestCandidate()
    self.assertEqual(candidate, self._GetPathToManifest(most_recent_candidate))
    self.mox.VerifyAll()

  def testGetLatestCandidateOneRetry(self):
    """Makes sure we can get the latest candidate even on retry."""
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_LoadSpecs')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_SetInFlight')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_PrepSpecChanges')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_PushSpecChanges')

    my_info = manifest_version.VersionInfo('1.2.3.4')
    most_recent_candidate = lkgm_manager._LKGMCandidateInfo('1.2.3.4-rc12')

    lkgm_manager.LKGMManager._GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager._LoadSpecs(my_info)
    lkgm_manager.LKGMManager._PrepSpecChanges()
    lkgm_manager.LKGMManager._SetInFlight()
    lkgm_manager.LKGMManager._PushSpecChanges(
        mox.StrContains(most_recent_candidate.VersionString())).AndRaise(
            manifest_version.GitCommandException('Push failed'))

    lkgm_manager.LKGMManager._PrepSpecChanges()
    lkgm_manager.LKGMManager._SetInFlight()
    lkgm_manager.LKGMManager._PushSpecChanges(
        mox.StrContains(most_recent_candidate.VersionString()))

    self.mox.ReplayAll()
    self.manager.latest_unprocessed = '1.2.3.4-rc12'
    candidate = self.manager.GetLatestCandidate()
    self.assertEqual(candidate, self._GetPathToManifest(most_recent_candidate))
    self.mox.VerifyAll()

  def testGetLatestCandidateNone(self):
    """Makes sure we get nothing if there is no work to be done."""
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_GetCurrentVersionInfo')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, '_LoadSpecs')

    my_info = manifest_version.VersionInfo('1.2.3.4')
    lkgm_manager.LKGMManager._GetCurrentVersionInfo().AndReturn(my_info)
    lkgm_manager.LKGMManager._LoadSpecs(my_info)

    self.mox.ReplayAll()
    self.manager.MAX_TIMEOUT_SECONDS = 1 # Only run once.
    candidate = self.manager.GetLatestCandidate()
    self.assertEqual(candidate, None)
    self.mox.VerifyAll()

  def _CreateManifest(self):
    """Returns a created test manifest in tmpdir with its dir_pfx."""
    self.manager.current_version = '1.2.3.4-rc21'
    dir_pfx = '1.2'
    manifest = os.path.join(self.manager._TMP_MANIFEST_DIR,
                            self.manager.lkgm_subdir, 'buildspecs',
                            dir_pfx, '1.2.3.4-rc21.xml')
    manifest_version_unittest.TouchFile(manifest)
    return manifest, dir_pfx

  @staticmethod
  def _FinishBuilderHelper(manifest, path_for_builder, dir_pfx, status, wait=0):
    time.sleep(wait)
    manifest_version.CreateSymlink(
        manifest, os.path.join(path_for_builder, status, dir_pfx,
                               os.path.basename(manifest)))

  def _FinishBuild(self, manifest, path_for_builder, dir_pfx, status, wait=0):
    """Finishes a build by marking a status with optional delay."""
    if wait > 0:
      thread = threading.Thread(target=self._FinishBuilderHelper,
                                args=(manifest, path_for_builder, dir_pfx,
                                      status, wait))
      thread.start()
      return thread
    else:
      self._FinishBuilderHelper(manifest, path_for_builder, dir_pfx, status, 0)

  def testGetBuildersStatusBothFinished(self):
    """Tests GetBuilderStatus where both builds have finished."""
    self.mox.StubOutWithMock(lkgm_manager, '_SyncGitRepo')

    manifest, dir_pfx = self._CreateManifest()
    for_build1 = os.path.join(self.manager._TMP_MANIFEST_DIR,
                              self.manager.lkgm_subdir,
                              'build-name', 'build1')
    for_build2 = os.path.join(self.manager._TMP_MANIFEST_DIR,
                              self.manager.lkgm_subdir,
                              'build-name', 'build2')

    self._FinishBuild(manifest, for_build1, dir_pfx, 'fail')
    self._FinishBuild(manifest, for_build2, dir_pfx, 'pass')

    lkgm_manager._SyncGitRepo(self.manager._TMP_MANIFEST_DIR)
    self.mox.ReplayAll()
    statuses = self.manager.GetBuildersStatus(['build1', 'build2'])
    self.assertEqual(statuses['build1'], 'fail')
    self.assertEqual(statuses['build2'], 'pass')
    self.mox.VerifyAll()

  def testGetBuildersStatusWaitForOne(self):
    """Tests GetBuilderStatus where both builds have finished with one delay."""
    self.mox.StubOutWithMock(lkgm_manager, '_SyncGitRepo')

    manifest, dir_pfx = self._CreateManifest()
    for_build1 = os.path.join(self.manager._TMP_MANIFEST_DIR,
                              self.manager.lkgm_subdir,
                              'build-name', 'build1')
    for_build2 = os.path.join(self.manager._TMP_MANIFEST_DIR,
                              self.manager.lkgm_subdir,
                              'build-name', 'build2')

    self._FinishBuild(manifest, for_build1, dir_pfx, 'fail')
    self._FinishBuild(manifest, for_build2, dir_pfx, 'pass', wait=3)

    lkgm_manager._SyncGitRepo(self.manager._TMP_MANIFEST_DIR).MultipleTimes()
    self.mox.ReplayAll()

    statuses = self.manager.GetBuildersStatus(['build1', 'build2'])
    self.assertEqual(statuses['build1'], 'fail')
    self.assertEqual(statuses['build2'], 'pass')
    self.mox.VerifyAll()

  def testGetBuildersStatusReachTimeout(self):
    """Tests GetBuilderStatus where one build finishes and one never does."""
    self.mox.StubOutWithMock(lkgm_manager, '_SyncGitRepo')

    manifest, dir_pfx = self._CreateManifest()
    for_build1 = os.path.join(self.manager._TMP_MANIFEST_DIR,
                              self.manager.lkgm_subdir,
                              'build-name', 'build1')
    for_build2 = os.path.join(self.manager._TMP_MANIFEST_DIR,
                              self.manager.lkgm_subdir,
                              'build-name', 'build2')

    self._FinishBuild(manifest, for_build1, dir_pfx, 'fail', wait=3)
    thread = self._FinishBuild(manifest, for_build2, dir_pfx, 'pass', wait=10)

    lkgm_manager._SyncGitRepo(self.manager._TMP_MANIFEST_DIR).MultipleTimes()
    self.mox.ReplayAll()
    # Let's reduce this.
    self.manager.LONG_MAX_TIMEOUT_SECONDS = 5
    statuses = self.manager.GetBuildersStatus(['build1', 'build2'])
    self.assertEqual(statuses['build1'], 'fail')
    self.assertEqual(statuses['build2'], None)
    thread.join()
    self.mox.VerifyAll()

  def testGenerateBlameListSinceLKGM(self):
    """Tests that we can generate a blamelist from one commit message."""
    fake_git_log = """Author: Sammy Sosa <fake@fake.com>

    Date:   Mon Aug 8 14:52:06 2011 -0700

    Add in a test for cbuildbot

    TEST=So much testing
    BUG=chromium-os:99999

    Change-Id: Ib72a742fd2cee3c4a5223b8easwasdgsdgfasdf
    Reviewed-on: http://gerrit.chromium.org/gerrit/1234
    Reviewed-by: Fake person <fake@fake.org>
    Tested-by: Sammy Sosa <fake@fake.com>
    """
    fake_revision = '1234567890'
    fake_project_handler = self.mox.CreateMock(cros_lib.ManifestHandler)
    fake_project_handler.projects = { 'fake/repo': { 'name': 'fake/repo',
                                                     'path': 'fake/path',
                                                     'revision': fake_revision,
                                                   }
                                    }
    fake_result = self.mox.CreateMock(cros_lib.CommandResult)
    fake_result.output = fake_git_log

    self.mox.StubOutWithMock(cros_lib.ManifestHandler, 'ParseManifest')

    cros_lib.ManifestHandler.ParseManifest(
        self.tmpmandir + '/LKGM/lkgm.xml').AndReturn(fake_project_handler)
    self.mox.ReplayAll()
    self.manager.GenerateBlameListSinceLKGM()
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

      gerrit_patch = self.mox.CreateMock(patch.GerritPatch)
      gerrit_patch.project = 'chromite/tacos'
      gerrit_patch.id = '1234567890'
      gerrit_patch.commit = '0987654321'
      self.manager.AddPatchesToManifest(tmp_manifest, [gerrit_patch])

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


  def tearDown(self):
    if os.path.exists(self.tmpdir): shutil.rmtree(self.tmpdir)
    shutil.rmtree(self.tmpmandir)


if __name__ == '__main__':
  unittest.main()
