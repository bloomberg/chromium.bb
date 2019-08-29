# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolchain-related operations."""

from __future__ import print_function

from chromite.api import faux
from chromite.api import validate
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.lib import toolchain_util

_NAMES_FOR_ARTIFACTS = {
    toolchain_pb2.ORDERFILE: 'orderfile',
    toolchain_pb2.KERNEL_AFDO: 'kernel_afdo',
    toolchain_pb2.CHROME_AFDO: 'chrome_afdo'
}

# Using a function instead of a dict because we need to mock these
# functions in unittest, and mock doesn't play well with a dict definition.
def _GetMethodForUpdatingArtifacts(artifact_type):
  return {
      toolchain_pb2.ORDERFILE: toolchain_util.OrderfileUpdateChromeEbuild,
      toolchain_pb2.KERNEL_AFDO: toolchain_util.AFDOUpdateKernelEbuild,
      toolchain_pb2.CHROME_AFDO: toolchain_util.AFDOUpdateChromeEbuild
  }[artifact_type]


@faux.all_empty
@validate.require('build_target.name')
@validate.is_in('artifact_type', _NAMES_FOR_ARTIFACTS.keys())
@validate.validation_complete
def UpdateEbuildWithAFDOArtifacts(input_proto, output_proto, _config):
  """Update Chrome or kernel ebuild with most recent unvetted artifacts.

  Args:
    input_proto (VerifyAFDOArtifactsRequest): The input proto
    output_proto (VerifyAFDOArtifactsResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """

  board = input_proto.build_target.name
  update_method = _GetMethodForUpdatingArtifacts(input_proto.artifact_type)
  output_proto.status = update_method(board)


@faux.all_empty
@validate.require('build_target.name')
@validate.is_in('artifact_type', _NAMES_FOR_ARTIFACTS.keys())
@validate.validation_complete
def UploadVettedAFDOArtifacts(input_proto, output_proto, _config):
  """Upload a vetted orderfile to GS bucket.

  Args:
    input_proto (VerifyAFDOArtifactsRequest): The input proto
    output_proto (VerifyAFDOArtifactsResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  board = input_proto.build_target.name
  artifact_type = _NAMES_FOR_ARTIFACTS[input_proto.artifact_type]
  output_proto.status = toolchain_util.UploadAndPublishVettedAFDOArtifacts(
      artifact_type, board)
