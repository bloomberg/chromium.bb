# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Binhost operations."""

from __future__ import print_function

from chromite.api.controller import binhost
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.service import binhost as binhost_service

class PrepareBinhostUploadsTest(cros_test_lib.MockTestCase):
  """Unittests for PrepareBinhostUploads."""

  def setUp(self):
    self.PatchObject(binhost_service, 'GetPrebuiltsRoot',
                     return_value='/build/target/packages')
    self.PatchObject(binhost_service, 'GetPrebuiltsFiles',
                     return_value=['foo.tbz2', 'bar.tbz2'])
    self.PatchObject(binhost_service, 'UpdatePackageIndex',
                     return_value='/build/target/packages/Packages')

  def testPrepareBinhostUploads(self):
    """PrepareBinhostUploads returns Packages and tar files."""
    input_proto = binhost_pb2.PrepareBinhostUploadsRequest()
    input_proto.build_target.name = 'target'
    input_proto.uri = 'gs://chromeos-prebuilt/target'
    output_proto = binhost_pb2.PrepareBinhostUploadsResponse()
    binhost.PrepareBinhostUploads(input_proto, output_proto)
    self.assertEqual(output_proto.uploads_dir, '/build/target/packages')
    self.assertItemsEqual(
        [ut.path for ut in output_proto.upload_targets],
        ['Packages', 'foo.tbz2', 'bar.tbz2'])

  def testPrepareBinhostUploadsNonGsUri(self):
    """PrepareBinhostUploads dies when URI does not point to GS."""
    input_proto = binhost_pb2.PrepareBinhostUploadsRequest()
    input_proto.build_target.name = 'target'
    input_proto.uri = 'https://foo.bar'
    output_proto = binhost_pb2.PrepareBinhostUploadsResponse()
    with self.assertRaises(ValueError):
      binhost.PrepareBinhostUploads(input_proto, output_proto)


class SetBinhostTest(cros_test_lib.MockTestCase):
  """Unittests for SetBinhost."""

  def testSetBinhost(self):
    """SetBinhost calls service with correct args."""
    set_binhost = self.PatchObject(binhost_service, 'SetBinhost',
                                   return_value='/path/to/BINHOST.conf')

    input_proto = binhost_pb2.SetBinhostRequest()
    input_proto.build_target.name = 'target'
    input_proto.private = True
    input_proto.key = binhost_pb2.POSTSUBMIT_BINHOST
    input_proto.uri = 'gs://chromeos-prebuilt/target'

    output_proto = binhost_pb2.SetBinhostResponse()

    binhost.SetBinhost(input_proto, output_proto)

    self.assertEqual(output_proto.output_file, '/path/to/BINHOST.conf')
    set_binhost.assert_called_once_with(
        'target', 'PARALLEL_POSTSUBMIT_BINHOST',
        'gs://chromeos-prebuilt/target', private=True)

class RegenBuildCacheTest(cros_test_lib.MockTestCase):
  """Unittests for RegenBuildCache."""

  def testRegenBuildCache(self):
    """RegenBuildCache calls service with the correct args."""
    regen_cache = self.PatchObject(binhost_service, 'RegenBuildCache')

    input_proto = binhost_pb2.RegenBuildCacheRequest()
    input_proto.overlay_type = binhost_pb2.OVERLAYTYPE_BOTH
    output_proto = binhost_pb2.RegenBuildCacheResponse()

    binhost.RegenBuildCache(input_proto, output_proto)
    regen_cache.assert_called_once_with('both')

  def testRequiresOverlayType(self):
    """RegenBuildCache dies if overlay_type not specified."""
    regen_cache = self.PatchObject(binhost_service, 'RegenBuildCache')
    die = self.PatchObject(cros_build_lib, 'Die')

    input_proto = binhost_pb2.RegenBuildCacheRequest()
    input_proto.overlay_type = binhost_pb2.OVERLAYTYPE_UNSPECIFIED
    output_proto = binhost_pb2.RegenBuildCacheResponse()

    binhost.RegenBuildCache(input_proto, output_proto)
    die.assert_called_once()
    regen_cache.assert_not_called()
