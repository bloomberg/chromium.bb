# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for vm_test_stages."""

from __future__ import print_function

import os

from chromite.cbuildbot import cbuildbot_unittest
from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import vm_test_stages
from chromite.lib import cgroups
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import osutils


# pylint: disable=too-many-ancestors

class GCETestStageTest(generic_stages_unittest.AbstractStageTestCase,
                       cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests for the GCETest stage."""

  BOT_ID = 'amd64-generic-full'
  RELEASE_TAG = ''

  def setUp(self):
    for cmd in ('CreateTestRoot', 'GenerateStackTraces', 'ArchiveFile',
                'UploadArchivedFile', 'BuildAndArchiveTestResultsTarball'):
      self.PatchObject(commands, cmd, autospec=True)
    for cmd in ('RunTestSuite', 'ArchiveTestResults', 'ArchiveVMFiles',
                'RunDevModeTest', 'RunCrosVMTest',
                'ListTests', 'GetTestResultsDir',):
      self.PatchObject(vm_test_stages, cmd, autospec=True)
    self.PatchObject(vm_test_stages.VMTestStage, '_NoTestResults',
                     autospec=True, return_value=False)
    self.PatchObject(osutils, 'RmDir', autospec=True)
    self.PatchObject(cgroups, 'SimpleContainChildren', autospec=True)
    self._Prepare()

    # Simulate breakpad symbols being ready.
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('breakpad_symbols_generated', True)

  def ConstructStage(self):
    # pylint: disable=W0212
    self._run.GetArchive().SetupArchivePath()
    stage = vm_test_stages.GCETestStage(self._run, self._current_board)
    image_dir = stage.GetImageDirSymlink()
    osutils.Touch(os.path.join(image_dir, constants.TEST_KEY_PRIVATE),
                  makedirs=True)
    return stage

  def testGceTests(self):
    """Verifies that GCE_SMOKE_TEST_TYPE tests are run on GCE."""
    self._run.config['gce_tests'] = [
        config_lib.GCETestConfig(constants.GCE_SMOKE_TEST_TYPE)
    ]
    gce_tarball = constants.TEST_IMAGE_GCE_TAR

    # pylint: disable=unused-argument
    def _MockRunTestSuite(buildroot, board, image_path, results_dir, test_type,
                          *args, **kwargs):
      self.assertEndsWith(image_path, gce_tarball)
      self.assertEqual(test_type, constants.GCE_SMOKE_TEST_TYPE)
    # pylint: enable=unused-argument

    vm_test_stages.RunTestSuite.side_effect = _MockRunTestSuite

    self.RunStage()

    self.assertTrue(vm_test_stages.RunTestSuite.called and
                    vm_test_stages.RunTestSuite.call_count == 1)


class VMTestStageTest(generic_stages_unittest.AbstractStageTestCase,
                      cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests for the VMTest stage."""

  BOT_ID = 'amd64-generic-full'
  RELEASE_TAG = ''

  def setUp(self):
    for cmd in ('CreateTestRoot', 'GenerateStackTraces', 'ArchiveFile',
                'UploadArchivedFile', 'BuildAndArchiveTestResultsTarball'):
      self.PatchObject(commands, cmd, autospec=True)
    for cmd in ('RunTestSuite', 'ArchiveTestResults', 'ArchiveVMFiles',
                'RunDevModeTest', 'RunCrosVMTest',
                'ListTests', 'GetTestResultsDir',):
      self.PatchObject(vm_test_stages, cmd, autospec=True)
    self.PatchObject(vm_test_stages.VMTestStage, '_NoTestResults',
                     autospec=True, return_value=False)
    self.PatchObject(osutils, 'RmDir', autospec=True)
    self.PatchObject(cgroups, 'SimpleContainChildren', autospec=True)
    self._Prepare()

    # Simulate breakpad symbols being ready.
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('breakpad_symbols_generated', True)

  def ConstructStage(self):
    # pylint: disable=W0212
    self._run.GetArchive().SetupArchivePath()
    stage = vm_test_stages.VMTestStage(self._run, self._current_board)
    image_dir = stage.GetImageDirSymlink()
    osutils.Touch(os.path.join(image_dir, constants.TEST_KEY_PRIVATE),
                  makedirs=True)
    return stage

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self._run.config['vm_tests'] = [
        config_lib.VMTestConfig(constants.FULL_AU_TEST_TYPE)
    ]
    self.RunStage()

  def testQuickTests(self):
    """Tests if quick unit and cros_au_test_harness tests are run correctly."""
    self._run.config['vm_tests'] = [
        config_lib.VMTestConfig(constants.SIMPLE_AU_TEST_TYPE)
    ]
    self.RunStage()

  def testFailedTest(self):
    """Tests if quick unit and cros_au_test_harness tests are run correctly."""
    self.PatchObject(vm_test_stages.VMTestStage, '_RunTest',
                     autospec=True, side_effect=Exception())
    self.assertRaises(failures_lib.StepFailure, self.RunStage)

  def testRaisesInfraFail(self):
    """Tests that a infra failures has been raised."""
    commands.BuildAndArchiveTestResultsTarball.side_effect = (
        OSError('Cannot archive'))
    stage = self.ConstructStage()
    self.assertRaises(failures_lib.InfrastructureFailure, stage.PerformStage)

  def testSkipVMTest(self):
    """Tests trybot with no vm test."""
    extra_cmd_args = ['--novmtests']
    self._Prepare(extra_cmd_args=extra_cmd_args)
    self.RunStage()

  def testReportTestResults(self):
    """Test trybot with reporting function."""
    self._run.config['vm_tests'] = [
        config_lib.VMTestConfig(constants.FULL_AU_TEST_TYPE)
    ]
    self._run.config['vm_test_report_to_dashboards'] = True
    self.RunStage()


class RunTestSuiteTest(cros_build_lib_unittest.RunCommandTempDirTestCase):
  """Test RunTestSuite functionality."""

  TEST_BOARD = 'amd64-generic'
  BUILD_ROOT = '/fake/root'

  def _RunTestSuite(self, test_type):
    vm_test_stages.RunTestSuite(self.BUILD_ROOT, self.TEST_BOARD, self.tempdir,
                                '/tmp/taco', archive_dir='/fake/root',
                                whitelist_chrome_crashes=False,
                                test_type=test_type)
    self.assertCommandContains(['--no_graphics', '--verbose'])

  def testFull(self):
    """Test running FULL config."""
    self._RunTestSuite(constants.FULL_AU_TEST_TYPE)
    self.assertCommandContains(['--quick'], expected=False)
    self.assertCommandContains(['--only_verify'], expected=False)

  def testSimple(self):
    """Test SIMPLE config."""
    self._RunTestSuite(constants.SIMPLE_AU_TEST_TYPE)
    self.assertCommandContains(['--quick_update'])

  def testSmoke(self):
    """Test SMOKE config."""
    self._RunTestSuite(constants.SMOKE_SUITE_TEST_TYPE)
    self.assertCommandContains(['--only_verify'])

  def testGceSmokeTestType(self):
    """Test GCE_SMOKE_TEST_TYPE."""
    self._RunTestSuite(constants.GCE_SMOKE_TEST_TYPE)
    self.assertCommandContains(['--only_verify'])
    self.assertCommandContains(['--type=gce'])
    self.assertCommandContains(['--suite=gce-smoke'])

  def testGceSanityTestType(self):
    """Test GCE_SANITY_TEST_TYPE."""
    self._RunTestSuite(constants.GCE_SANITY_TEST_TYPE)
    self.assertCommandContains(['--only_verify'])
    self.assertCommandContains(['--type=gce'])
    self.assertCommandContains(['--suite=gce-sanity'])


class UnmockedTests(cros_test_lib.TempDirTestCase):
  """Test cases which really run tests, instead of using mocks."""

  def testListFaliedTests(self):
    """Tests if we can list failed tests."""
    test_report_1 = """
/tmp/taco/taste_tests/all/results-01-has_salsa              [  PASSED  ]
/tmp/taco/taste_tests/all/results-01-has_salsa/has_salsa    [  PASSED  ]
/tmp/taco/taste_tests/all/results-02-has_cheese             [  FAILED  ]
/tmp/taco/taste_tests/all/results-02-has_cheese/has_cheese  [  FAILED  ]
/tmp/taco/taste_tests/all/results-02-has_cheese/has_cheese   FAIL: No cheese.
"""
    test_report_2 = """
/tmp/taco/verify_tests/all/results-01-has_salsa              [  PASSED  ]
/tmp/taco/verify_tests/all/results-01-has_salsa/has_salsa    [  PASSED  ]
/tmp/taco/verify_tests/all/results-02-has_cheese             [  PASSED  ]
/tmp/taco/verify_tests/all/results-02-has_cheese/has_cheese  [  PASSED  ]
"""
    results_path = os.path.join(self.tempdir, 'tmp/taco')
    os.makedirs(results_path)
    # Create two reports with the same content to test that we don't
    # list the same test twice.
    osutils.WriteFile(
        os.path.join(results_path, 'taste_tests', 'all', 'test_report.log'),
        test_report_1, makedirs=True)
    osutils.WriteFile(
        os.path.join(results_path, 'taste_tests', 'failed', 'test_report.log'),
        test_report_1, makedirs=True)
    osutils.WriteFile(
        os.path.join(results_path, 'verify_tests', 'all', 'test_report.log'),
        test_report_2, makedirs=True)

    self.assertEquals(
        vm_test_stages.ListTests(results_path, show_passed=False),
        [('has_cheese', 'taste_tests/all/results-02-has_cheese')])

  def testArchiveTestResults(self):
    """Test if we can archive a test results dir."""
    test_results_dir = 'tmp/taco'
    results_path = os.path.join(self.tempdir, 'chroot', test_results_dir)
    archive_dir = os.path.join(self.tempdir, 'archived_taco')
    os.makedirs(results_path)
    os.makedirs(archive_dir)
    # File that should be archived.
    osutils.Touch(os.path.join(results_path, 'foo.txt'))
    # Flies that should be ignored.
    osutils.Touch(os.path.join(results_path,
                               'chromiumos_qemu_disk.bin.foo'))
    os.symlink('/src/foo', os.path.join(results_path, 'taco_link'))
    vm_test_stages.ArchiveTestResults(results_path, archive_dir)
    self.assertExists(os.path.join(archive_dir, 'foo.txt'))
    self.assertNotExists(
        os.path.join(archive_dir, 'chromiumos_qemu_disk.bin.foo'))
    self.assertNotExists(os.path.join(archive_dir, 'taco_link'))
