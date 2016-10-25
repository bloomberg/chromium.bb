# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for manifest_version. Needs to be run inside of chroot."""

from __future__ import print_function

import datetime
import mock
import os
import tempfile

from chromite.cbuildbot import buildbucket_lib
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import repository
from chromite.lib import constants
from chromite.lib import failures_lib
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


class VersionInfoTest(cros_test_lib.MockTempDirTestCase):
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
    create_mock = self.PatchObject(git, 'CreateBranch')
    push_mock = self.PatchObject(manifest_version, '_PushGitChanges')
    clean_mock = self.PatchObject(git, 'CleanAndCheckoutUpstream')

    version_file = self.CreateFakeVersionFile(
        self.tempdir, version=version, chrome_branch=chrome_branch)
    info = manifest_version.VersionInfo(version_file=version_file,
                                        incr_type=incr_type)
    info.IncrementVersion()
    info.UpdateVersionFile(message, dry_run=False)

    create_mock.assert_called_once_with(
        self.tempdir, manifest_version.PUSH_BRANCH)
    push_mock.assert_called_once_with(
        self.tempdir, message, dry_run=False, push_to=None)
    clean_mock.assert_called_once_with(self.tempdir)

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


class BuildSpecsManagerTest(cros_test_lib.MockTempDirTestCase):
  """Tests for the BuildSpecs manager."""

  def setUp(self):
    os.makedirs(os.path.join(self.tempdir, '.repo'))
    self.source_repo = 'ssh://source/repo'
    self.manifest_repo = 'ssh://manifest/repo'
    self.version_file = 'version-file.sh'
    self.branch = 'master'
    self.build_names = ['x86-generic-paladin']
    self.incr_type = 'branch'
    # Change default to something we clean up.
    self.tmpmandir = os.path.join(self.tempdir, 'man')
    osutils.SafeMakedirs(self.tmpmandir)
    self.manager = None

  def BuildManager(self, buildbucket_client=None):
    repo = repository.RepoRepository(
        self.source_repo, self.tempdir, self.branch)
    manager = manifest_version.BuildSpecsManager(
        repo, self.manifest_repo, self.build_names, self.incr_type, False,
        branch=self.branch, dry_run=True,
        testjob=False, buildbucket_client=buildbucket_client)
    manager.manifest_dir = self.tmpmandir
    # Shorten the sleep between attempts.
    manager.SLEEP_TIMEOUT = 1

    return manager

  def testPublishManifestCommitMessageWithBuildId(self):
    """Tests that PublishManifest writes a build id."""
    self.manager = self.BuildManager()
    expected_message = ('Automatic: Start x86-generic-paladin master 1\n'
                        'CrOS-Build-Id: %s' % MOCK_BUILD_ID)
    push_mock = self.PatchObject(self.manager, 'PushSpecChanges')

    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    # Create a fake manifest file.
    m = os.path.join(self.tmpmandir, '1.xml')
    osutils.Touch(m)
    self.manager.InitializeManifestVariables(info)

    self.manager.PublishManifest(m, '1', build_id=MOCK_BUILD_ID)

    push_mock.assert_called_once_with(expected_message)

  def testPublishManifestCommitMessageWithNegativeBuildId(self):
    """Tests that PublishManifest doesn't write a negative build_id"""
    self.manager = self.BuildManager()
    expected_message = 'Automatic: Start x86-generic-paladin master 1'
    push_mock = self.PatchObject(self.manager, 'PushSpecChanges')

    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    # Create a fake manifest file.
    m = os.path.join(self.tmpmandir, '1.xml')
    osutils.Touch(m)
    self.manager.InitializeManifestVariables(info)

    self.manager.PublishManifest(m, '1', build_id=-1)

    push_mock.assert_called_once_with(expected_message)

  def testPublishManifestCommitMessageWithNoneBuildId(self):
    """Tests that PublishManifest doesn't write a non-existant build_id"""
    self.manager = self.BuildManager()
    expected_message = 'Automatic: Start x86-generic-paladin master 1'
    push_mock = self.PatchObject(self.manager, 'PushSpecChanges')

    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    # Create a fake manifest file.
    m = os.path.join(self.tmpmandir, '1.xml')
    osutils.Touch(m)
    self.manager.InitializeManifestVariables(info)

    self.manager.PublishManifest(m, '1')

    push_mock.assert_called_once_with(expected_message)

  def testLoadSpecs(self):
    """Tests whether we can load specs correctly."""
    self.manager = self.BuildManager()
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
    m = self.PatchObject(self.manager, 'GetBuildStatus', return_value=missing)
    self.manager.InitializeManifestVariables(info)
    self.assertEqual(self.manager.latest_unprocessed, '1.2.5')
    m.assert_called_once_with(self.build_names[0], '1.2.5')

  def testLatestSpecFromDir(self):
    """Tests whether we can get sorted specs correctly from a directory."""
    self.manager = self.BuildManager()
    self.PatchObject(repository, 'CloneGitRepo', side_effect=Exception())
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

    spec = self.manager._LatestSpecFromDir(info, specs_dir)
    # Should be the latest on the 99.1 branch.
    self.assertEqual(spec, '99.1.10')

  def testGetNextVersionNoIncrement(self):
    """Tests whether we can get the next version to be built correctly.

    Tests without pre-existing version in manifest dir.
    """
    self.manager = self.BuildManager()
    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    self.manager.latest = None
    version = self.manager.GetNextVersion(info)
    self.assertEqual(FAKE_VERSION_STRING, version)

  def testGetNextVersionIncrement(self):
    """Tests that we create a new version if a previous one exists."""
    self.manager = self.BuildManager()
    m = self.PatchObject(manifest_version.VersionInfo, 'UpdateVersionFile')
    version_file = VersionInfoTest.CreateFakeVersionFile(self.tempdir)
    info = manifest_version.VersionInfo(version_file=version_file,
                                        incr_type='branch')

    self.manager.latest = FAKE_VERSION_STRING
    version = self.manager.GetNextVersion(info)
    self.assertEqual(FAKE_VERSION_STRING_NEXT, version)
    m.assert_called_once_with(
        'Automatic: %s - Updating to a new version number from %s' % (
            self.build_names[0], FAKE_VERSION_STRING), dry_run=True)

  def testGetNextBuildSpec(self):
    """End-to-end test of updating the manifest."""
    self.manager = self.BuildManager()
    my_info = manifest_version.VersionInfo('1.2.3', chrome_branch='4')
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetCurrentVersionInfo', return_value=my_info)
    self.PatchObject(repository.RepoRepository, 'Sync')
    self.PatchObject(repository.RepoRepository, 'ExportManifest',
                     return_value='<manifest />')
    rc = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc.SetDefaultCmdResult()

    self.manager.GetNextBuildSpec(retries=0)
    self.manager.UpdateStatus({self.build_names[0]: True})

  def testUnpickleBuildStatus(self):
    """Tests that _UnpickleBuildStatus returns the correct values."""
    self.manager = self.BuildManager()
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
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetSlaveStatusesFromCIDB', side_effect=status_runs)

    final_status_dict = status_runs[-1]
    build_statuses = [
        manifest_version.BuilderStatus(final_status_dict.get(x), None)
        for x in builders
    ]
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetBuildStatus',
                     side_effect=build_statuses)

    return self.manager.GetBuildersStatus('builderid', builders)

  def testGetBuildersStatusBothFinished(self):
    """Tests GetBuilderStatus where both builds have finished."""
    self.manager = self.BuildManager()
    status_runs = [{'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_PASSED}]
    statuses = self._GetBuildersStatus(['build1', 'build2'], status_runs)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())

  def testGetBuildersStatusLoop(self):
    """Tests GetBuilderStatus where builds are inflight."""
    self.manager = self.BuildManager()
    status_runs = [{'build1': constants.BUILDER_STATUS_INFLIGHT,
                    'build2': constants.BUILDER_STATUS_MISSING},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_INFLIGHT},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_PASSED}]
    statuses = self._GetBuildersStatus(['build1', 'build2'], status_runs)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())

  def testGetSlaveStatusesFromBuildbucket(self):
    """Test GetSlaveStatusesFromBuildbucket"""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2'
    }

    buildbucket_client_mock = mock.Mock()
    self.manager = self.BuildManager(
        buildbucket_client=buildbucket_client_mock)

    # Test completed builds.
    build_status = expected_status = {
        'status': 'COMPLETED',
        'result': 'SUCCESS'
    }
    content = {
        'build':build_status
    }
    buildbucket_client_mock.GetBuildRequest.return_value = content
    status_dict = self.manager.GetSlaveStatusesFromBuildbucket(
        buildbucket_id_dict)
    self.assertEqual(status_dict['build1'], expected_status)
    self.assertEqual(status_dict['build2'], expected_status)

    # Test started builds.
    build_status = {
        'status': 'STARTED',
    }
    expected_status = {
        'status': 'STARTED',
        'result': None
    }
    content = {
        'build':build_status
    }
    buildbucket_client_mock.GetBuildRequest.return_value = content
    status_dict = self.manager.GetSlaveStatusesFromBuildbucket(
        buildbucket_id_dict)
    self.assertEqual(status_dict['build1'], expected_status)
    self.assertEqual(status_dict['build2'], expected_status)

    # Test BuildbucketResponseException failures.
    buildbucket_client_mock.GetBuildRequest.side_effect = (
        buildbucket_lib.BuildbucketResponseException)
    status_dict = self.manager.GetSlaveStatusesFromBuildbucket(
        buildbucket_id_dict)
    self.assertIsNone(status_dict.get('build1'))
    self.assertIsNone(status_dict.get('build2'))

  def _GetBuildersStatusWithBuildbucket(self, builders, status_runs,
                                        buildbucket_id_dict):
    """Test a call to BuildSpecsManager.GetBuildersStatus.

    Args:
      builders: List of builders to get status for.
      status_runs: List of dictionaries of expected build and status.
      buildbucket_id_dict: A dict mapping build names to buildbucket_ids.
    """
    result_dict = {'SUCCESS': 'pass', 'FAILURE': 'fail'}
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetSlaveStatusesFromBuildbucket',
                     side_effect=status_runs)

    final_status_dict = status_runs[-1]
    build_statuses = [
        manifest_version.BuilderStatus(
            result_dict.get(final_status_dict.get(x).get('result')), None)
        for x in builders
    ]
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetBuildStatus',
                     side_effect=build_statuses)

    return self.manager.GetBuildersStatus(
        'builderid', builders, buildbucket_id_dict=buildbucket_id_dict)

  def testGetBuildersStatusBothFinishedWithBuildbucket(self):
    """Tests GetBuilderStatus where both Buildbucket builds have finished."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2'
    }
    buildbucket_client_mock = mock.Mock()
    self.manager = self.BuildManager(buildbucket_client=buildbucket_client_mock)
    status_runs = [{
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        }
    }]
    statuses = self._GetBuildersStatusWithBuildbucket(
        ['build1', 'build2'], status_runs, buildbucket_id_dict)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())

class SlaveStatusTest(cros_test_lib.TestCase):
  """Test methods testing methods in SalveStatus class."""

  def testGetMissing(self):
    """Tests GetMissing returns the missing builders."""
    status = {'build1': constants.BUILDER_STATUS_FAILED,
              'build2': constants.BUILDER_STATUS_INFLIGHT}
    builders_array = ['build1', 'build2', 'missing_builder']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set())

    self.assertEqual(slaveStatus.GetMissing(), ['missing_builder'])

  def testGetMissingWithBuildbucket(self):
    """Tests GetMissing returns the missing builders with Buildbucket."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2'
    }
    status = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
            'result': None
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
            'result': None
        }
    }
    builders_array = ['build1', 'build2', 'missing_builder']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set(),
                                               buildbucket_id_dict)

    self.assertEqual(slaveStatus.GetMissing(), ['missing_builder'])

  def testGetMissingNone(self):
    """Tests GetMissing returns nothing when all builders are accounted for."""
    status = {'build1': constants.BUILDER_STATUS_FAILED,
              'build2': constants.BUILDER_STATUS_INFLIGHT}
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set())

    self.assertEqual(slaveStatus.GetMissing(), [])

  def testGetMissingNoneWithBuildbucket(self):
    """Tests GetMissing returns nothing with Buildbucket."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2'
    }
    status = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
            'result': None
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
            'result': None
        }
    }
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set(),
                                               buildbucket_id_dict)

    self.assertEqual(slaveStatus.GetMissing(), [])

  def testGetCompleted(self):
    """Tests GetCompleted returns the right builders that have completed."""
    status = {'passed': constants.BUILDER_STATUS_PASSED,
              'failed': constants.BUILDER_STATUS_FAILED,
              'aborted': constants.BUILDER_STATUS_ABORTED,
              'skipped': constants.BUILDER_STATUS_SKIPPED,
              'forgiven': constants.BUILDER_STATUS_FORGIVEN,
              'inflight': constants.BUILDER_STATUS_INFLIGHT,
              'missing': constants.BUILDER_STATUS_MISSING,
              'planned': constants.BUILDER_STATUS_PLANNED}
    builders_array = ['passed', 'failed', 'aborted', 'skipped', 'forgiven',
                      'inflight', 'missing', 'planning']
    previous_completed = set(['passed'])
    expected_completed = set(['passed', 'failed', 'aborted', 'skipped',
                              'forgiven'])
    slaveStatus = manifest_version.SlaveStatus(status,
                                               datetime.datetime.now(),
                                               builders_array,
                                               previous_completed)

    self.assertEqual(slaveStatus.GetCompleted(), expected_completed)
    self.assertEqual(slaveStatus.previous_completed, expected_completed)

  def testGetCompletedWithBuildbucket(self):
    """Tests GetCompleted with Buildbucket"""
    buildbucket_id_dict = {
        'scheduled': 'id_1',
        'started': 'id_2',
        'completed_success': 'id_3',
        'completed_failure': 'id_4',
        'completed_canceled': 'id_5'
    }
    status = {
        'scheduled': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
            'result': None
        },
        'started': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
            'result': None
        },
        'completed_success': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        },
        'completed_failure': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
        },
        'completed_canceled': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_CANCELED
        }
    }
    builders_array = ['scheduled', 'started', 'completed_success',
                      'completed_failure', 'completed_canceled']
    previous_completed = set(['completed_success'])
    expected_completed = set(['completed_success', 'completed_failure',
                              'completed_canceled'])
    slaveStatus = manifest_version.SlaveStatus(status,
                                               datetime.datetime.now(),
                                               builders_array,
                                               previous_completed,
                                               buildbucket_id_dict)

    self.assertEqual(slaveStatus.GetCompleted(), expected_completed)
    self.assertEqual(slaveStatus.previous_completed, expected_completed)

  def testCompleted(self):
    """Tests Completed returns proper bool."""
    statusNotCompleted = {'build1': constants.BUILDER_STATUS_FAILED,
                          'build2': constants.BUILDER_STATUS_INFLIGHT}
    statusCompleted = {'build1': constants.BUILDER_STATUS_FAILED,
                       'build2': constants.BUILDER_STATUS_PASSED}
    builders_array = ['build1', 'build2']
    slaveStatusNotCompleted = manifest_version.SlaveStatus(
        statusNotCompleted, datetime.datetime.now(), builders_array, set())
    slaveStatusCompleted = manifest_version.SlaveStatus(
        statusCompleted, datetime.datetime.now(), builders_array, set())

    self.assertFalse(slaveStatusNotCompleted.Completed())
    self.assertTrue(slaveStatusCompleted.Completed())

  def testCompletedWithBuildbucket(self):
    """Tests Completed returns proper bool with Buildbucket."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2'
    }
    statusNotCompleted = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
            'result': None
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        }
    }
    statusCompleted = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
        }
    }
    builders_array = ['build1', 'build2', 'not_scheduled_build']
    slaveStatusNotCompleted = manifest_version.SlaveStatus(
        statusNotCompleted, datetime.datetime.now(), builders_array, set(),
        buildbucket_id_dict)
    slaveStatusCompleted = manifest_version.SlaveStatus(
        statusCompleted, datetime.datetime.now(), builders_array, set(),
        buildbucket_id_dict)

    self.assertFalse(slaveStatusNotCompleted.Completed())
    self.assertTrue(slaveStatusCompleted.Completed())

  def testShouldFailForBuilderStartTimeoutTrue(self):
    """Tests that ShouldFailForBuilderStartTimeout says fail when it should."""
    status = {'build1': constants.BUILDER_STATUS_FAILED}
    start_time = datetime.datetime.now()
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set())
    check_time = start_time + datetime.timedelta(
        minutes=slaveStatus.BUILDER_START_TIMEOUT + 1)

    self.assertTrue(slaveStatus.ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForBuilderStartTimeoutTrueWithBuildbucket(self):
    """Tests that ShouldFailForBuilderStartTimeout says fail when it should."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2',
    }
    status = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
            'result': None
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        }
    }

    start_time = datetime.datetime.now()
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               buildbucket_id_dict)
    check_time = start_time + datetime.timedelta(
        minutes=slaveStatus.BUILDER_START_TIMEOUT + 1)

    self.assertTrue(slaveStatus.ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForBuilderStartTimeoutFalseTooEarly(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    Make sure that we don't fail if there are missing builders but we're
    checking before the timeout and the other builders have completed.
    """
    status = {'build1': constants.BUILDER_STATUS_FAILED}
    start_time = datetime.datetime.now()
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set())

    self.assertFalse(slaveStatus.ShouldFailForBuilderStartTimeout(start_time))

  def testShouldFailForBuilderStartTimeoutFalseTooEarlyWithBuildbucket(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    With Buildbucket, make sure that we don't fail if there are missing
    builders but we're checking before the timeout and the other builders
    have completed.
    """
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2',
    }
    status = {
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        }
    }

    start_time = datetime.datetime.now()
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               buildbucket_id_dict)

    self.assertFalse(slaveStatus.ShouldFailForBuilderStartTimeout(start_time))

  def testShouldFailForBuilderStartTimeoutFalseNotCompleted(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    Make sure that we don't fail if there are missing builders and we're
    checking after the timeout but the other builders haven't completed.
    """
    status = {'build1': constants.BUILDER_STATUS_INFLIGHT}
    start_time = datetime.datetime.now()
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set())
    check_time = start_time + datetime.timedelta(
        minutes=slaveStatus.BUILDER_START_TIMEOUT + 1)

    self.assertFalse(slaveStatus.ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForStartTimeoutFalseNotCompletedWithBuildbucket(self):
    """Tests that ShouldWait doesn't fail with Buildbucket.

    With Buildbucket, make sure that we don't fail if there are missing builders
    and we're checking after the timeout but the other builders haven't
    completed.
    """
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2',
    }
    status = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
            'result': None
        },
    }
    start_time = datetime.datetime.now()
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               buildbucket_id_dict)
    check_time = start_time + datetime.timedelta(
        minutes=slaveStatus.BUILDER_START_TIMEOUT + 1)

    self.assertFalse(slaveStatus.ShouldFailForBuilderStartTimeout(check_time))

  def testShouldWaitAllBuildersCompleted(self):
    """Tests that ShouldWait says no waiting because all builders finished."""
    status = {'build1': constants.BUILDER_STATUS_FAILED,
              'build2': constants.BUILDER_STATUS_PASSED}
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set())

    self.assertFalse(slaveStatus.ShouldWait())

  def testShouldWaitAllBuildersCompletedWithBuildbucket(self):
    """ShouldWait says no because all builders finished with Buildbucket."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2',
    }
    status = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        }
    }
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set(),
                                               buildbucket_id_dict)

    self.assertFalse(slaveStatus.ShouldWait())

  def testShouldWaitMissingBuilder(self):
    """Tests that ShouldWait says no waiting because a builder is missing."""
    status = {'build1': constants.BUILDER_STATUS_FAILED}
    builders_array = ['build1', 'build2']
    start_time = datetime.datetime.now() - datetime.timedelta(hours=1)
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set())

    self.assertFalse(slaveStatus.ShouldWait())

  def testShouldWaitScheduledBuilderWithBuildbucket(self):
    """ShouldWait says no because a builder is in scheduled with Buildbucket."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2',
    }
    status = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
            'result': None
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
        }
    }
    builders_array = ['build1', 'build2']
    start_time = datetime.datetime.now() - datetime.timedelta(hours=1)
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               buildbucket_id_dict)

    self.assertFalse(slaveStatus.ShouldWait())

  def testShouldWaitNoScheduledBuilderWithBuildbucket(self):
    """ShouldWait says no because all scheduled builds are completed."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2',
    }
    status = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
        }
    }
    builders_array = ['build1', 'build2', 'missing_builder']
    start_time = datetime.datetime.now() - datetime.timedelta(hours=1)
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               buildbucket_id_dict)

    self.assertFalse(slaveStatus.ShouldWait())

  def testShouldWaitMissingBuilderWithBuildbucket(self):
    """ShouldWait says yes waiting because one build status is missing."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2',
    }
    status = {
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
        }
    }
    builders_array = ['build1', 'build2']
    start_time = datetime.datetime.now() - datetime.timedelta(hours=1)
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               buildbucket_id_dict)

    self.assertTrue(slaveStatus.ShouldWait())


  def testShouldWaitBuildersStillBuilding(self):
    """Tests that ShouldWait says to wait because builders still building."""
    status = {'build1': constants.BUILDER_STATUS_INFLIGHT,
              'build2': constants.BUILDER_STATUS_FAILED}
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set())

    self.assertTrue(slaveStatus.ShouldWait())

  def testShouldWaitBuildersStillBuildingWithBuildbucket(self):
    """ShouldWait says yes because builders still in started status."""
    buildbucket_id_dict = {
        'build1': 'id_1',
        'build2': 'id_2',
    }
    status = {
        'build1': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
            'result': None
        },
        'build2': {
            'status': constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            'result': constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
        }
    }
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set(),
                                               buildbucket_id_dict)

    self.assertTrue(slaveStatus.ShouldWait())
