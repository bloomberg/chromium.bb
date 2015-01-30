# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for manifest_version. Needs to be run inside of chroot."""

from __future__ import print_function

import mox
import os
import random
import tempfile
from xml.dom import minidom

from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import repository
from chromite.lib import cros_build_lib_unittest
from chromite.lib import git
from chromite.lib import cros_test_lib
from chromite.lib import osutils


FAKE_VERSION = """
CHROMEOS_BUILD=%(build_number)s
CHROMEOS_BRANCH=%(branch_build_number)s
CHROMEOS_PATCH=%(patch_number)s
CHROME_BRANCH=%(chrome_branch)s
"""

FAKE_WHITELISTED_REMOTES = ('cros', 'chromium')
FAKE_NON_WHITELISTED_REMOTE = 'hottubtimemachine'

FAKE_VERSION_STRING = '1.2.3'
FAKE_VERSION_STRING_NEXT = '1.2.4'
CHROME_BRANCH = '13'

# Use the chromite repo to actually test git changes.
GIT_TEST_PATH = 'chromite'

MOCK_BUILD_ID = 162345


# pylint: disable=protected-access


class HelperMethodsTest(cros_test_lib.TempDirTestCase):
  """Test methods associated with methods not in a class."""

  def testCreateSymlink(self):
    """Tests that we can create symlinks and remove a previous one."""
    srcfile = os.path.join(self.tempdir, 'src')
    osutils.Touch(srcfile)
    other_dir = os.path.join(self.tempdir, 'other_dir')
    os.makedirs(other_dir)
    destfile = os.path.join(other_dir, 'dest')

    manifest_version.CreateSymlink(srcfile, destfile)
    self.assertTrue(os.path.lexists(destfile),
                    'Unable to create symlink to %s' % destfile)


class VersionInfoTest(cros_test_lib.MoxTempDirTestCase):
  """Test methods testing methods in VersionInfo class."""

  @classmethod
  def WriteFakeVersionFile(cls, version_file, version=None, chrome_branch=None):
    """Helper method to write a version file from specified version number."""
    if version is None:
      version = FAKE_VERSION_STRING
    if chrome_branch is None:
      chrome_branch = CHROME_BRANCH

    osutils.SafeMakedirs(os.path.split(version_file)[0])
    info = manifest_version.VersionInfo(version, chrome_branch)
    osutils.WriteFile(version_file, FAKE_VERSION % info.__dict__)

  @classmethod
  def CreateFakeVersionFile(cls, tmpdir, version=None, chrome_branch=None):
    """Helper method to create a version file from specified version number."""
    version_file = tempfile.mktemp(dir=tmpdir)
    cls.WriteFakeVersionFile(version_file, version=version,
                             chrome_branch=chrome_branch)
    return version_file

  def testLoadFromFile(self):
    """Tests whether we can load from a version file."""
    version_file = self.CreateFakeVersionFile(self.tempdir)
    info = manifest_version.VersionInfo(version_file=version_file)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testLoadFromRepo(self):
    """Tests whether we can load from a source repo."""
    version_file = os.path.join(self.tempdir, constants.VERSION_FILE)
    self.WriteFakeVersionFile(version_file)
    info = manifest_version.VersionInfo.from_repo(self.tempdir)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testLoadFromString(self):
    """Tests whether we can load from a string."""
    info = manifest_version.VersionInfo(FAKE_VERSION_STRING, CHROME_BRANCH)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def CommonTestIncrementVersion(self, incr_type, version, chrome_branch=None):
    """Common test increment.  Returns path to new incremented file."""
    message = 'Incrementing cuz I sed so'
    self.mox.StubOutWithMock(git, 'CreateBranch')
    self.mox.StubOutWithMock(manifest_version, '_PushGitChanges')
    self.mox.StubOutWithMock(git, 'CleanAndCheckoutUpstream')

    git.CreateBranch(self.tempdir, manifest_version.PUSH_BRANCH)

    version_file = self.CreateFakeVersionFile(
        self.tempdir, version=version, chrome_branch=chrome_branch)

    manifest_version._PushGitChanges(self.tempdir, message, dry_run=False,
                                     push_to=None)

    git.CleanAndCheckoutUpstream(self.tempdir)
    self.mox.ReplayAll()
    info = manifest_version.VersionInfo(version_file=version_file,
                                        incr_type=incr_type)
    info.IncrementVersion()
    info.UpdateVersionFile(message, dry_run=False)
    self.mox.VerifyAll()
    return version_file

  def testIncrementVersionPatch(self):
    """Tests whether we can increment a version file by patch number."""
    version_file = self.CommonTestIncrementVersion('branch', '1.2.3')
    new_info = manifest_version.VersionInfo(version_file=version_file,
                                            incr_type='branch')
    self.assertEqual(new_info.VersionString(), '1.2.4')

  def testIncrementVersionBranch(self):
    """Tests whether we can increment a version file by branch number."""
    version_file = self.CommonTestIncrementVersion('branch', '1.2.0')
    new_info = manifest_version.VersionInfo(version_file=version_file,
                                            incr_type='branch')
    self.assertEqual(new_info.VersionString(), '1.3.0')

  def testIncrementVersionBuild(self):
    """Tests whether we can increment a version file by build number."""
    version_file = self.CommonTestIncrementVersion('build', '1.0.0')
    new_info = manifest_version.VersionInfo(version_file=version_file,
                                            incr_type='build')
    self.assertEqual(new_info.VersionString(), '2.0.0')

  def testIncrementVersionChrome(self):
    """Tests whether we can increment the chrome version."""
    version_file = self.CommonTestIncrementVersion(
        'chrome_branch', version='1.0.0', chrome_branch='29')
    new_info = manifest_version.VersionInfo(version_file=version_file)
    self.assertEqual(new_info.VersionString(), '2.0.0')
    self.assertEqual(new_info.chrome_branch, '30')


class BuildSpecsManagerTest(cros_test_lib.MoxTempDirTestCase,
                            cros_test_lib.MockTestCase):
  """Tests for the BuildSpecs manager."""

  def setUp(self):
    os.makedirs(os.path.join(self.tempdir, '.repo'))
    self.source_repo = 'ssh://source/repo'
    self.manifest_repo = 'ssh://manifest/repo'
    self.version_file = 'version-file.sh'
    self.branch = 'master'
    self.build_names = ['x86-generic-paladin']
    self.incr_type = 'branch'

    repo = repository.RepoRepository(
        self.source_repo, self.tempdir, self.branch)
    self.manager = manifest_version.BuildSpecsManager(
        repo, self.manifest_repo, self.build_names, self.incr_type, False,
        branch=self.branch, dry_run=True)

    # Change default to something we clean up.
    self.tmpmandir = os.path.join(self.tempdir, 'man')
    osutils.SafeMakedirs(self.tmpmandir)
    self.manager.manifest_dir = self.tmpmandir
    # Shorten the sleep between attempts.
    self.manager.SLEEP_TIMEOUT = 1

  def testPublishManifestCommitMessageWithBuildId(self):
    """Tests that PublishManifest writes a build id."""
    expected_message = ('Automatic: Start x86-generic-paladin master 1\n'
                        'CrOS-Build-Id: %s' % MOCK_BUILD_ID)
    self.mox.StubOutWithMock(self.manager, 'PushSpecChanges')

    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    # Create a fake manifest file.
    m = os.path.join(self.tmpmandir, '1.xml')
    osutils.Touch(m)
    self.manager.InitializeManifestVariables(info)

    self.manager.PushSpecChanges(expected_message)

    self.mox.ReplayAll()
    self.manager.PublishManifest(m, '1', build_id=MOCK_BUILD_ID)
    self.mox.VerifyAll()

  def testPublishManifestCommitMessageWithNegativeBuildId(self):
    """Tests that PublishManifest doesn't write a negative build_id"""
    expected_message = 'Automatic: Start x86-generic-paladin master 1'
    self.mox.StubOutWithMock(self.manager, 'PushSpecChanges')

    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    # Create a fake manifest file.
    m = os.path.join(self.tmpmandir, '1.xml')
    osutils.Touch(m)
    self.manager.InitializeManifestVariables(info)

    self.manager.PushSpecChanges(expected_message)

    self.mox.ReplayAll()
    self.manager.PublishManifest(m, '1', build_id=-1)
    self.mox.VerifyAll()

  def testPublishManifestCommitMessageWithNoneBuildId(self):
    """Tests that PublishManifest doesn't write a non-existant build_id"""
    expected_message = 'Automatic: Start x86-generic-paladin master 1'
    self.mox.StubOutWithMock(self.manager, 'PushSpecChanges')

    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    # Create a fake manifest file.
    m = os.path.join(self.tmpmandir, '1.xml')
    osutils.Touch(m)
    self.manager.InitializeManifestVariables(info)

    self.manager.PushSpecChanges(expected_message)

    self.mox.ReplayAll()
    self.manager.PublishManifest(m, '1')
    self.mox.VerifyAll()

  def testLoadSpecs(self):
    """Tests whether we can load specs correctly."""
    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')
    mpath = os.path.join(self.manager.manifest_dir, 'buildspecs', CHROME_BRANCH)
    m1, m2, m3, m4 = [os.path.join(mpath, '1.2.%d.xml' % x)
                      for x in [2, 3, 4, 5]]
    for_build = os.path.join(self.manager.manifest_dir, 'build-name',
                             self.build_names[0])

    # Create fake buildspecs.
    osutils.SafeMakedirs(os.path.join(mpath))
    for m in [m1, m2, m3, m4]:
      osutils.Touch(m)

    # Fake BuilderStatus with status MISSING.
    missing = manifest_version.BuilderStatus(constants.BUILDER_STATUS_MISSING,
                                             None)

    # Fail 1, pass 2, leave 3,4 unprocessed.
    manifest_version.CreateSymlink(m1, os.path.join(
        for_build, 'fail', CHROME_BRANCH, os.path.basename(m1)))
    manifest_version.CreateSymlink(m1, os.path.join(
        for_build, 'pass', CHROME_BRANCH, os.path.basename(m2)))
    self.mox.StubOutWithMock(self.manager, 'GetBuildStatus')
    self.manager.GetBuildStatus(self.build_names[0], '1.2.5').AndReturn(missing)
    self.mox.ReplayAll()
    self.manager.InitializeManifestVariables(info)
    self.mox.VerifyAll()
    self.assertEqual(self.manager.latest_unprocessed, '1.2.5')

  def testLatestSpecFromDir(self):
    """Tests whether we can get sorted specs correctly from a directory."""
    self.mox.StubOutWithMock(repository, 'CloneGitRepo')
    info = manifest_version.VersionInfo(
        '99.1.2', CHROME_BRANCH, incr_type='branch')

    specs_dir = os.path.join(self.manager.manifest_dir, 'buildspecs',
                             CHROME_BRANCH)
    m1, m2, m3, m4 = [os.path.join(specs_dir, x)
                      for x in ['100.0.0.xml', '99.3.3.xml', '99.1.10.xml',
                                '99.1.5.xml']]

    # Create fake buildspecs.
    osutils.SafeMakedirs(specs_dir)
    for m in [m1, m2, m3, m4]:
      osutils.Touch(m)

    self.mox.ReplayAll()
    spec = self.manager._LatestSpecFromDir(info, specs_dir)
    self.mox.VerifyAll()
    # Should be the latest on the 99.1 branch.
    self.assertEqual(spec, '99.1.10')

  def testGetNextVersionNoIncrement(self):
    """Tests whether we can get the next version to be built correctly.

    Tests without pre-existing version in manifest dir.
    """
    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    self.manager.latest = None
    self.mox.ReplayAll()
    version = self.manager.GetNextVersion(info)
    self.mox.VerifyAll()
    self.assertEqual(FAKE_VERSION_STRING, version)

  def testGetNextVersionIncrement(self):
    """Tests that we create a new version if a previous one exists."""
    self.mox.StubOutWithMock(manifest_version.VersionInfo, 'UpdateVersionFile')
    version_file = VersionInfoTest.CreateFakeVersionFile(self.tempdir)
    info = manifest_version.VersionInfo(version_file=version_file,
                                        incr_type='branch')
    info.UpdateVersionFile(
        'Automatic: %s - Updating to a new version number from %s' % (
            self.build_names[0], FAKE_VERSION_STRING), dry_run=True)

    self.manager.latest = FAKE_VERSION_STRING
    self.mox.ReplayAll()
    version = self.manager.GetNextVersion(info)
    self.mox.VerifyAll()
    self.assertEqual(FAKE_VERSION_STRING_NEXT, version)

  def testGetNextBuildSpec(self):
    """End-to-end test of updating the manifest."""
    my_info = manifest_version.VersionInfo('1.2.3', chrome_branch='4')
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetCurrentVersionInfo', return_value=my_info)
    self.PatchObject(repository.RepoRepository, 'Sync')
    self.PatchObject(repository.RepoRepository, 'ExportManifest',
                     return_value='<manifest />')
    rc = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc.SetDefaultCmdResult()

    self.mox.ReplayAll()
    self.manager.GetNextBuildSpec(retries=0)
    self.manager.UpdateStatus({self.build_names[0]: True})
    self.mox.VerifyAll()

  def testUnpickleBuildStatus(self):
    """Tests that _UnpickleBuildStatus returns the correct values."""
    failed_msg = failures_lib.BuildFailureMessage(
        'you failed', ['traceback'], True, 'taco', 'bot')
    failed_input_status = manifest_version.BuilderStatus(
        constants.BUILDER_STATUS_FAILED, failed_msg)
    passed_input_status = manifest_version.BuilderStatus(
        constants.BUILDER_STATUS_PASSED, None)

    failed_output_status = self.manager._UnpickleBuildStatus(
        failed_input_status.AsPickledDict())
    passed_output_status = self.manager._UnpickleBuildStatus(
        passed_input_status.AsPickledDict())
    empty_string_status = self.manager._UnpickleBuildStatus('')

    self.assertEqual(failed_input_status.AsFlatDict(),
                     failed_output_status.AsFlatDict())
    self.assertEqual(passed_input_status.AsFlatDict(),
                     passed_output_status.AsFlatDict())
    self.assertTrue(empty_string_status.Failed())

  def _GetBuildersStatus(self, builders, status_runs):
    """Test a call to BuildSpecsManager.GetBuildersStatus.

    Args:
      builders: List of builders to get status for.
      status_runs: List of dictionaries of expected build and status.
    """
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetSlaveStatusesFromCIDB')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetBuildStatus')
    for status_dict in status_runs:
      manifest_version.BuildSpecsManager.GetSlaveStatusesFromCIDB(
          mox.IgnoreArg()).AndReturn(status_dict)

    final_status_dict = status_runs[-1]
    for builder in builders:
      status = manifest_version.BuilderStatus(
          final_status_dict.get(builder), None)
      manifest_version.BuildSpecsManager.GetBuildStatus(
          builder, mox.IgnoreArg()).AndReturn(status)

    self.mox.ReplayAll()
    statuses = self.manager.GetBuildersStatus(mox.IgnoreArg, builders)
    self.mox.VerifyAll()
    return statuses

  def testGetBuildersStatusBothFinished(self):
    """Tests GetBuilderStatus where both builds have finished."""
    status_runs = [{'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_PASSED}]
    statuses = self._GetBuildersStatus(['build1', 'build2'], status_runs)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())

  def testGetBuildersStatusLoop(self):
    """Tests GetBuilderStatus where builds are inflight."""
    status_runs = [{'build1': constants.BUILDER_STATUS_INFLIGHT,
                    'build2': constants.BUILDER_STATUS_MISSING},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_INFLIGHT},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_PASSED}]
    statuses = self._GetBuildersStatus(['build1', 'build2'], status_runs)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())


class ProjectSdkManifestTest(cros_test_lib.TestCase):
  """Test cases for ManifestVersionedSyncStage.ConvertToProjectSdkManifest."""

  def _CreateFakeManifest(self, num_internal, num_external, commits,
                          has_default_remote=False):
    """Creates a fake manifest with (optionally) some internal projects.

    Args:
      num_internal: Number of internal projects to add.
      num_external: Number of external projects to add.
      commits: Number of commits to add.
      has_default_remote: If the manifest should have a default remote.

    Returns:
      A fake manifest for use in tests.
    """
    tmp_manifest = tempfile.mktemp('manifest')
    # Create fake but empty manifest file.
    new_doc = minidom.getDOMImplementation().createDocument(None, 'manifest',
                                                            None)
    m_element = new_doc.getElementsByTagName('manifest')[0]

    default_remote = None
    if has_default_remote:
      default_remote = FAKE_WHITELISTED_REMOTES[0]
      new_element = minidom.Element('default')
      new_element.setAttribute('remote', default_remote)
      m_element.appendChild(new_element)
    remotes_to_use = list(FAKE_WHITELISTED_REMOTES) * (
        num_external / len(FAKE_WHITELISTED_REMOTES))

    internal_remotes = [FAKE_NON_WHITELISTED_REMOTE] * num_internal
    remotes_to_use.extend(internal_remotes)
    # Randomize the list of remotes to get wider test coverage for the
    # filtering logic.
    random.shuffle(remotes_to_use)

    for idx in xrange(num_internal + num_external):
      new_element = minidom.Element('project')
      new_element.setAttribute('name', 'project_%d' % idx)
      new_element.setAttribute('path', 'some_path/to/project_%d' % idx)
      new_element.setAttribute('revision', 'revision_%d' % idx)
      remote = remotes_to_use[idx % len(remotes_to_use)]
      # Skip setting a remote attribute if this is a default remote.
      if not has_default_remote or remote is not default_remote:
        new_element.setAttribute('remote', remote)
      m_element.appendChild(new_element)

    for idx in xrange(commits):
      new_element = minidom.Element('pending_commit')
      new_element.setAttribute('project', 'project_%d' % idx)
      new_element.setAttribute('change_id', 'changeid_%d' % idx)
      new_element.setAttribute('commit', 'commit_%d' % idx)
      m_element.appendChild(new_element)

    with open(tmp_manifest, 'w+') as manifest_file:
      new_doc.writexml(manifest_file, newl='\n')

    return tmp_manifest

  def testFilterProjectsFromManifest(self):
    """Tests whether we can remove internal projects from a manifest."""
    fake_manifest = None
    fake_new_manifest = None
    try:
      fake_manifest = self._CreateFakeManifest(num_internal=20,
                                               num_external=80,
                                               commits=100)
      fake_new_manifest = manifest_version.FilterManifest(
          fake_manifest, whitelisted_remotes=FAKE_WHITELISTED_REMOTES)

      new_dom = minidom.parse(fake_new_manifest)
      projects = new_dom.getElementsByTagName('project')
      # All external projects must be present in the new manifest.
      self.assertEqual(len(projects), 80)
      project_remote_dict = {}
      # All projects should have whitelisted remotes.
      for p in projects:
        remote = p.getAttribute('remote')
        self.assertIn(remote, FAKE_WHITELISTED_REMOTES)
        project_remote_dict[p.getAttribute('name')] = remote

      # Check commits. All commits should correspond to projects which
      # have whitelisted remotes.
      commits = new_dom.getElementsByTagName('pending_commit')
      self.assertEqual(len(commits), 80)
      for c in commits:
        p = c.getAttribute('project')
        self.assertIn(project_remote_dict[p], FAKE_WHITELISTED_REMOTES)

    finally:
      if fake_manifest:
        os.remove(fake_manifest)
      if fake_new_manifest:
        os.remove(fake_new_manifest)

  def testFilterProjectsFromExternalManifest(self):
    """Tests filtering on a project where no filtering is needed."""
    fake_manifest = None
    fake_new_manifest = None
    try:
      fake_manifest = self._CreateFakeManifest(num_internal=0,
                                               num_external=100,
                                               commits=20)
      fake_new_manifest = manifest_version.FilterManifest(
          fake_manifest, whitelisted_remotes=FAKE_WHITELISTED_REMOTES)

      new_dom = minidom.parse(fake_new_manifest)
      projects = new_dom.getElementsByTagName('project')
      self.assertEqual(len(projects), 100)
      commits = new_dom.getElementsByTagName('pending_commit')
      self.assertEqual(len(commits), 20)

    finally:
      if fake_manifest:
        os.remove(fake_manifest)
      if fake_new_manifest:
        os.remove(fake_new_manifest)

  def testFilterDefaultProjectsFromManifest(self):
    """Tests whether we correctly handle projects with default remotes."""
    fake_manifest = None
    fake_new_manifest = None
    try:
      fake_manifest = self._CreateFakeManifest(num_internal=20,
                                               num_external=80,
                                               commits=20,
                                               has_default_remote=True)
      fake_new_manifest = manifest_version.FilterManifest(
          fake_manifest, whitelisted_remotes=FAKE_WHITELISTED_REMOTES)

      new_dom = minidom.parse(fake_new_manifest)
      projects = new_dom.getElementsByTagName('project')
      self.assertEqual(len(projects), 80)

    finally:
      if fake_manifest:
        os.remove(fake_manifest)
      if fake_new_manifest:
        os.remove(fake_new_manifest)



  def _HelperTestConvertToProjectSdkManifestEmpty(self, source, expected):
    """Test that ConvertToProjectSdkManifest turns |source| into |expected|.

    Args:
      source: Starting manifest as a string.
      expected: Expected result manifest, as a string.
    """
    manifest_out = None

    try:      # Convert source into a file.
      with tempfile.NamedTemporaryFile() as manifest_in:
        osutils.WriteFile(manifest_in.name, source)
        manifest_out = manifest_version.ConvertToProjectSdkManifest(
            manifest_in.name)

      # Convert output file into string.
      result = osutils.ReadFile(manifest_out)

    finally:
      if manifest_out:
        os.unlink(manifest_out)

    # Verify the result.
    self.assertEqual(expected, result)

  def testConvertToProjectSdkManifestEmpty(self):
    EMPTY = '<?xml version="1.0" encoding="UTF-8"?><manifest></manifest>'
    EXPECTED = '<?xml version="1.0" encoding="utf-8"?><manifest/>'
    self._HelperTestConvertToProjectSdkManifestEmpty(EMPTY, EXPECTED)

  def testConvertToProjectSdkManifestMixed(self):
    MIXED = '''<?xml version="1.0" encoding="utf-8"?>
<manifest>
  <remote alias="cros-int" fetch="https://int-url" name="chrome"/>
  <remote alias="cros" fetch="https://url/" name="chromium"/>
  <remote fetch="https://url" name="cros"/>
  <remote fetch="https://int-url" name="cros-int"/>

  <default remote="cros" revision="refs/heads/master" sync-j="8"/>

  <project name="int" remote="cros-int" />
  <project groups="foo,bar" name="int-other-groups" remote="cros-int" />
  <project groups="minilayout" name="int-mini" remote="cros-int" />
  <project groups="minilayout,foo" name="int-mini-groups" remote="cros-int" />

  <project name="exp" remote="cros" />
  <project groups="foo,bar" name="exp-other-groups" remote="cros" />
  <project groups="minilayout" name="exp-mini" remote="cros" />
  <project groups="minilayout,foo" name="exp-mini-groups" remote="cros" />

  <project name="def-other-groups" />
  <project groups="foo,bar" name="def-other-groups" />
  <project groups="minilayout" name="def-mini" />
  <project groups="minilayout,foo" name="def-mini-groups" />
  <project groups="minilayout , foo" name="def-spacing" />

  <repo-hooks enabled-list="pre-upload" in-project="chromiumos/repohooks"/>
</manifest>
'''

    EXPECTED = '''<?xml version="1.0" encoding="utf-8"?><manifest>
  <remote alias="cros" fetch="https://url/" name="chromium"/>
  <remote fetch="https://url" name="cros"/>
  <default remote="cros" revision="refs/heads/master" sync-j="8"/>
  <project groups="minilayout" name="exp-mini" remote="cros"/>
  <project groups="minilayout,foo" name="exp-mini-groups" remote="cros"/>
  <project groups="minilayout" name="def-mini"/>
  <project groups="minilayout,foo" name="def-mini-groups"/>
  <project groups="minilayout , foo" name="def-spacing"/>
  <repo-hooks enabled-list="pre-upload" in-project="chromiumos/repohooks"/>
</manifest>'''

    self._HelperTestConvertToProjectSdkManifestEmpty(MIXED, EXPECTED)
