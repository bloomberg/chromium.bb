# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for updating Chrome ebuild stages."""

from __future__ import print_function

import os
import mock

from chromite.api.gen.chromite.api import toolchain_pb2

from chromite.cbuildbot import afdo
from chromite.cbuildbot import cbuildbot_unittest
from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import afdo_stages
from chromite.cbuildbot.stages import generic_stages_unittest

from chromite.lib import alerts
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib.buildstore import FakeBuildStore


def _GenerateAFDOGenerateTest(chrome_version_str, expected_to_generate_profile,
                              test_name):

  class Test(generic_stages_unittest.AbstractStageTestCase):
    """Test that we don't update non-r1 AFDO profiles"""

    def setUp(self):
      self.PatchObject(afdo, 'CanGenerateAFDOData', lambda _: True)
      chrome_version = portage_util.SplitCPV(chrome_version_str)
      self.PatchObject(portage_util, 'PortageqBestVisible',
                       lambda *_, **_2: chrome_version)
      self.wait_for_data = self.PatchObject(
          afdo, 'WaitForAFDOPerfData', autospec=True, wraps=lambda *_: True)

      afdo_path = '/does/not/exist/afdo.prof'
      self.generate_afdo_data = self.PatchObject(
          afdo, 'GenerateAFDOData', autospec=True, wraps=lambda *_: afdo_path)

      self.PatchObject(afdo_stages.AFDODataGenerateStage, '_GetCurrentArch',
                       lambda *_: 'some_arch')
      self.PatchObject(alerts, 'SendEmailLog')
      self._Prepare()
      self.buildstore = FakeBuildStore()

    def ConstructStage(self):
      return afdo_stages.AFDODataGenerateStage(self._run, self.buildstore,
                                               'some_board')

    def testAFDOUpdateChromeEbuildStage(self):
      self.RunStage()
      if expected_to_generate_profile:
        self.assertTrue(self.wait_for_data.called)
        self.assertTrue(self.generate_afdo_data.called)
      else:
        self.assertFalse(self.wait_for_data.called)
        self.assertFalse(self.generate_afdo_data.called)

  Test.__name__ = test_name
  return Test


def _MakeChromeVersionWithRev(rev):
  return 'chromeos-chrome/chromeos-chrome-68.0.3436.0_rc-r%d' % rev


R1Test = _GenerateAFDOGenerateTest(
    chrome_version_str=_MakeChromeVersionWithRev(1),
    expected_to_generate_profile=True,
    test_name='AFDODataGenerateStageUpdatesR1ProfilesTest',
)

R2Test = _GenerateAFDOGenerateTest(
    chrome_version_str=_MakeChromeVersionWithRev(2),
    expected_to_generate_profile=False,
    test_name='AFDODataGenerateStageDoesntUpdateNonR1ProfilesTest',
)


class UpdateChromeEbuildTest(generic_stages_unittest.AbstractStageTestCase):
  """Test updating Chrome ebuild files"""

  def setUp(self):
    # Intercept afdo.UpdateChromeEbuildAFDOFile so that we can check how it is
    # called.
    self.patch_mock = self.PatchObject(
        afdo, 'UpdateChromeEbuildAFDOFile', autospec=True)
    # Don't care the return value of portage_util.PortageqBestVisible
    self.PatchObject(portage_util, 'PortageqBestVisible')
    # Don't care the return value of gs.GSContext
    self.PatchObject(gs, 'GSContext')
    # Don't call the getters; Use mock responses instead.
    self.PatchDict(
        afdo.PROFILE_SOURCES, {
            'benchmark': lambda *_: 'benchmark.afdo',
            'silvermont': lambda *_: 'silvermont.afdo'
        },
        clear=True)
    self._Prepare()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    return afdo_stages.AFDOUpdateChromeEbuildStage(self._run, self.buildstore)

  def testAFDOUpdateChromeEbuildStage(self):
    self.RunStage()

    # afdo.UpdateChromeEbuildAFDOFile should be called with the mock responses
    # from profile source specific query.
    self.patch_mock.assert_called_with('amd64-generic', {
        'benchmark': 'benchmark.afdo',
        'silvermont': 'silvermont.afdo'
    })


class GenerateAFDOArtifactStageTests(
    generic_stages_unittest.AbstractStageTestCase,
    cbuildbot_unittest.SimpleBuilderTestCase):
  """Test class of GenerateAFDOArtifactStage."""

  RELEASE_TAG = ''

  # pylint: disable=protected-access
  def setUp(self):
    self._Prepare()
    self.rc_mock = self.StartPatcher(cros_test_lib.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()
    self.buildstore = FakeBuildStore()
    # Prepare the directories
    chroot_tmp = os.path.join(self.build_root, 'chroot', 'tmp')
    osutils.SafeMakedirs(chroot_tmp)

  # pylint: disable=arguments-differ
  def ConstructStage(self, is_afdo):
    self._run.GetArchive().SetupArchivePath()
    if is_afdo:
      return afdo_stages.GenerateBenchmarkAFDOStage(
          self._run, self.buildstore, self._current_board)

    return afdo_stages.GenerateChromeOrderfileStage(
        self._run, self.buildstore, self._current_board)

  def testRunSuccess(self):
    """Test the main function runs without problems."""
    for is_afdo in [True, False]:
      stage = self.ConstructStage(is_afdo)
      artifacts = ['tarball.1.xz', '/path/to/tarball.2.xz']
      self.PatchObject(stage, 'ArtifactUploader')
      mock_generate = self.PatchObject(
          commands, 'GenerateAFDOArtifacts',
          return_value=artifacts)
      mock_put = self.PatchObject(stage._upload_queue, 'put')
      stage.PerformStage()
      output_path = os.path.abspath(
          os.path.join(self.build_root, 'chroot',
                       stage.archive_path))
      if is_afdo:
        target = toolchain_pb2.BENCHMARK_AFDO
      else:
        target = toolchain_pb2.ORDERFILE
      mock_generate.assert_called_once_with(
          self.build_root, None, self._current_board,
          output_path, target)
      calls = [mock.call([os.path.basename(x)]) for x in artifacts]
      mock_put.assert_has_calls(calls)


class VerifyAFDOArtifactStageTests(
    generic_stages_unittest.AbstractStageTestCase,
    cbuildbot_unittest.SimpleBuilderTestCase):
  """Test class of VerifyAFDOArtifactStage."""
  RELEASE_TAG = ''

  # pylint: disable=protected-access
  def setUp(self):
    self._Prepare()
    self.rc_mock = self.StartPatcher(cros_test_lib.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()
    self.buildstore = FakeBuildStore()

  # pylint: disable=arguments-differ
  def ConstructStage(self, artifact_type, stage_type):
    self._run.GetArchive().SetupArchivePath()
    if artifact_type == 'orderfile':
      if stage_type == 'update':
        return afdo_stages.OrderfileUpdateChromeEbuildStage(
            self._run, self.buildstore, self._current_board)

      return afdo_stages.UploadVettedOrderfileStage(
          self._run, self.buildstore, self._current_board)

    if artifact_type == 'kernel_afdo':
      if stage_type == 'update':
        return afdo_stages.KernelAFDOUpdateEbuildStage(
            self._run, self.buildstore, self._current_board)

      return afdo_stages.UploadVettedKernelAFDOStage(
          self._run, self.buildstore, self._current_board)

    if stage_type == 'update':
      return afdo_stages.ChromeAFDOUpdateEbuildStage(
          self._run, self.buildstore, self._current_board)

    return afdo_stages.UploadVettedChromeAFDOStage(
        self._run, self.buildstore, self._current_board)

  def testOrderfileUpdateSuccess(
      self, artifact_type='orderfile', stage_type='update'):
    """Test update ebuild with orderfile call correctly.

    Parameterize the two arguments for testing different stage types.
    """
    stage = self.ConstructStage(artifact_type, stage_type)
    mock_verify = self.PatchObject(
        commands, 'VerifyAFDOArtifacts', return_value=True)

    stage.PerformStage()

    # Verify wrapper function is called correctly.
    if artifact_type == 'orderfile':
      target = toolchain_pb2.ORDERFILE
    elif artifact_type == 'kernel_afdo':
      target = toolchain_pb2.KERNEL_AFDO
    else:
      target = toolchain_pb2.CHROME_AFDO

    if stage_type == 'update':
      build_api = 'chromite.api.ToolchainService/UpdateEbuildWithAFDOArtifacts'
    else:
      build_api = 'chromite.api.ToolchainService/UploadVettedAFDOArtifacts'

    mock_verify.assert_called_once_with(
        self.build_root, self._current_board,
        target, build_api)

  def testOrderfileUploadSuccess(self):
    """Test upload vetted orderfile call correctly."""
    self.testOrderfileUpdateSuccess(stage_type='upload')

  def testKernelAFDOUpdateSuccess(self):
    """Test update ebuild with kernel AFDO call correctly."""
    self.testOrderfileUpdateSuccess(artifact_type='kernel_afdo')

  def testKernelAFDOUploadSuccess(self):
    """Test upload vetted kernel AFDO call correctly."""
    self.testOrderfileUpdateSuccess(artifact_type='kernel_afdo',
                                    stage_type='upload')

  def testChromeAFDOUpdateSuccess(self):
    """Test update ebuild with Chrome AFDO call correctly."""
    self.testOrderfileUpdateSuccess(artifact_type='chrome_afdo')

  def testChromeAFDOUploadSuccess(self):
    """Test upload vetted Chrome AFDO call correctly."""
    self.testOrderfileUpdateSuccess(artifact_type='chrome_afdo',
                                    stage_type='upload')
