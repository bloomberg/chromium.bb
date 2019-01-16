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

from google.protobuf import field_mask_pb2

from chromite.lib import cros_logging as logging
from chromite.lib.luci.prpc.client import Client

from infra_libs.buildbucket.proto import rpc_pb2
from infra_libs.buildbucket.proto.rpc_prpc_pb2 import BuildsServiceDescription

BBV2_URL_ENDPOINT_PROD = (
    "cr-buildbucket.appspot.com"
)
BBV2_URL_ENDPOINT_TEST = (
    "cr-buildbucket-test.appspot.com"
)

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

def UpdateSelfCommonBuildProperties(critical=None):
  """Update build.output.properties for the current build.

  This will be a generic function for all properties to be stored in
  Buildbucket.

  Args:
    critical: (Optional) If provided, the |important| flag for this build.
  """
  if critical is not None:
    UpdateSelfBuildPropertiesNonBlocking('critical', critical)

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

  def GetBuildStatus(self, buildbucket_id, properties=None):
    """GetBuildStatus of a specific build with buildbucket_id.

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
