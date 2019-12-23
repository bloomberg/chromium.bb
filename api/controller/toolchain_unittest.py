# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Toolchain-related operations."""

from __future__ import print_function

from chromite.api import api_config
from chromite.api import controller
from chromite.api.controller import toolchain
from chromite.api.gen.chromite.api import artifacts_pb2
from chromite.api.gen.chromite.api import sysroot_pb2
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.api.gen.chromiumos.builder_config_pb2 import BuilderConfig
from chromite.api.gen.chromiumos import common_pb2

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import toolchain_util

# pylint: disable=protected-access

class UpdateEbuildWithAFDOArtifactsTest(cros_test_lib.MockTestCase,
                                        api_config.ApiConfigMixin):
  """Unittests for UpdateEbuildWithAFDOArtifacts."""

  @staticmethod
  def mock_die(message, *args):
    raise cros_build_lib.DieSystemExit(message % args)

  def setUp(self):
    self.board = 'board'
    self.response = toolchain_pb2.VerifyAFDOArtifactsResponse()
    self.invalid_artifact_type = toolchain_pb2.BENCHMARK_AFDO
    self.orderfile_command = self.PatchObject(
        toolchain_util, 'OrderfileUpdateChromeEbuild', return_value=True)
    self.kernel_command = self.PatchObject(
        toolchain_util, 'AFDOUpdateKernelEbuild', return_value=True)
    self.chrome_command = self.PatchObject(
        toolchain_util, 'AFDOUpdateChromeEbuild', return_value=True)
    self.PatchObject(cros_build_lib, 'Die', new=self.mock_die)

  def _GetRequest(self, build_target=None, artifact_type=None):
    return toolchain_pb2.VerifyAFDOArtifactsRequest(
        build_target={'name': build_target},
        artifact_type=artifact_type,
    )

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(toolchain_util, 'OrderfileUpdateChromeEbuild')
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mock value."""
    patch = self.PatchObject(toolchain_util, 'OrderfileUpdateChromeEbuild')
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(self.response.status, True)

  def testWrongArtifactType(self):
    """Test passing wrong artifact type."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=self.invalid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                              self.api_config)
    self.assertIn('artifact_type (%d) must be in' % self.invalid_artifact_type,
                  str(context.exception))

  def testOrderfileSuccess(self):
    """Test the command is called correctly with orderfile."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.api_config)
    self.orderfile_command.assert_called_once_with(self.board)
    self.kernel_command.assert_not_called()
    self.chrome_command.assert_not_called()

  def testKernelAFDOSuccess(self):
    """Test the command is called correctly with kernel afdo."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.KERNEL_AFDO)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.api_config)
    self.kernel_command.assert_called_once_with(self.board)
    self.orderfile_command.assert_not_called()
    self.chrome_command.assert_not_called()

  def testChromeAFDOSuccess(self):
    """Test the command is called correctly with Chrome afdo."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.CHROME_AFDO)
    toolchain.UpdateEbuildWithAFDOArtifacts(request, self.response,
                                            self.api_config)
    self.chrome_command.assert_called_once_with(self.board)
    self.orderfile_command.assert_not_called()
    self.kernel_command.assert_not_called()


class UploadVettedFDOArtifactsTest(UpdateEbuildWithAFDOArtifactsTest):
  """Unittests for UploadVettedAFDOArtifacts."""

  @staticmethod
  def mock_die(message, *args):
    raise cros_build_lib.DieSystemExit(message % args)

  def setUp(self):
    self.board = 'board'
    self.response = toolchain_pb2.VerifyAFDOArtifactsResponse()
    self.invalid_artifact_type = toolchain_pb2.BENCHMARK_AFDO
    self.command = self.PatchObject(
        toolchain_util,
        'UploadAndPublishVettedAFDOArtifacts',
        return_value=True)
    self.PatchObject(cros_build_lib, 'Die', new=self.mock_die)

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UploadVettedAFDOArtifacts(request, self.response,
                                        self.validate_only_config)
    self.command.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mock value."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UploadVettedAFDOArtifacts(request, self.response,
                                        self.mock_call_config)
    self.command.assert_not_called()
    self.assertEqual(self.response.status, True)

  def testWrongArtifactType(self):
    """Test passing wrong artifact type."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=self.invalid_artifact_type)
    with self.assertRaises(cros_build_lib.DieSystemExit) as context:
      toolchain.UploadVettedAFDOArtifacts(request, self.response,
                                          self.api_config)
    self.assertIn('artifact_type (%d) must be in' % self.invalid_artifact_type,
                  str(context.exception))

  def testOrderfileSuccess(self):
    """Test the command is called correctly with orderfile."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.ORDERFILE)
    toolchain.UploadVettedAFDOArtifacts(request, self.response, self.api_config)
    self.command.assert_called_once_with('orderfile', self.board)

  def testKernelAFDOSuccess(self):
    """Test the command is called correctly with kernel afdo."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.KERNEL_AFDO)
    toolchain.UploadVettedAFDOArtifacts(request, self.response, self.api_config)
    self.command.assert_called_once_with('kernel_afdo', self.board)

  def testChromeAFDOSuccess(self):
    """Test the command is called correctly with Chrome afdo."""
    request = self._GetRequest(
        build_target=self.board, artifact_type=toolchain_pb2.CHROME_AFDO)
    toolchain.UploadVettedAFDOArtifacts(request, self.response, self.api_config)
    self.command.assert_called_once_with('chrome_afdo', self.board)


class PrepareForBuildTest(cros_test_lib.MockTempDirTestCase,
                          api_config.ApiConfigMixin):
  """Unittests for PrepareForBuild."""

  def setUp(self):
    self.response = toolchain_pb2.PrepareForToolchainBuildResponse()
    self.prep = self.PatchObject(
        toolchain_util, 'PrepareForBuild',
        return_value=toolchain_util.PrepareForBuildReturn.NEEDED)
    self.bundle = self.PatchObject(
        toolchain_util, 'BundleArtifacts', return_value=[])
    self.PatchObject(toolchain, '_TOOLCHAIN_ARTIFACT_HANDLERS', {
        BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE:
            toolchain._Handlers('UnverifiedOrderingFile',
                                self.prep, self.bundle),
    })

  def _GetRequest(self, artifact_types=None):
    if artifact_types is None:
      artifact_types = []
    chroot = common_pb2.Chroot(path=self.tempdir)
    sysroot = sysroot_pb2.Sysroot(
        path='/build/board', build_target=common_pb2.BuildTarget(name='board'))
    return toolchain_pb2.PrepareForToolchainBuildRequest(
        artifact_types=artifact_types,
        chroot=chroot, sysroot=sysroot,
    )

  def testRaisesForUnknown(self):
    request = self._GetRequest([BuilderConfig.Artifacts.IMAGE_ARCHIVES])
    self.assertRaises(
        KeyError,
        toolchain.PrepareForBuild, request, self.response, self.api_config)

  def testAcceptsNone(self):
    request = toolchain_pb2.PrepareForToolchainBuildRequest(
        artifact_types=[BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE],
        chroot=None, sysroot=None)
    toolchain.PrepareForBuild(request, self.response, self.api_config)
    self.prep.assert_called_once_with(
        'UnverifiedOrderingFile', None, '', '', {})

  def testHandlesUnknownInputArtifacts(self):
    request = toolchain_pb2.PrepareForToolchainBuildRequest(
        artifact_types=[BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE],
        chroot=None, sysroot=None, input_artifacts=[
            BuilderConfig.Artifacts.InputArtifactInfo(
                input_artifact_type=BuilderConfig.Artifacts.IMAGE_ZIP,
                input_artifact_gs_locations=['path1']),
        ])
    toolchain.PrepareForBuild(request, self.response, self.api_config)
    self.prep.assert_called_once_with(
        'UnverifiedOrderingFile', None, '', '', {})

  def testHandlesDuplicateInputArtifacts(self):
    request = toolchain_pb2.PrepareForToolchainBuildRequest(
        artifact_types=[BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE],
        chroot=None, sysroot=None, input_artifacts=[
            BuilderConfig.Artifacts.InputArtifactInfo(
                input_artifact_type=\
                    BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE,
                input_artifact_gs_locations=['path1', 'path2']),
            BuilderConfig.Artifacts.InputArtifactInfo(
                input_artifact_type=\
                    BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE,
                input_artifact_gs_locations=['path3']),
        ])
    toolchain.PrepareForBuild(request, self.response, self.api_config)
    self.prep.assert_called_once_with(
        'UnverifiedOrderingFile', None, '', '', {
            'UnverifiedOrderingFile': [
                'gs://path1', 'gs://path2', 'gs://path3']})


class BundleToolchainTest(cros_test_lib.MockTempDirTestCase,
                          api_config.ApiConfigMixin):
  """Unittests for BundleToolchain."""

  def setUp(self):
    self.response = toolchain_pb2.BundleToolchainResponse()
    self.prep = self.PatchObject(
        toolchain_util, 'PrepareForBuild',
        return_value=toolchain_util.PrepareForBuildReturn.NEEDED)
    self.bundle = self.PatchObject(
        toolchain_util, 'BundleArtifacts', return_value=[])
    self.PatchObject(toolchain, '_TOOLCHAIN_ARTIFACT_HANDLERS', {
        BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE:
            toolchain._Handlers('UnverifiedOrderingFile',
                                self.prep, self.bundle),
    })

  def _GetRequest(self, artifact_types=None):
    chroot = common_pb2.Chroot(path=self.tempdir)
    sysroot = sysroot_pb2.Sysroot(
        path='/build/board', build_target=common_pb2.BuildTarget(name='board'))
    return toolchain_pb2.BundleToolchainRequest(
        chroot=chroot, sysroot=sysroot,
        output_dir=self.tempdir,
        artifact_types=artifact_types,
    )

  def testRaisesForUnknown(self):
    request = self._GetRequest([BuilderConfig.Artifacts.IMAGE_ARCHIVES])
    self.assertEqual(
        controller.RETURN_CODE_UNRECOVERABLE,
        toolchain.BundleArtifacts(request, self.response, self.api_config))

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    request = self._GetRequest(
        [BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE])
    toolchain.BundleArtifacts(request, self.response,
                              self.validate_only_config)
    self.bundle.assert_not_called()

  def testSetsArtifactsInfo(self):
    request = self._GetRequest(
        [BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE])
    self.bundle.return_value = ['artifact.xz']
    toolchain.BundleArtifacts(request, self.response, self.api_config)
    self.assertEqual(1, len(self.response.artifacts_info))
    self.assertEqual(
        self.response.artifacts_info[0],
        toolchain_pb2.ArtifactInfo(
            artifact_type=BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE,
            artifacts=[
                artifacts_pb2.Artifact(path=self.bundle.return_value[0])]))
