# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for buildbucket_v2."""

from __future__ import print_function

from google.protobuf import field_mask_pb2
from google.protobuf.struct_pb2 import Struct, Value

from chromite.lib import buildbucket_v2
from chromite.lib import cros_test_lib
from chromite.lib import cros_logging as logging
from chromite.lib import metadata_lib
from chromite.lib.luci.prpc.client import Client, ProtocolError

from infra_libs.buildbucket.proto import build_pb2, rpc_pb2

class BuildbucketV2Test(cros_test_lib.MockTestCase):
  """Tests for buildbucket_v2."""
  # pylint: disable=attribute-defined-outside-init

  def testCreatesClient(self):
    ret = buildbucket_v2.BuildbucketV2(test_env=True)
    self.assertIsInstance(ret.client, Client)

  def testGetBuildWithProperties(self):
    fake_field_mask = field_mask_pb2.FieldMask(paths=['properties'])
    fake_get_build_request = object()
    bbv2 = buildbucket_v2.BuildbucketV2()
    client = bbv2.client
    self.get_build_request_fn = self.PatchObject(
        rpc_pb2, 'GetBuildRequest', return_value=fake_get_build_request)
    self.get_build_function = self.PatchObject(client, 'GetBuild')
    bbv2.GetBuild('some-id', 'properties')
    self.get_build_request_fn.assert_called_with(id='some-id',
                                                 fields=fake_field_mask)
    self.get_build_function.assert_called_with(fake_get_build_request)

  def testGetBuildWithoutProperties(self):
    fake_get_build_request = object()
    bbv2 = buildbucket_v2.BuildbucketV2()
    client = bbv2.client
    self.get_build_request_fn = self.PatchObject(
        rpc_pb2, 'GetBuildRequest', return_value=fake_get_build_request)
    self.get_build_function = self.PatchObject(client, 'GetBuild')
    bbv2.GetBuild('some-id')
    self.get_build_request_fn.assert_called_with(id='some-id')
    self.get_build_function.assert_called_with(fake_get_build_request)

  def testGetKilledChildBuildsWithValidId(self):
    """Test a valid query flow."""
    bbv2 = buildbucket_v2.BuildbucketV2()
    buildbucket_id = 1234
    expected_child_builds = [8921795536486453568, 8921795536486453567]
    fake_properties = Struct(fields={
        'killed_child_builds': Value(string_value=str(expected_child_builds))
    })
    fake_output = build_pb2.Build.Output(properties=fake_properties)
    fake_build = build_pb2.Build(id=1234, output=fake_output)
    self.PatchObject(buildbucket_v2.BuildbucketV2, 'GetBuild',
                     return_value=fake_build)
    builds = bbv2.GetKilledChildBuilds(buildbucket_id)
    self.assertEqual(builds, expected_child_builds)

  def testGetKilledChildBuildsWithInvalidId(self):
    """Test an unsuccessful query."""
    bbv2 = buildbucket_v2.BuildbucketV2()
    buildbucket_id = 1234
    self.PatchObject(buildbucket_v2.BuildbucketV2, 'GetBuild',
                     side_effect=ProtocolError)
    builds = bbv2.GetKilledChildBuilds(buildbucket_id)
    self.assertIsNone(builds)


class StaticFunctionsTest(cros_test_lib.MockTestCase):
  """Test static functions in lib/buildbucket_v2.py."""
  # pylint: disable=attribute-defined-outside-init

  def testUpdateSelfBuildPropertiesNonBlocking(self):
    self.logging_function = self.PatchObject(
        logging, 'PrintKitchenSetBuildProperty')
    buildbucket_v2.UpdateSelfBuildPropertiesNonBlocking('key', 'value')
    self.logging_function.assert_called_with(
        'key', 'value')

  def testUpdateSelfCommonBuildProperties(self):
    self.underlying_function = self.PatchObject(
        buildbucket_v2, 'UpdateSelfBuildPropertiesNonBlocking')
    fake_value = 123
    fake_id = 8921795536486453568
    buildbucket_v2.UpdateSelfCommonBuildProperties(critical=True)
    self.underlying_function.assert_called_with('critical', True)
    buildbucket_v2.UpdateSelfCommonBuildProperties(cidb_id=fake_value)
    self.underlying_function.assert_called_with('cidb_id', fake_value)
    buildbucket_v2.UpdateSelfCommonBuildProperties(
        chrome_version=fake_value)
    self.underlying_function.assert_called_with('chrome_version', fake_value)
    buildbucket_v2.UpdateSelfCommonBuildProperties(
        milestone_version=fake_value)
    self.underlying_function.assert_called_with(
        'milestone_version', fake_value)
    buildbucket_v2.UpdateSelfCommonBuildProperties(
        platform_version=fake_value)
    self.underlying_function.assert_called_with(
        'platform_version', fake_value)
    buildbucket_v2.UpdateSelfCommonBuildProperties(full_version=fake_value)
    self.underlying_function.assert_called_with('full_version', fake_value)
    buildbucket_v2.UpdateSelfCommonBuildProperties(toolchain_url=fake_value)
    self.underlying_function.assert_called_with('toolchain_url', fake_value)
    buildbucket_v2.UpdateSelfCommonBuildProperties(build_type=fake_value)
    self.underlying_function.assert_called_with('build_type', fake_value)
    buildbucket_v2.UpdateSelfCommonBuildProperties(unibuild=True)
    self.underlying_function.assert_called_with('unibuild', True)
    buildbucket_v2.UpdateSelfCommonBuildProperties(suite_scheduling=True)
    self.underlying_function.assert_called_with('suite_scheduling', True)
    buildbucket_v2.UpdateSelfCommonBuildProperties(
        killed_child_builds=[fake_id, fake_value])
    self.underlying_function.assert_called_with('killed_child_builds',
                                                str([fake_id, fake_value]))
    buildbucket_v2.UpdateSelfCommonBuildProperties(board='grunt')
    self.underlying_function.assert_called_with('board', 'grunt')
    buildbucket_v2.UpdateSelfCommonBuildProperties(
        main_firmware_version='Google_Grunt.11031.62.0')
    self.underlying_function.assert_called_with(
        'main_firmware_version', 'Google_Grunt.11031.62.0')
    buildbucket_v2.UpdateSelfCommonBuildProperties(
        ec_firmware_version='aleena_v2.1.108-9ca28c388')
    self.underlying_function.assert_called_with(
        'ec_firmware_version', 'aleena_v2.1.108-9ca28c388')

  def testUpdateBuildMetadata(self):
    fake_dict = {'version': {'chrome': 'chrome_version',
                             'milestone': 'milestone_version',
                             'platform': 'platform_version',
                             'full': 'full_version'},
                 'toolchain-url': 'toolchain_url',
                 'build_type': 'canary',
                 'important': True,
                 'unibuild': True}
    self.PatchObject(metadata_lib.CBuildbotMetadata, 'GetDict',
                     return_value=fake_dict)
    self.PatchObject(buildbucket_v2, 'UpdateSelfCommonBuildProperties')
    fake_metadata = metadata_lib.CBuildbotMetadata()
    buildbucket_v2.UpdateBuildMetadata(fake_metadata)
    buildbucket_v2.UpdateSelfCommonBuildProperties.assert_called_with(
        critical=True, chrome_version='chrome_version',
        milestone_version='milestone_version',
        platform_version='platform_version',
        full_version='full_version',
        toolchain_url='toolchain_url',
        build_type='canary',
        unibuild=True,
        suite_scheduling=False)
