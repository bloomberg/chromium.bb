# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for manifest_version. Needs to be run inside of chroot."""

from __future__ import print_function

import datetime
import mock
import os
import tempfile
import time

from chromite.cbuildbot import buildbucket_lib
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import repository
from chromite.lib import constants
from chromite.lib import config_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import git
from chromite.lib import metadata_lib
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

class _BuildbucketInfos(object):
  """Helper methods to build BuildbucketInfo."""

  @staticmethod
  def GetScheduledBuild(bb_id='scheduled_id_1', retry=0):
    return manifest_version.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
        result=None
    )

  @staticmethod
  def GetStartedBuild(bb_id='started_id_1', retry=0):
    return manifest_version.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
        result=None
    )

  @staticmethod
  def GetSuccessBuild(bb_id='success_id_1', retry=0):
    return manifest_version.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
    )

  @staticmethod
  def GetFailureBuild(bb_id='failure_id_1', retry=0):
    return manifest_version.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
    )

  @staticmethod
  def GetCanceledBuild(bb_id='canceled_id_1', retry=0):
    return manifest_version.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_CANCELED
    )

  @staticmethod
  def GetMissingBuild(bb_id='missing_id_1', retry=0):
    return manifest_version.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=None,
        result=None
    )

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

  def BuildManager(self, config=None, metadata=None,
                   buildbucket_client=None):
    repo = repository.RepoRepository(
        self.source_repo, self.tempdir, self.branch)
    manager = manifest_version.BuildSpecsManager(
        repo, self.manifest_repo, self.build_names, self.incr_type, False,
        branch=self.branch, dry_run=True, config=config, metadata=metadata,
        buildbucket_client=buildbucket_client)
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

  def _CreateCQMasterManager(self, config=None, metadata=None,
                             buildbucket_client=None):
    if config is None:
      config = config_lib.BuildConfig(name='master-paladin',
                                      master=True)
    if metadata is None:
      metadata = metadata_lib.CBuildbotMetadata()

    if buildbucket_client is None:
      buildbucket_client = mock.Mock()

    return self.BuildManager(
        config=config, metadata=metadata,
        buildbucket_client=buildbucket_client)

  def testGetSlaveStatusesFromBuildbucket(self):
    """Test GetSlaveStatusesFromBuildbucket."""
    self.manager = self._CreateCQMasterManager()

    # Test completed builds.
    build_info_dict = {
        'build1': {'buildbucket_id': 'id_1', 'retry': 0},
        'build2': {'buildbucket_id': 'id_2', 'retry': 0}
    }
    self.PatchObject(buildbucket_lib, 'GetScheduledBuildDict',
                     return_value=build_info_dict)

    expected_status = 'COMPLETED'
    expected_result = 'SUCCESS'
    content = {
        'build': {
            'status': expected_status,
            'result': expected_result
        }
    }
    self.manager.buildbucket_client.GetBuildRequest.return_value = content
    updated_build_info_dict = self.manager.GetSlaveStatusesFromBuildbucket()

    self.assertEqual(updated_build_info_dict['build1'].status,
                     expected_status)
    self.assertEqual(updated_build_info_dict['build1'].result,
                     expected_result)
    self.assertEqual(updated_build_info_dict['build2'].status,
                     expected_status)
    self.assertEqual(updated_build_info_dict['build2'].result,
                     expected_result)

    # Test started builds.
    expected_status = 'STARTED'
    expected_result = None
    content = {
        'build': {
            'status': 'STARTED'
        }
    }
    self.manager.buildbucket_client.GetBuildRequest.return_value = content
    updated_build_info_dict = self.manager.GetSlaveStatusesFromBuildbucket()

    self.assertEqual(updated_build_info_dict['build1'].status,
                     expected_status)
    self.assertEqual(updated_build_info_dict['build1'].result,
                     expected_result)
    self.assertEqual(updated_build_info_dict['build2'].status,
                     expected_status)
    self.assertEqual(updated_build_info_dict['build2'].result,
                     expected_result)

    # Test BuildbucketResponseException failures.
    self.manager.buildbucket_client.GetBuildRequest.side_effect = (
        buildbucket_lib.BuildbucketResponseException)
    updated_build_info_dict = self.manager.GetSlaveStatusesFromBuildbucket()
    self.assertIsNone(updated_build_info_dict['build1'].status)
    self.assertIsNone(updated_build_info_dict['build2'].status)

  def _GetBuildersStatusWithBuildbucket(self, builders, status_runs,
                                        build_info_dicts):
    """Test a call to BuildSpecsManager.GetBuildersStatus.

    Args:
      builders: List of builders to get status for.
      status_runs: List of dictionaries of expected build and status.
      build_info_dicts: A list of dict mapping build names to
                        buildbucket_ids.
    """
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetSlaveStatusesFromCIDB',
                     side_effect=status_runs)
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetSlaveStatusesFromBuildbucket',
                     side_effect=build_info_dicts)
    self.PatchObject(buildbucket_lib,
                     'GetBuildInfoDict',
                     side_effect=build_info_dicts)

    final_status_dict = status_runs[-1]
    build_statuses = [
        manifest_version.BuilderStatus(final_status_dict.get(x), None)
        for x in builders
    ]
    self.PatchObject(manifest_version.BuildSpecsManager,
                     'GetBuildStatus',
                     side_effect=build_statuses)

    return self.manager.GetBuildersStatus(
        'builderid', builders)

  def testGetBuildersStatusBothFinishedWithBuildbucket(self):
    """Tests GetBuilderStatus where both Buildbucket builds have finished."""
    self.manager = self._CreateCQMasterManager()

    status_runs = [{'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_PASSED}]
    build_info_dicts = [{
        'build1': _BuildbucketInfos.GetFailureBuild(),
        'build2': _BuildbucketInfos.GetSuccessBuild()
    }]

    statuses = self._GetBuildersStatusWithBuildbucket(
        ['build1', 'build2'], status_runs, build_info_dicts)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())

  def testGetBuildersStatusLoopWithBuildbucket(self):
    """Tests GetBuilderStatus where both Buildbucket builds have finished."""
    self.manager = self._CreateCQMasterManager()
    retry_patch = self.PatchObject(manifest_version.BuildSpecsManager,
                                   'RetryBuilds')

    status_runs = [{'build1': constants.BUILDER_STATUS_INFLIGHT,
                    'build2': constants.BUILDER_STATUS_MISSING},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_INFLIGHT},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_PASSED}]
    build_info_dict = [{
        'build1': _BuildbucketInfos.GetStartedBuild(),
        'build2': _BuildbucketInfos.GetMissingBuild()
    }, {
        'build1': _BuildbucketInfos.GetFailureBuild(),
        'build2': _BuildbucketInfos.GetStartedBuild()
    }, {
        'build1': _BuildbucketInfos.GetFailureBuild(),
        'build2': _BuildbucketInfos.GetSuccessBuild()
    }]

    statuses = self._GetBuildersStatusWithBuildbucket(
        ['build1', 'build2'], status_runs, build_info_dict)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())
    self.assertEqual(retry_patch.call_count, 0)

  def testGetBuildersStatusLoopWithBuildbucketRetry(self):
    """Tests GetBuilderStatus loop with buildbucket retry."""
    self.manager = self._CreateCQMasterManager()
    retry_patch = self.PatchObject(manifest_version.BuildSpecsManager,
                                   'RetryBuilds')

    status_runs = [{'build1': constants.BUILDER_STATUS_INFLIGHT},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_INFLIGHT},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_PASSED}]
    build_info_dict = [{
        'build1': _BuildbucketInfos.GetStartedBuild(),
        'build2': _BuildbucketInfos.GetFailureBuild()
    }, {
        'build1': _BuildbucketInfos.GetFailureBuild(),
        'build2': _BuildbucketInfos.GetStartedBuild()
    }, {
        'build1': _BuildbucketInfos.GetFailureBuild(),
        'build2': _BuildbucketInfos.GetSuccessBuild()
    }]

    statuses = self._GetBuildersStatusWithBuildbucket(
        ['build1', 'build2'], status_runs, build_info_dict)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Passed())
    self.assertTrue(retry_patch.call_count, 1)

  def testGetBuildersStatusLoopWithBuildbucketRetry_2(self):
    """Tests GetBuilderStatus loop with buildbucket retry."""
    self.manager = self._CreateCQMasterManager()
    retry_patch = self.PatchObject(manifest_version.BuildSpecsManager,
                                   'RetryBuilds')

    status_runs = [{'build1': constants.BUILDER_STATUS_INFLIGHT},
                   {'build1': constants.BUILDER_STATUS_FAILED},
                   {'build1': constants.BUILDER_STATUS_FAILED},
                   {'build1': constants.BUILDER_STATUS_FAILED,
                    'build2': constants.BUILDER_STATUS_FAILED}]
    build_info_dict = [{
        'build1': _BuildbucketInfos.GetStartedBuild(),
        'build2': _BuildbucketInfos.GetFailureBuild()
    }, {
        'build1': _BuildbucketInfos.GetFailureBuild(),
        'build2': _BuildbucketInfos.GetFailureBuild(retry=1)
    }, {
        'build1': _BuildbucketInfos.GetFailureBuild(),
        'build2': _BuildbucketInfos.GetFailureBuild(retry=2)
    }, {
        'build1': _BuildbucketInfos.GetFailureBuild(),
        'build2': _BuildbucketInfos.GetFailureBuild(retry=2)
    }]

    statuses = self._GetBuildersStatusWithBuildbucket(
        ['build1', 'build2'], status_runs, build_info_dict)
    self.assertTrue(statuses['build1'].Failed())
    self.assertTrue(statuses['build2'].Failed())

    # build2 cannot be retried for more than retry_limit times.
    self.assertTrue(retry_patch.call_count,
                    constants.BUILDBUCKET_BUILD_RETRY_LIMIT)

  def testRetryBuilds(self):
    """Test RetryBuilds."""
    metadata = metadata_lib.CBuildbotMetadata()
    slaves = [('failure', 'id_1', time.time()),
              ('canceled', 'id_2', time.time())]
    metadata.ExtendKeyListWithList(
        constants.METADATA_SCHEDULED_SLAVES, slaves)

    self.manager = self._CreateCQMasterManager(metadata=metadata)

    builds_to_retry = set(['failure', 'canceled'])
    updated_build_info_dict = {
        'failure': manifest_version.BuildbucketInfo(
            'id', 0,
            constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
        ),
        'canceled': manifest_version.BuildbucketInfo(
            'id', 0,
            constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
            constants.BUILDBUCKET_BUILDER_RESULT_CANCELED
        )
    }
    content = {
        'build':{
            'status': 'SCHEDULED',
            'id': 'retry_id',
            'retry_of': 'id',
            'created_ts': time.time()
        }
    }
    self.manager.buildbucket_client.RetryBuildRequest.return_value = content

    self.manager.RetryBuilds(builds_to_retry, updated_build_info_dict)
    build_info_dict = buildbucket_lib.GetBuildInfoDict(self.manager.metadata)
    self.assertEqual(build_info_dict['failure']['buildbucket_id'], 'retry_id')
    self.assertEqual(build_info_dict['failure']['retry'], 1)

    self.manager.RetryBuilds(builds_to_retry, updated_build_info_dict)
    build_info_dict = buildbucket_lib.GetBuildInfoDict(self.manager.metadata)
    self.assertEqual(build_info_dict['canceled']['buildbucket_id'], 'retry_id')
    self.assertEqual(build_info_dict['canceled']['retry'], 2)

  def testShouldWait(self):
    """Test ShouldWait."""
    retry_patch = self.PatchObject(manifest_version.BuildSpecsManager,
                                   'RetryBuilds')
    self.manager = self.BuildManager()
    slave_status_mock = mock.Mock()
    slave_status_mock.ShouldWait.return_value = True
    slave_status_mock.GetBuildsToRetry.return_value = (
        set(['build_1', 'build_2']))

    self.assertTrue(self.manager.ShouldWait(slave_status_mock))
    self.assertEqual(retry_patch.call_count, 1)

    retry_patch.reset_mock()
    slave_status_mock.ShouldWait.return_value = False
    self.assertFalse(self.manager.ShouldWait(slave_status_mock))
    self.assertEqual(retry_patch.call_count, 0)

    retry_patch.reset_mock()
    slave_status_mock.ShouldWait.return_value = True
    slave_status_mock.GetBuildsToRetry.return_value = None
    self.assertTrue(self.manager.ShouldWait(slave_status_mock))
    self.assertEqual(retry_patch.call_count, 0)

class SlaveStatusTest(cros_test_lib.TestCase):
  """Test methods testing methods in SalveStatus class."""

  def testGetMissing(self):
    """Tests GetMissing returns the missing builders."""
    status = {'build1': constants.BUILDER_STATUS_FAILED,
              'build2': constants.BUILDER_STATUS_INFLIGHT}
    builders_array = ['build1', 'build2', 'missing_builder']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set())

    self.assertEqual(slaveStatus.GetMissing(), set(['missing_builder']))

  def testGetMissingWithBuildbucket(self):
    """Tests GetMissing returns the missing builders with Buildbucket."""
    status = {'started':  constants.BUILDER_STATUS_INFLIGHT}
    build_info_dict = {
        'scheduled': _BuildbucketInfos.GetScheduledBuild(),
        'started': _BuildbucketInfos.GetStartedBuild(),
        'missing': _BuildbucketInfos.GetMissingBuild()}
    builders_array = ['scheduled', 'started', 'missing']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set(),
                                               build_info_dict)

    self.assertEqual(slaveStatus.GetMissing(), set(['missing']))

  def testGetMissingNone(self):
    """Tests GetMissing returns nothing when all builders are accounted for."""
    status = {'build1': constants.BUILDER_STATUS_FAILED,
              'build2': constants.BUILDER_STATUS_INFLIGHT}
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set())

    self.assertEqual(slaveStatus.GetMissing(), set())

  def testGetMissingNoneWithBuildbucket(self):
    """Tests GetMissing returns nothing with Buildbucket."""
    status = {'started': constants.BUILDER_STATUS_INFLIGHT}
    build_info_dict = {
        'scheduled': _BuildbucketInfos.GetScheduledBuild(),
        'started': _BuildbucketInfos.GetStartedBuild()
    }
    builders_array = ['build1', 'build2']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set(),
                                               build_info_dict)

    self.assertEqual(slaveStatus.GetMissing(), set())

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

  def GetRetriableBuilds(self):
    """Test GetRetriableBuilds."""

    # No build to retry.
    status = {'started': constants.BUILDER_STATUS_INFLIGHT,
              'completed_success': constants.BUILDER_STATUS_PASSED,
              'completed_failure': constants.BUILDER_STATUS_FAILED,
              'completed_canceled': constants.BUILDER_STATUS_INFLIGHT}
    build_info_dict = {
        'scheduled': _BuildbucketInfos.GetScheduledBuild(),
        'started': _BuildbucketInfos.GetStartedBuild(),
        'completed_success': _BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': _BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': _BuildbucketInfos.GetCanceledBuild()}
    completed_all = set(['completed_success',
                         'completed_failure',
                         'completed_canceled'])
    builders_array = ['scheduled', 'started', 'completed_success',
                      'completed_failure', 'completed_canceled']
    slaveStatus = manifest_version.SlaveStatus(status,
                                               datetime.datetime.now(),
                                               builders_array,
                                               set(),
                                               build_info_dict)
    retry_builds = slaveStatus.GetRetriableBuilds(completed_all)
    self.assertEqual(retry_builds, set())

    # Retry failed and canceled builds.
    status = {'started': constants.BUILDER_STATUS_INFLIGHT,
              'completed_success': constants.BUILDER_STATUS_PASSED}
    slaveStatus = manifest_version.SlaveStatus(status,
                                               datetime.datetime.now(),
                                               builders_array,
                                               set(),
                                               build_info_dict)
    expected_retry = set(['completed_failure',
                          'completed_canceled'])
    retry_builds = slaveStatus.GetRetriableBuilds(completed_all)
    self.assertEqual(retry_builds, expected_retry)

    # Don't retry builds which have exceeded the retry_limit.
    build_info_dict = {
        'scheduled': _BuildbucketInfos.GetScheduledBuild(),
        'started': _BuildbucketInfos.GetStartedBuild(),
        'completed_success': _BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': _BuildbucketInfos.GetFailureBuild(
            retry=constants.BUILDBUCKET_BUILD_RETRY_LIMIT),
        'completed_canceled': _BuildbucketInfos.GetCanceledBuild()}
    slaveStatus = manifest_version.SlaveStatus(status,
                                               datetime.datetime.now(),
                                               builders_array,
                                               set(),
                                               build_info_dict)
    expected_retry = set(['completed_canceled'])
    retry_builds = slaveStatus.GetRetriableBuilds(completed_all)
    self.assertEqual(retry_builds, expected_retry)

  def testGetCompletedWithBuildbucket(self):
    """Tests GetCompleted with Buildbucket"""
    status = {'started': constants.BUILDER_STATUS_INFLIGHT,
              'completed_success': constants.BUILDER_STATUS_PASSED,
              'completed_failure': constants.BUILDER_STATUS_FAILED,
              'completed_canceled': constants.BUILDER_STATUS_INFLIGHT}
    build_info_dict = {
        'scheduled': _BuildbucketInfos.GetScheduledBuild(),
        'started': _BuildbucketInfos.GetStartedBuild(),
        'completed_success': _BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': _BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': _BuildbucketInfos.GetCanceledBuild()}
    builders_array = ['scheduled', 'started', 'completed_success',
                      'completed_failure', 'completed_canceled']
    previous_completed = set(['completed_success'])
    expected_completed = set(['completed_success', 'completed_failure',
                              'completed_canceled'])
    slaveStatus = manifest_version.SlaveStatus(status,
                                               datetime.datetime.now(),
                                               builders_array,
                                               previous_completed,
                                               build_info_dict)

    self.assertEqual(slaveStatus.GetCompleted(), expected_completed)
    self.assertEqual(slaveStatus.previous_completed, expected_completed)

  def testGetBuildsToRetry(self):
    """Test GetBuildsToRetry."""
    status = {'started': constants.BUILDER_STATUS_INFLIGHT,
              'completed_success': constants.BUILDER_STATUS_PASSED,
              'completed_canceled': constants.BUILDER_STATUS_INFLIGHT}
    builders_array = ['scheduled', 'started', 'completed_success',
                      'completed_failure', 'completed_canceled']

    expected_completed = set(['completed_success'])
    slaveStatus = manifest_version.SlaveStatus(status,
                                               datetime.datetime.now(),
                                               builders_array,
                                               set(['completed_success']))
    self.assertEqual(slaveStatus.GetCompleted(), expected_completed)
    self.assertEqual(slaveStatus.previous_completed, expected_completed)
    self.assertEqual(slaveStatus.GetBuildsToRetry(), None)

  def testGetBuildsToRetryWithBuildbucket(self):
    """Test GetBuildsToRetry with Buildbucket."""
    status = {'started': constants.BUILDER_STATUS_INFLIGHT,
              'completed_success': constants.BUILDER_STATUS_PASSED,
              'completed_canceled': constants.BUILDER_STATUS_INFLIGHT}
    build_info_dict = {
        'scheduled': _BuildbucketInfos.GetScheduledBuild(),
        'started': _BuildbucketInfos.GetStartedBuild(),
        'completed_success': _BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': _BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': _BuildbucketInfos.GetCanceledBuild()}
    builders_array = ['scheduled', 'started', 'completed_success',
                      'completed_failure', 'completed_canceled']
    expected_completed = set(['completed_success',
                              'completed_canceled'])
    expected_retry_builds = set(['completed_failure'])
    slaveStatus = manifest_version.SlaveStatus(status,
                                               datetime.datetime.now(),
                                               builders_array,
                                               set(['completed_success']),
                                               build_info_dict)

    self.assertEqual(slaveStatus.GetCompleted(), expected_completed)
    self.assertEqual(slaveStatus.previous_completed, expected_completed)
    self.assertEqual(slaveStatus.GetBuildsToRetry(), expected_retry_builds)

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
    status_not_completed = {
        'started': constants.BUILDER_STATUS_INFLIGHT,
        'failure': constants.BUILDER_STATUS_FAILED}
    build_info_dict_not_completed = {
        'started': _BuildbucketInfos.GetStartedBuild(),
        'failure': _BuildbucketInfos.GetFailureBuild(),
        'missing': _BuildbucketInfos.GetMissingBuild()}
    builders_array = ['started', 'failure', 'missing']
    slaveStatusNotCompleted = manifest_version.SlaveStatus(
        status_not_completed, datetime.datetime.now(), builders_array, set(),
        build_info_dict_not_completed)

    self.assertFalse(slaveStatusNotCompleted.Completed())

    status_completed = {
        'success': constants.BUILDER_STATUS_PASSED,
        'failure': constants.BUILDER_STATUS_FAILED}
    build_info_dict_complted = {
        'success': _BuildbucketInfos.GetSuccessBuild(),
        'failure': _BuildbucketInfos.GetFailureBuild()}
    builders_array = ['success', 'failure']
    slaveStatusCompleted = manifest_version.SlaveStatus(
        status_completed, datetime.datetime.now(), builders_array, set(),
        build_info_dict_complted)

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
    status = {'success': constants.BUILDER_STATUS_PASSED}

    build_info_dict = {
        'success': _BuildbucketInfos.GetSuccessBuild(),
        'scheduled': _BuildbucketInfos.GetScheduledBuild()
    }

    start_time = datetime.datetime.now()
    builders_array = ['success', 'scheduled']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               build_info_dict)
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
    status = {'success': constants.BUILDER_STATUS_PASSED}
    build_info_dict = {
        'success': _BuildbucketInfos.GetSuccessBuild(),
        'missing': _BuildbucketInfos.GetMissingBuild()}

    start_time = datetime.datetime.now()
    builders_array = ['success', 'missing']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               build_info_dict)

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
    status = {'started': constants.BUILDER_STATUS_INFLIGHT}
    build_info_dict = {
        'started': _BuildbucketInfos.GetStartedBuild(),
        'missing': _BuildbucketInfos.GetMissingBuild()}

    start_time = datetime.datetime.now()
    builders_array = ['started', 'missing']
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               build_info_dict)
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
    status = {'failure': constants.BUILDER_STATUS_FAILED,
              'success': constants.BUILDER_STATUS_PASSED}
    build_info_dict = {
        'failure': _BuildbucketInfos.GetFailureBuild(),
        'success': _BuildbucketInfos.GetSuccessBuild()}
    builders_array = ['failure', 'success']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set(),
                                               build_info_dict)

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
    status = {'failure': constants.BUILDER_STATUS_FAILED}
    build_info_dict = {
        'failure': _BuildbucketInfos.GetFailureBuild(),
        'scheduled': _BuildbucketInfos.GetScheduledBuild()}
    builders_array = ['failure', 'scheduled']
    start_time = datetime.datetime.now() - datetime.timedelta(hours=1)
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               build_info_dict)

    self.assertFalse(slaveStatus.ShouldWait())

  def testShouldWaitNoScheduledBuilderWithBuildbucket(self):
    """ShouldWait says no because all scheduled builds are completed."""
    status = {'failure': constants.BUILDER_STATUS_FAILED,
              'success': constants.BUILDER_STATUS_PASSED}
    build_info_dict = {
        'failure': _BuildbucketInfos.GetFailureBuild(),
        'success': _BuildbucketInfos.GetSuccessBuild()}
    builders_array = ['build1', 'build2', 'missing_builder']
    start_time = datetime.datetime.now() - datetime.timedelta(hours=1)
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               build_info_dict)

    self.assertFalse(slaveStatus.ShouldWait())

  def testShouldWaitMissingBuilderWithBuildbucket(self):
    """ShouldWait says yes waiting because one build status is missing."""
    status = {'failure': constants.BUILDER_STATUS_FAILED}
    build_info_dict = {
        'failure': _BuildbucketInfos.GetFailureBuild(),
        'missing': _BuildbucketInfos.GetMissingBuild()
    }
    builders_array = ['failure', 'missing']
    start_time = datetime.datetime.now() - datetime.timedelta(hours=1)
    slaveStatus = manifest_version.SlaveStatus(status, start_time,
                                               builders_array, set(),
                                               build_info_dict)

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
    status = {'started': constants.BUILDER_STATUS_INFLIGHT,
              'failure': constants.BUILDER_STATUS_FAILED}
    build_info_dict = {
        'started': _BuildbucketInfos.GetStartedBuild(),
        'failure': _BuildbucketInfos.GetFailureBuild()
    }
    builders_array = ['started', 'failure']
    slaveStatus = manifest_version.SlaveStatus(status, datetime.datetime.now(),
                                               builders_array, set(),
                                               build_info_dict)

    self.assertTrue(slaveStatus.ShouldWait())
