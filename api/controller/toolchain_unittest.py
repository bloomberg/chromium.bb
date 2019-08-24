# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Toolchain-related operations."""

from __future__ import print_function

import os

import mock

from chromite.api import api_config
from chromite.api.controller import toolchain
from chromite.api.gen.chromite.api import toolchain_pb2

from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import toolchain_util


class UpdateChromeEbuildWithOrderfileTest(cros_test_lib.MockTestCase,
                                          api_config.ApiConfigMixin):
  """Unittests for UpdateChromeEbuildWithOrderfile."""

  def setUp(self):
    self.command = self.PatchObject(toolchain_util,
                                    'OrderfileUpdateChromeEbuild')
    self.board = 'board'
    self.input_proto = toolchain_pb2.UpdateChromeEbuildRequest(
        build_target={'name': self.board})

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(toolchain_util, 'OrderfileUpdateChromeEbuild')

    response = toolchain_pb2.UpdateChromeEbuildResponse()
    toolchain.UpdateChromeEbuildWithOrderfile(self.input_proto, response,
                                              self.validate_only_config)
    patch.assert_not_called()

  def testSuccess(self):
    """Test the command is called correctly."""
    output_proto = toolchain_pb2.UpdateChromeEbuildResponse()
    toolchain.UpdateChromeEbuildWithOrderfile(self.input_proto,
                                              output_proto, self.api_config)
    self.command.assert_called_once_with(self.board)


class UploadVettedOrderfileTest(cros_test_lib.MockTestCase,
                                api_config.ApiConfigMixin):
  """Unittests for UploadVettedOrderfile."""

  def setUp(self):
    self.find_command = self.PatchObject(
        toolchain_util,
        'FindLatestChromeOrderfile',
        return_value='orderfile-3.0.tar.xz')
    self.copy_command = self.PatchObject(gs.GSContext, 'Copy', autospec=True)
    self.from_url = os.path.join(toolchain_util.ORDERFILE_GS_URL_UNVETTED,
                                 'orderfile-3.0.tar.xz')
    self.to_url = os.path.join(toolchain_util.ORDERFILE_GS_URL_VETTED,
                               'orderfile-3.0.tar.xz')
    self.input_proto = toolchain_pb2.UploadVettedOrderfileRequest()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(toolchain_util, 'OrderfileUpdateChromeEbuild')

    response = toolchain_pb2.UploadVettedOrderfileResponse()
    toolchain.UploadVettedOrderfile(self.input_proto, response,
                                    self.validate_only_config)
    patch.assert_not_called()

  def testRunFail(self):
    """Test that it fails to upload an orderfile because it's already vetted."""
    exist_command = self.PatchObject(gs.GSContext, 'Exists', return_value=True)
    output_proto = toolchain_pb2.UploadVettedOrderfileResponse()
    toolchain.UploadVettedOrderfile(self.input_proto, output_proto,
                                    self.api_config)
    self.assertFalse(output_proto.status)
    self.find_command.assert_called_once_with(
        toolchain_util.ORDERFILE_GS_URL_UNVETTED)
    exist_command.assert_called_once_with(self.to_url)

  def testRunPass(self):
    """Test run successfully."""
    exist_command = self.PatchObject(gs.GSContext, 'Exists', return_value=False)
    output_proto = toolchain_pb2.UploadVettedOrderfileResponse()
    toolchain.UploadVettedOrderfile(self.input_proto, output_proto,
                                    self.api_config)
    self.assertTrue(output_proto.status)
    self.find_command.assert_called_once_with(
        toolchain_util.ORDERFILE_GS_URL_UNVETTED)
    exist_command.assert_called_once_with(self.to_url)
    self.copy_command.assert_called_once_with(
        mock.ANY,  # Placeholder for 'self'
        self.from_url,
        self.to_url,
        acl='public-read')
