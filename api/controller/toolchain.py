# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolchain-related operations."""

from __future__ import print_function

import collections

from chromite.api import faux
from chromite.api import validate
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.lib import toolchain_util
from chromite.api.gen.chromiumos.builder_config_pb2 import BuilderConfig

# TODO(crbug/1019868): Add handlers as needed.
_Handlers = collections.namedtuple('_Handlers', ['name', 'prepare', 'bundle'])
_TOOLCHAIN_ARTIFACT_HANDLERS = {
    BuilderConfig.Artifacts.UNVERIFIED_ORDERING_FILE:
        _Handlers('UnverifiedOrderingFile', None, None),
    BuilderConfig.Artifacts.VERIFIED_ORDERING_FILE:
        _Handlers('VerifedOrderingFile', None, None),
    BuilderConfig.Artifacts.CHROME_CLANG_WARNINGS_FILE:
        _Handlers('ChromeClangWarningsFile', None, None),
    BuilderConfig.Artifacts.UNVERIFIED_LLVM_PGO_FILE:
        _Handlers('UnverifiedLlvmPgoFile', None, None),
    BuilderConfig.Artifacts.UNVERIFIED_CHROME_AFDO_FILE:
        _Handlers('UnverifiedChromeAfdoFile', None, None),
    BuilderConfig.Artifacts.VERIFIED_CHROME_AFDO_FILE:
        _Handlers('VerifiedChromeAfdoFile', None, None),
    BuilderConfig.Artifacts.VERIFIED_KERNEL_AFDO_FILE:
        _Handlers('VerifiedKernelAfdoFile', None, None),
}


# TODO(crbug/1031213): When @faux is expanded to have more than success/failure,
# this should be changed.
@faux.all_empty
@validate.require('artifact_types')
@validate.validation_complete
def PrepareForBuild(input_proto, output_proto, _config):
  """Prepare to build toolchain artifacts.

  The handlers (from _TOOLCHAIN_ARTIFACT_HANDLERS above) are called with:
      artifact_name (str): name of the artifact type.
      chroot_path (str):  chroot path (absolute path)
      sysroot_path (str): sysroot path inside the chroot (e.g., /build/atlas)
      build_target_name (str): name of the build target (e.g., atlas)

  They locate and modify any ebuilds and/or source required for the artifact
  being created, then return a value from toolchain_util.PrepareForBuildReturn.

  Args:
    input_proto (PrepareForToolchainBuildRequest): The input proto
    output_proto (PrepareForToolchainBuildResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  results = set()
  for artifact_type in input_proto.artifact_types:
    # Ignore any artifact_types not handled.
    handler = _TOOLCHAIN_ARTIFACT_HANDLERS.get(artifact_type)
    if handler and handler.prepare:
      results.add(handler.prepare(
          handler.name, input_proto.chroot.path, input_proto.sysroot.path,
          input_proto.sysroot.build_target.name))

  # Translate the returns from the handlers we called.
  #   If any NEEDED => NEEDED
  #   elif any UNKNOWN => UNKNOWN
  #   elif any POINTLESS => POINTLESS
  #   else UNKNOWN.
  proto_resp = toolchain_pb2.PrepareForToolchainBuildResponse
  if toolchain_util.PrepareForBuildReturn.NEEDED in results:
    output_proto.build_relevance = proto_resp.NEEDED
  elif toolchain_util.PrepareForBuildReturn.UNKNOWN in results:
    output_proto.build_relevance = proto_resp.UNKNOWN
  elif toolchain_util.PrepareForBuildReturn.POINTLESS in results:
    output_proto.build_relevance = proto_resp.POINTLESS
  else:
    output_proto.build_relevance = proto_resp.UNKNOWN


# TODO(crbug/1031213): When @faux is expanded to have more than success/failure,
# this should be changed.
@faux.all_empty
@validate.require('artifact_types')
@validate.require('output_dir')
def BundleArtifacts(input_proto, output_proto, _config):
  """Bundle toolchain artifacts.

  The handlers (from _TOOLCHAIN_ARTIFACT_HANDLERS above) are called with:
      artifact_name (str): name of the artifact type
      chroot_path (str):  chroot path (absolute path)
      sysroot_path (str): sysroot path inside the chroot (e.g., /build/atlas)
      build_target_name (str): name of the build target (e.g., atlas)
      output_dir (str): absolute path where artifacts are being bundled.
        (e.g., /b/s/w/ir/k/recipe_cleanup/artifactssptfMU)

  Note: the actual upload to GS is done by CI, not here.

  Args:
    input_proto (BundleToolchainRequest): The input proto
    output_proto (BundleToolchainResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  # TODO(crbug/1019868): This is moving, handle both cases.
  resp_artifact = (
      getattr(toolchain_pb2.BundleToolchainResponse, 'ArtifactInfo', None) or
      getattr(toolchain_pb2, 'ArtifactInfo'))
  # resp_artifact = toolchain_pb2.ArtifactInfo

  for artifact_type in input_proto.artifact_types:
    # Ignore any artifact_types not handled.
    handler = _TOOLCHAIN_ARTIFACT_HANDLERS.get(artifact_type)
    if handler and handler.bundle:
      artifacts = handler.bundle(
          handler.name, input_proto.chroot.path, input_proto.sysroot.path,
          input_proto.sysroot.build_target.name, input_proto.output_dir)
      if artifacts:
        output_proto.artifacts_info.append(
            resp_artifact(artifact_type=artifact_type, artifacts=artifacts))


# TODO(crbug/1019868): Remove legacy code when cbuildbot builders are gone.
_NAMES_FOR_AFDO_ARTIFACTS = {
    toolchain_pb2.ORDERFILE: 'orderfile',
    toolchain_pb2.KERNEL_AFDO: 'kernel_afdo',
    toolchain_pb2.CHROME_AFDO: 'chrome_afdo'
}

# TODO(crbug/1019868): Remove legacy code when cbuildbot builders are gone.
# Using a function instead of a dict because we need to mock these
# functions in unittest, and mock doesn't play well with a dict definition.
def _GetMethodForUpdatingAFDOArtifacts(artifact_type):
  return {
      toolchain_pb2.ORDERFILE: toolchain_util.OrderfileUpdateChromeEbuild,
      toolchain_pb2.KERNEL_AFDO: toolchain_util.AFDOUpdateKernelEbuild,
      toolchain_pb2.CHROME_AFDO: toolchain_util.AFDOUpdateChromeEbuild
  }[artifact_type]


# TODO(crbug/1019868): Remove legacy code when cbuildbot builders are gone.
def _UpdateEbuildWithAFDOArtifactsResponse(_input_proto, output_proto, _config):
  """Add successful status to the faux response."""
  output_proto.status = True


# TODO(crbug/1019868): Remove legacy code when cbuildbot builders are gone.
@faux.success(_UpdateEbuildWithAFDOArtifactsResponse)
@faux.empty_error
@validate.require('build_target.name')
@validate.is_in('artifact_type', _NAMES_FOR_AFDO_ARTIFACTS)
@validate.validation_complete
def UpdateEbuildWithAFDOArtifacts(input_proto, output_proto, _config):
  """Update Chrome or kernel ebuild with most recent unvetted artifacts.

  Args:
    input_proto (VerifyAFDOArtifactsRequest): The input proto
    output_proto (VerifyAFDOArtifactsResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  board = input_proto.build_target.name
  update_method = _GetMethodForUpdatingAFDOArtifacts(input_proto.artifact_type)
  output_proto.status = update_method(board)


# TODO(crbug/1019868): Remove legacy code when cbuildbot builders are gone.
def _UploadVettedAFDOArtifactsResponse(_input_proto, output_proto, _config):
  """Add successful status to the faux response."""
  output_proto.status = True


# TODO(crbug/1019868): Remove legacy code when cbuildbot builders are gone.
@faux.success(_UploadVettedAFDOArtifactsResponse)
@faux.empty_error
@validate.require('build_target.name')
@validate.is_in('artifact_type', _NAMES_FOR_AFDO_ARTIFACTS)
@validate.validation_complete
def UploadVettedAFDOArtifacts(input_proto, output_proto, _config):
  """Upload a vetted orderfile to GS bucket.

  Args:
    input_proto (VerifyAFDOArtifactsRequest): The input proto
    output_proto (VerifyAFDOArtifactsResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  board = input_proto.build_target.name
  artifact_type = _NAMES_FOR_AFDO_ARTIFACTS[input_proto.artifact_type]
  output_proto.status = toolchain_util.UploadAndPublishVettedAFDOArtifacts(
      artifact_type, board)
