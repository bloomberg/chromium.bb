# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Payload operations."""

from __future__ import print_function

import sys

from chromite.api import api_config
from chromite.api import controller
from chromite.api.controller import payload
from chromite.api.gen.chromite.api import payload_pb2
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import cros_test_lib
from chromite.lib.paygen import paygen_payload_lib


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class PayloadApiTests(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Unittests for SetBinhost."""

  def setUp(self):
    self.response = payload_pb2.PayloadGenerationResult()

    src_build = payload_pb2.Build(version='1.0.0', bucket='test',
                                  channel='test-channel', build_target=
                                  common_pb2.BuildTarget(name='cave'))

    src_image = payload_pb2.UnsignedImage(
        build=src_build, image_type=6, milestone='R70')

    tgt_build = payload_pb2.Build(version='2.0.0', bucket='test',
                                  channel='test-channel', build_target=
                                  common_pb2.BuildTarget(name='cave'))

    tgt_image = payload_pb2.UnsignedImage(
        build=tgt_build, image_type=6, milestone='R70')

    self.req = payload_pb2.PayloadGenerationRequest(
        tgt_unsigned_image=tgt_image, src_unsigned_image=src_image,
        bucket='test-destination-bucket', verify=True, keyset='update_signer')

    self.result = payload_pb2.PayloadGenerationResult()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""

    res = payload.GeneratePayload(self.req, self.result,
                                  self.validate_only_config)
    self.assertEqual(res, controller.RETURN_CODE_VALID_INPUT)

  def testCallSucceeds(self):
    """Check that a call is made succesfully."""
    # Deep patch the paygen lib, this is a full run through service as well.
    self.PatchObject(paygen_payload_lib, 'PaygenPayload')
    res = payload.GeneratePayload(self.req, self.result, self.api_config)
    self.assertEqual(res, controller.RETURN_CODE_SUCCESS)

  def testMockError(self):
    """Test mock error call does not execute any logic, returns error."""
    patch = self.PatchObject(paygen_payload_lib, 'PaygenPayload')

    res = payload.GeneratePayload(self.req, self.result,
                                  self.mock_error_config)
    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY, res)

  def testMockCall(self):
    """Test mock call does not execute any logic, returns success."""
    patch = self.PatchObject(paygen_payload_lib, 'PaygenPayload')

    res = payload.GeneratePayload(self.req, self.result,
                                  self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(controller.RETURN_CODE_SUCCESS, res)
