# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolchain-related operations."""

from __future__ import print_function

import os

from chromite.api import validate
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import toolchain_util


@validate.require('build_target.name')
@validate.validation_complete
def UpdateChromeEbuildWithOrderfile(input_proto, _output_proto, _config):
  """Update Chrome ebuild with most recent unvetted orderfile.

  Args:
    input_proto (UpdateChromeEbuildRequest): The input proto
    _output_proto (UpdateChromeEbuildResponse): Empty output proto
    _config (api_config.ApiConfig): The API call config.
  """

  board = input_proto.build_target.name
  toolchain_util.OrderfileUpdateChromeEbuild(board)


@validate.validation_complete
def UploadVettedOrderfile(_input_proto, output_proto, _config):
  """Upload a vetted orderfile to GS bucket.

  Args:
    _input_proto (UploadVettedOrderfileRequest): Empty input proto
    output_proto (UploadVettedOrderfileResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  gs_context = gs.GSContext()
  orderfile = toolchain_util.FindLatestChromeOrderfile(
      toolchain_util.ORDERFILE_GS_URL_UNVETTED)

  if gs_context.Exists(
      os.path.join(toolchain_util.ORDERFILE_GS_URL_VETTED, orderfile)):
    output_proto.status = False
    return

  source_url = os.path.join(toolchain_util.ORDERFILE_GS_URL_UNVETTED, orderfile)
  dest_url = os.path.join(toolchain_util.ORDERFILE_GS_URL_VETTED, orderfile)

  logging.info('Copying tarball from %s to %s', source_url, dest_url)
  gs_context.Copy(source_url, dest_url, acl='public-read')
  output_proto.status = True
