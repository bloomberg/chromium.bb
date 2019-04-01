# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite Buildbucket V2 client wrapper and helpers.

The client constructor and methods here are tailored to Chromite's
usecases. Other users should, instead, prefer to copy the Python
client out of lib/luci/prpc and third_party/infra_libs/buildbucket.
"""

from __future__ import print_function

import ast

from google.protobuf import field_mask_pb2

from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib.luci.prpc.client import Client, ProtocolError

from infra_libs.buildbucket.proto import rpc_pb2
from infra_libs.buildbucket.proto.rpc_prpc_pb2 import BuildsServiceDescription

BBV2_URL_ENDPOINT_PROD = (
    "cr-buildbucket.appspot.com"
)
BBV2_URL_ENDPOINT_TEST = (
    "cr-buildbucket-test.appspot.com"
)
BB_STATUS_DICT = {
    # A mapping of Buildbucket V2 statuses to chromite's statuses. For
    # buildbucket reference, see here:
    # https://chromium.googlesource.com/infra/luci/luci-go/+/master/buildbucket/proto/common.proto
    0: 'unspecified',
    1: constants.BUILDER_STATUS_PLANNED,
    2: constants.BUILDER_STATUS_INFLIGHT,
    12: constants.BUILDER_STATUS_PASSED,
    20: constants.BUILDER_STATUS_FAILED,
    68: constants.BUILDER_STATUS_ABORTED
}

def UpdateSelfBuildPropertiesNonBlocking(key, value):
  """Updates the build.output.properties with key:value through a service.

  Butler is a ChOps service that reads in logs and updates Buildbucket of the
  properties. This method has no guarantees on the timeliness of updating
  the property.

  Args:
    key: name of the property.
    value: value of the property.
  """
  logging.PrintKitchenSetBuildProperty(key, value)

def UpdateSelfCommonBuildProperties(
    critical=None, chrome_version=None, milestone_version=None,
    platform_version=None, full_version=None, toolchain_url=None,
    build_type=None, unibuild=None, suite_scheduling=None,
    killed_child_builds=None, board=None, main_firmware_version=None,
    ec_firmware_version=None):
  """Update build.output.properties for the current build.

  Sends the property values to buildbucket via
  UpdateSelfBuildPropertiesNonBlocking. All arguments are optional.

  Args:
    critical: (Optional) |important| flag of the build.
    chrome_version: (Optional) version of chrome of the build. Eg "74.0.3687.0".
    milestone_version: (Optional) milestone version of  of the build. Eg "74".
    platform_version: (Optional) platform version of the build. Eg "11671.0.0".
    full_version: (Optional) full version of the build.
        Eg "R74-11671.0.0-b3416654".
    toolchain_url: (Optional) toolchain_url of the build.
    build_type: (Optional) One of ('paladin', 'full', 'canary', 'pre_cq',...).
    unibuild: (Optional) Boolean indicating whether build is unibuild.
    suite_scheduling: (Optional)
    killed_child_builds: (Optional) A list of Buildbucket IDs of child builds
      that were killed by self-destructed master build.
    board: (Optional) board of the build.
    main_firmware_version: (Optional) main firmware version of the build.
    ec_firmware_version: (Optional) ec_firmware version of the build.
  """
  if critical is not None:
    UpdateSelfBuildPropertiesNonBlocking('critical', critical)
  if chrome_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('chrome_version', chrome_version)
  if milestone_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('milestone_version', milestone_version)
  if platform_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('platform_version', platform_version)
  if full_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('full_version', full_version)
  if toolchain_url is not None:
    UpdateSelfBuildPropertiesNonBlocking('toolchain_url', toolchain_url)
  if build_type is not None:
    UpdateSelfBuildPropertiesNonBlocking('build_type', build_type)
  if unibuild is not None:
    UpdateSelfBuildPropertiesNonBlocking('unibuild', unibuild)
  if suite_scheduling is not None:
    UpdateSelfBuildPropertiesNonBlocking('suite_scheduling', suite_scheduling)
  if killed_child_builds is not None:
    UpdateSelfBuildPropertiesNonBlocking('killed_child_builds',
                                         str(killed_child_builds))
  if board is not None:
    UpdateSelfBuildPropertiesNonBlocking('board', board)
  if main_firmware_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('main_firmware_version',
                                         main_firmware_version)
  if ec_firmware_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('ec_firmware_version',
                                         ec_firmware_version)

def UpdateBuildMetadata(metadata):
  """Update build.output.properties from a CBuildbotMetadata instance.

  The function further uses UpdateSelfCommonBuildProperties and has hence
  no guarantee of timely updation.

  Args:
    metadata: CBuildbot Metadata instance to update with.
  """
  d = metadata.GetDict()
  versions = d.get('version') or {}
  UpdateSelfCommonBuildProperties(
      chrome_version=versions.get('chrome'),
      milestone_version=versions.get('milestone'),
      platform_version=versions.get('platform'),
      full_version=versions.get('full'),
      toolchain_url=d.get('toolchain-url'),
      build_type=d.get('build_type'),
      critical=d.get('important'),
      unibuild=d.get('unibuild', False),
      suite_scheduling=d.get('suite_scheduling', False))


class BuildbucketV2(object):
  """Connection to Buildbucket V2 database."""

  def __init__(self, test_env=False):
    """Constructor for Buildbucket V2 Build client.

    Args:
      test_env: Whether to have the client connect to test URL endpoint on GAE.
    """
    if test_env:
      self.client = Client(BBV2_URL_ENDPOINT_TEST, BuildsServiceDescription)
    else:
      self.client = Client(BBV2_URL_ENDPOINT_PROD, BuildsServiceDescription)

  def GetBuild(self, buildbucket_id, properties=None):
    """GetBuild call of a specific build with buildbucket_id.

    Args:
      buildbucket_id: id of the build in buildbucket.
      properties: specific build.output.properties to query.

    Returns:
      The corresponding Build proto. See here:
      https://chromium.googlesource.com/infra/luci/luci-go/+/master/buildbucket/proto/build.proto
    """
    if properties:
      field_mask = field_mask_pb2.FieldMask(paths=[properties])
      get_build_request = rpc_pb2.GetBuildRequest(id=buildbucket_id,
                                                  fields=field_mask)
    else:
      get_build_request = rpc_pb2.GetBuildRequest(id=buildbucket_id)
    return self.client.GetBuild(get_build_request)

  def GetKilledChildBuilds(self, buildbucket_id):
    """Get IDs of all the builds killed by self-destructed master build.

    Args:
      buildbucket_id: Buildbucket ID of the master build.

    Returns:
      A list of Buildbucket IDs of the child builds that were killed by the
      master build or None if the query was unsuccessful.
    """
    properties = 'output.properties'
    try:
      build_with_properties = self.GetBuild(buildbucket_id,
                                            properties=properties)
    except ProtocolError:
      logging.error('Could not fetch Buildbucket status for %d', buildbucket_id)
      return

    if build_with_properties.output.HasField('properties'):
      build_properties = build_with_properties.output.properties
      if ('killed_child_builds' in build_properties and
          build_properties['killed_child_builds'] is not 'None'):
        return ast.literal_eval(build_properties['killed_child_builds'])
