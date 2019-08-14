# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Binhost operations."""

from __future__ import print_function

import mock

from chromite.api import api_config
from chromite.api.controller import binhost
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.service import binhost as binhost_service


class PrepareBinhostUploadsTest(cros_test_lib.MockTestCase,
                                api_config.ApiConfigMixin):
  """Unittests for PrepareBinhostUploads."""

  def setUp(self):
    self.PatchObject(binhost_service, 'GetPrebuiltsRoot',
                     return_value='/build/target/packages')
    self.PatchObject(binhost_service, 'GetPrebuiltsFiles',
                     return_value=['foo.tbz2', 'bar.tbz2'])
    self.PatchObject(binhost_service, 'UpdatePackageIndex',
                     return_value='/build/target/packages/Packages')

    self.response = binhost_pb2.PrepareBinhostUploadsResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service, 'GetPrebuiltsRoot')

    request = binhost_pb2.PrepareBinhostUploadsRequest()
    request.build_target.name = 'target'
    request.uri = 'gs://chromeos-prebuilt/target'
    rc = binhost.PrepareBinhostUploads(request, self.response,
                                       self.validate_only_config)
    patch.assert_not_called()
    self.assertEqual(rc, 0)

  def testPrepareBinhostUploads(self):
    """PrepareBinhostUploads returns Packages and tar files."""
    input_proto = binhost_pb2.PrepareBinhostUploadsRequest()
    input_proto.build_target.name = 'target'
    input_proto.uri = 'gs://chromeos-prebuilt/target'
    binhost.PrepareBinhostUploads(input_proto, self.response, self.api_config)
    self.assertEqual(self.response.uploads_dir, '/build/target/packages')
    self.assertItemsEqual(
        [ut.path for ut in self.response.upload_targets],
        ['Packages', 'foo.tbz2', 'bar.tbz2'])

  def testPrepareBinhostUploadsNonGsUri(self):
    """PrepareBinhostUploads dies when URI does not point to GS."""
    input_proto = binhost_pb2.PrepareBinhostUploadsRequest()
    input_proto.build_target.name = 'target'
    input_proto.uri = 'https://foo.bar'
    with self.assertRaises(ValueError):
      binhost.PrepareBinhostUploads(input_proto, self.response, self.api_config)


class SetBinhostTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Unittests for SetBinhost."""

  def setUp(self):
    self.response = binhost_pb2.SetBinhostResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service, 'GetPrebuiltsRoot')

    request = binhost_pb2.PrepareBinhostUploadsRequest()
    request.build_target.name = 'target'
    request.uri = 'gs://chromeos-prebuilt/target'
    binhost.PrepareBinhostUploads(request, self.response,
                                  self.validate_only_config)
    patch.assert_not_called()

  def testSetBinhost(self):
    """SetBinhost calls service with correct args."""
    set_binhost = self.PatchObject(binhost_service, 'SetBinhost',
                                   return_value='/path/to/BINHOST.conf')

    input_proto = binhost_pb2.SetBinhostRequest()
    input_proto.build_target.name = 'target'
    input_proto.private = True
    input_proto.key = binhost_pb2.POSTSUBMIT_BINHOST
    input_proto.uri = 'gs://chromeos-prebuilt/target'

    binhost.SetBinhost(input_proto, self.response, self.api_config)

    self.assertEqual(self.response.output_file, '/path/to/BINHOST.conf')
    set_binhost.assert_called_once_with(
        'target', 'PARALLEL_POSTSUBMIT_BINHOST',
        'gs://chromeos-prebuilt/target', private=True)


class RegenBuildCacheTest(cros_test_lib.MockTestCase,
                          api_config.ApiConfigMixin):
  """Unittests for RegenBuildCache."""

  def setUp(self):
    self.response = binhost_pb2.RegenBuildCacheResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service, 'RegenBuildCache')

    request = binhost_pb2.RegenBuildCacheRequest()
    request.overlay_type = binhost_pb2.OVERLAYTYPE_BOTH
    binhost.RegenBuildCache(request, self.response, self.validate_only_config)
    patch.assert_not_called()

  def testRegenBuildCache(self):
    """RegenBuildCache calls service with the correct args."""
    regen_cache = self.PatchObject(binhost_service, 'RegenBuildCache')

    input_proto = binhost_pb2.RegenBuildCacheRequest()
    input_proto.overlay_type = binhost_pb2.OVERLAYTYPE_BOTH

    binhost.RegenBuildCache(input_proto, self.response, self.api_config)
    regen_cache.assert_called_once_with(mock.ANY, 'both')

  def testRequiresOverlayType(self):
    """RegenBuildCache dies if overlay_type not specified."""
    regen_cache = self.PatchObject(binhost_service, 'RegenBuildCache')

    input_proto = binhost_pb2.RegenBuildCacheRequest()
    input_proto.overlay_type = binhost_pb2.OVERLAYTYPE_UNSPECIFIED

    with self.assertRaises(cros_build_lib.DieSystemExit):
      binhost.RegenBuildCache(input_proto, self.response, self.api_config)
    regen_cache.assert_not_called()
