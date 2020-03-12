# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolchain-related operations."""

from __future__ import print_function

import collections
import sys

from chromite.api import controller
from chromite.api import faux
from chromite.api import validate
from chromite.api.controller import controller_util
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.api.gen.chromiumos.builder_config_pb2 import BuilderConfig
from chromite.lib import cros_logging as logging
from chromite.lib import toolchain_util


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


_Handlers = collections.namedtuple('_Handlers', ['name', 'prepare', 'bundle'])
_TOOLCHAIN_ARTIFACT_HANDLERS = {
    BuilderConfig.Artifacts.UNVERIFIED_CHROME_LLVM_ORDERFILE:
        _Handlers('UnverifiedChromeLlvmOrderfile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.VERIFIED_CHROME_LLVM_ORDERFILE:
        _Handlers('VerifiedChromeLlvmOrderfile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.CHROME_CLANG_WARNINGS_FILE:
        _Handlers('ChromeClangWarningsFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.UNVERIFIED_LLVM_PGO_FILE:
        _Handlers('UnverifiedLlvmPgoFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.UNVERIFIED_CHROME_BENCHMARK_AFDO_FILE:
        _Handlers('UnverifiedChromeBenchmarkAfdoFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.UNVERIFIED_CHROME_BENCHMARK_PERF_FILE:
        _Handlers('UnverifiedChromeBenchmarkPerfFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.VERIFIED_CHROME_BENCHMARK_AFDO_FILE:
        _Handlers('VerifiedChromeBenchmarkAfdoFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.UNVERIFIED_KERNEL_CWP_AFDO_FILE:
        _Handlers('UnverifiedKernelCwpAfdoFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.VERIFIED_KERNEL_CWP_AFDO_FILE:
        _Handlers('VerifiedKernelCwpAfdoFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.UNVERIFIED_CHROME_CWP_AFDO_FILE:
        _Handlers('UnverifiedChromeCwpAfdoFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.VERIFIED_CHROME_CWP_AFDO_FILE:
        _Handlers('VerifiedChromeCwpAfdoFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
    BuilderConfig.Artifacts.VERIFIED_RELEASE_AFDO_FILE:
        _Handlers('VerifiedReleaseAfdoFile',
                  toolchain_util.PrepareForBuild,
                  toolchain_util.BundleArtifacts),
}


# TODO(crbug/1031213): When @faux is expanded to have more than success/failure,
# this should be changed.
@faux.all_empty
@validate.require('artifact_types')
# Note: chroot and sysroot are unspecified the first time that the build_target
# recipe calls PrepareForBuild.  The second time, they are specified.  No
# validation check because "all" values are valid.
@validate.validation_complete
def PrepareForBuild(input_proto, output_proto, _config):
  """Prepare to build toolchain artifacts.

  The handlers (from _TOOLCHAIN_ARTIFACT_HANDLERS above) are called with:
      artifact_name (str): name of the artifact type.
      chroot (chroot_lib.Chroot): chroot.  Will be None if the chroot has not
          yet been created.
      sysroot_path (str): sysroot path inside the chroot (e.g., /build/atlas).
          Will be an empty string if the sysroot has not yet been created.
      build_target_name (str): name of the build target (e.g., atlas).  Will be
          an empty string if the sysroot has not yet been created.
      input_artifacts ({(str) name:[str gs_locations]}): locations for possible
          input artifacts.  The handler is expected to know which keys it should
          be using, and ignore any keys that it does not understand.
      additional_args ({(str) name: (str) value}) Dictionary of additional
          arguments.

  They locate and modify any ebuilds and/or source required for the artifact
  being created, then return a value from toolchain_util.PrepareForBuildReturn.

  This function sets output_proto.build_relevance to the result.

  Args:
    input_proto (PrepareForToolchainBuildRequest): The input proto
    output_proto (PrepareForToolchainBuildResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  if input_proto.chroot.path:
    chroot = controller_util.ParseChroot(input_proto.chroot)
  else:
    chroot = None

  input_artifacts = collections.defaultdict(list)
  for art in input_proto.input_artifacts:
    item = _TOOLCHAIN_ARTIFACT_HANDLERS.get(art.input_artifact_type)
    if item:
      input_artifacts[item.name].extend(
          ['gs://%s' % str(x) for x in art.input_artifact_gs_locations])

  # Pass along any additional args.
  additional_args = {}
  which = input_proto.additional_args.WhichOneof('prepare_for_build_args')
  if which:
    # All of the additional arguments we understand are strings, so we can
    # copy whichever argument we got without processing it.
    additional_args[which] = getattr(input_proto.additional_args, which)

  results = set()
  sysroot_path = input_proto.sysroot.path
  build_target = input_proto.sysroot.build_target.name
  for artifact_type in input_proto.artifact_types:
    # Unknown artifact_types are an error.
    handler = _TOOLCHAIN_ARTIFACT_HANDLERS[artifact_type]
    if handler.prepare:
      results.add(handler.prepare(
          handler.name, chroot, sysroot_path, build_target, input_artifacts,
          additional_args))

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
  return controller.RETURN_CODE_SUCCESS


# TODO(crbug/1031213): When @faux is expanded to have more than success/failure,
# this should be changed.
@faux.all_empty
@validate.require('chroot.path', 'sysroot.path', 'sysroot.build_target.name',
                  'output_dir', 'artifact_types')
@validate.exists('output_dir')
@validate.validation_complete
def BundleArtifacts(input_proto, output_proto, _config):
  """Bundle toolchain artifacts.

  The handlers (from _TOOLCHAIN_ARTIFACT_HANDLERS above) are called with:
      artifact_name (str): name of the artifact type
      chroot (chroot_lib.Chroot): chroot
      sysroot_path (str): sysroot path inside the chroot (e.g., /build/atlas)
      chrome_root (str): path to chrome root. (e.g., /b/s/w/ir/k/chrome)
      build_target_name (str): name of the build target (e.g., atlas)
      output_dir (str): absolute path where artifacts are being bundled.
        (e.g., /b/s/w/ir/k/recipe_cleanup/artifactssptfMU)

  Note: the actual upload to GS is done by CI, not here.

  Args:
    input_proto (BundleToolchainRequest): The input proto
    output_proto (BundleToolchainResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  chroot = controller_util.ParseChroot(input_proto.chroot)

  # Pass along any additional args.
  additional_args = {}
  which = input_proto.additional_args.WhichOneof('prepare_for_build_args')
  if which:
    # All of the additional arguments we understand are strings, so we can
    # copy whichever argument we got without processing it.
    additional_args[which] = getattr(input_proto.additional_args, which)

  for artifact_type in input_proto.artifact_types:
    if artifact_type not in _TOOLCHAIN_ARTIFACT_HANDLERS:
      logging.error('%s not understood', artifact_type)
      return controller.RETURN_CODE_UNRECOVERABLE
    handler = _TOOLCHAIN_ARTIFACT_HANDLERS[artifact_type]
    if handler and handler.bundle:
      artifacts = handler.bundle(
          handler.name, chroot, input_proto.sysroot.path,
          input_proto.sysroot.build_target.name, input_proto.output_dir,
          additional_args)
      if artifacts:
        art_info = output_proto.artifacts_info.add()
        art_info.artifact_type = artifact_type
        for artifact in artifacts:
          art_info.artifacts.add().path = artifact


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
