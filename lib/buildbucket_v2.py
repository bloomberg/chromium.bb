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

from chromite.lib.luci.prpc.client import Client

from infra_libs.buildbucket.proto import rpc_pb2 #pylint: disable=unused-import
from infra_libs.buildbucket.proto.rpc_prpc_pb2 import BuildsServiceDescription

BBV2_URL_ENDPOINT_PROD = (
    "cr-buildbucket.appspot.com"
)
BBV2_URL_ENDPOINT_TEST = (
    "cr-buildbucket-test.appspot.com"
)


def GetBuildClient(test_env=False):
  """Constructor for Buildbucket V2 Build client.

  Args:
    test_env: Whether to have the client connect to test URL endpoint on GAE.

  Returns:
    An instance of chromite.lib.luci.prpc.client.Client that connects to the
    selected endpoint.
  """
  if test_env:
    return Client(BBV2_URL_ENDPOINT_TEST, BuildsServiceDescription)

  return Client(BBV2_URL_ENDPOINT_PROD, BuildsServiceDescription)
