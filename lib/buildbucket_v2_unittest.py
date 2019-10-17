# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for buildbucket_v2."""

from __future__ import print_function

from datetime import datetime, date

from google.protobuf import field_mask_pb2
from google.protobuf.struct_pb2 import Struct, Value
from google.protobuf.timestamp_pb2 import Timestamp

from chromite.lib import buildbucket_v2
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import cros_logging as logging
from chromite.lib import metadata_lib
from chromite.lib.luci.prpc.client import Client, ProtocolError

from infra_libs.buildbucket.proto import build_pb2, rpc_pb2, common_pb2
from infra_libs.buildbucket.proto import step_pb2

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

  def testGetBuildStages(self):
    """Test the GetBuildStages functionality."""
    bbv2 = buildbucket_v2.BuildbucketV2()
    start_time = Timestamp()
    start_time.GetCurrentTime()
    step = step_pb2.Step(name='stage_name', start_time=start_time,
                         status=2)
    build_with_steps = build_pb2.Build(steps=[step])
    get_build_fn = self.PatchObject(bbv2, 'GetBuild',
                                    return_value=build_with_steps)
    get_build_status_fn = self.PatchObject(
        bbv2, 'GetBuildStatus',
        return_value={'build_config': 'something-paladin'})
    expected_result = [{
        'name': 'stage_name',
        'start_time': datetime.fromtimestamp(start_time.seconds),
        'finish_time': None,
        'buildbucket_id': 1234,
        'status': constants.BUILDER_STATUS_INFLIGHT,
        'build_config': 'something-paladin'}]
    self.assertEqual(bbv2.GetBuildStages(1234), expected_result)
    get_build_fn.assert_called_once_with(1234, properties='steps')
    get_build_status_fn.assert_called_once_with(1234)

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

  def testGetBuildStatusWithValidId(self):
    """Tests for GetBuildStatus with a valid ID."""
    properties_dict = {
        'cidb_id': '1234',
        'bot_id': 'swarm-cros-34',
        'cbb_branch': 'master',
        'cbb_config': 'sludge-paladin-tryjob',
        'cbb_master_build_id': '4321',
        'platform_version':'11721.0.0',
        'milestone_version': '74',
        'full_version': 'R74-11721.0.0-b3457724',
        'critical': '1',
        'build_type': 'Try',
    }
    start_time = Timestamp()
    start_time.GetCurrentTime()
    fake_properties = Struct(fields={
        key: Value(string_value=value) for key, value in properties_dict.items()
    })
    fake_output = build_pb2.Build.Output(properties=fake_properties)
    fake_build = build_pb2.Build(
        id=1234, start_time=start_time, status=2, output=fake_output)
    self.PatchObject(buildbucket_v2.BuildbucketV2, 'GetBuild',
                     return_value=fake_build)
    expected_valid_status = {
        'build_config': 'sludge-paladin-tryjob',
        'start_time': datetime.fromtimestamp(start_time.seconds),
        'finish_time': None,
        'id': 1234,
        'status': constants.BUILDER_STATUS_INFLIGHT,
        'chrome_version': None,
        'platform_version':'11721.0.0',
        'milestone_version': '74',
        'full_version': 'R74-11721.0.0-b3457724',
        'important': 1,
        'buildbucket_id': 1234,
        'summary': None,
        'master_build_id': 4321,
        'bot_hostname': 'swarm-cros-34',
        'builder_name': None,
        'build_number': None,
        'buildbot_generation': None,
        'waterfall': None,
        'deadline': None,
        'build_type': 'Try',
        'metadata_url': None,
        'toolchain_url': None,
        'branch': 'master'
    }
    bbv2 = buildbucket_v2.BuildbucketV2()
    status = bbv2.GetBuildStatus(1234)
    self.assertEqual(status, expected_valid_status)

  def testGetBuildStatusWithInvalidId(self):
    """Test the function for an ID that doesn't exist in Buildbucket."""
    expected_invalid_status = {
        'build_config': None,
        'start_time': None,
        'finish_time': None,
        'status': None,
        'id': None,
        'chrome_version': None,
        'platform_version': None,
        'milestone_version': None,
        'full_version': None,
        'important': None,
        'buildbucket_id': 0,
        'summary': None,
        'master_build_id': None,
        'bot_hostname': None,
        'builder_name': None,
        'build_number': None,
        'buildbot_generation': None,
        'waterfall': None,
        'deadline': None,
        'build_type': None,
        'metadata_url': None,
        'toolchain_url': None,
        'branch': None
    }
    self.PatchObject(buildbucket_v2.BuildbucketV2, 'GetBuild',
                     side_effect=ProtocolError)
    bbv2 = buildbucket_v2.BuildbucketV2()
    status = bbv2.GetBuildStatus(0)
    self.assertEqual(status, expected_invalid_status)

  def testSearchBuildExceptionCases(self):
    """Test scenarios where SearchBuild raises an Exception."""
    bbv2 = buildbucket_v2.BuildbucketV2()
    builder = build_pb2.BuilderID(project='chromeos', bucket='general')
    tag = common_pb2.StringPair(key='cbb_master_buildbucket_id',
                                value=str(1234))
    build_predicate = rpc_pb2.BuildPredicate(
        builder=builder, tags=[tag])
    with self.assertRaises(AssertionError):
      bbv2.SearchBuild(None, None, 100)
    with self.assertRaises(AssertionError):
      bbv2.SearchBuild(build_predicate, None, None)
    with self.assertRaises(AssertionError):
      bbv2.SearchBuild(build_predicate, 'str_fields', 100)

  def testSearchBuild(self):
    """Test redirection to the underlying RPC call."""
    bbv2 = buildbucket_v2.BuildbucketV2()
    builder = build_pb2.BuilderID(project='chromeos', bucket='general')
    tag = common_pb2.StringPair(key='cbb_master_buildbucket_id',
                                value=str(1234))
    build_predicate = rpc_pb2.BuildPredicate(
        builder=builder, tags=[tag])
    fields = field_mask_pb2.FieldMask()
    search_builds_fn = self.PatchObject(bbv2.client, 'SearchBuilds')
    bbv2.SearchBuild(build_predicate, fields=fields, page_size=123)
    search_builds_fn.assert_called_once_with(rpc_pb2.SearchBuildsRequest(
        predicate=build_predicate, fields=fields, page_size=123))

  def testGetBuildHistoryIgnoreIdFoundId(self):
    """Test GetBuildHistory ignore_build_id logic."""
    # pylint: disable=unused-variable
    bbv2 = buildbucket_v2.BuildbucketV2()
    builder = build_pb2.BuilderID(project='chromeos', bucket='general')
    tags = [common_pb2.StringPair(key='cbb_config',
                                  value='something-paladin')]
    build_list = [build_pb2.Build(id=1234),
                  build_pb2.Build(id=2341)]
    search_fn = self.PatchObject(
        bbv2, 'SearchBuild',
        return_value=rpc_pb2.SearchBuildsResponse(builds=build_list))
    status_fn = self.PatchObject(bbv2, 'GetBuildStatus')
    bbv2.GetBuildHistory('something-paladin', 1, ignore_build_id=1234)
    fake_predicate = rpc_pb2.BuildPredicate(
        builder=builder, tags=tags, create_time=None, build=None)
    search_fn.assert_called_once_with(fake_predicate, page_size=2)
    status_fn.assert_called_once_with(2341)

  def testGetBuildHistoryIgnoreIdWithoutId(self):
    """Test GetBuildHistory ignore_build_id logic when ID is absent."""
    bbv2 = buildbucket_v2.BuildbucketV2()
    builder = build_pb2.BuilderID(project='chromeos', bucket='general')
    tags = [common_pb2.StringPair(key='cbb_config',
                                  value='something-paladin')]
    build_list = [build_pb2.Build(id=1234),
                  build_pb2.Build(id=2341)]
    search_fn = self.PatchObject(
        bbv2, 'SearchBuild',
        return_value=rpc_pb2.SearchBuildsResponse(builds=build_list))
    status_fn = self.PatchObject(bbv2, 'GetBuildStatus')
    bbv2.GetBuildHistory('something-paladin', 1, ignore_build_id=1000)
    fake_predicate = rpc_pb2.BuildPredicate(
        builder=builder, tags=tags, create_time=None, build=None)
    search_fn.assert_called_with(fake_predicate, page_size=2)
    status_fn.assert_called_with(1234)

  def testGetBuildHistoryOtherArgs(self):
    """Test GetBuildHistory's processing of (args - ignore_build_id)."""
    builder = build_pb2.BuilderID(project='chromeos', bucket='general')
    tags = [common_pb2.StringPair(key='cbb_config',
                                  value='something-paladin'),
            common_pb2.StringPair(key='cbb_branch',
                                  value='master')]
    start_date = date(2019, 4, 16)
    create_time = buildbucket_v2.DateToTimeRange(start_date)
    fake_build = rpc_pb2.BuildRange(start_build_id=1234)
    fake_predicate = rpc_pb2.BuildPredicate(
        builder=builder, tags=tags, create_time=create_time, build=fake_build)
    bbv2 = buildbucket_v2.BuildbucketV2()
    search_fn = self.PatchObject(bbv2, 'SearchBuild')
    self.PatchObject(bbv2, 'GetBuildStatus')
    bbv2.GetBuildHistory('something-paladin', 10, start_date=start_date,
                         branch='master', start_build_id=1234)
    search_fn.assert_called_once_with(fake_predicate, page_size=10)

  def testGetChildStatusesSuccess(self):
    """Test GetChildStatuses when RPC succeeds."""
    bbv2 = buildbucket_v2.BuildbucketV2()
    fake_search_build_response = rpc_pb2.SearchBuildsResponse()
    self.PatchObject(bbv2, 'SearchBuild',
                     return_value=fake_search_build_response)
    self.assertEqual(bbv2.GetChildStatuses(1234), [])
    fake_build = build_pb2.Build(id=2341)
    fake_search_build_response = rpc_pb2.SearchBuildsResponse(
        builds=[fake_build])
    self.PatchObject(bbv2, 'SearchBuild',
                     return_value=fake_search_build_response)
    get_build_status = self.PatchObject(bbv2, 'GetBuildStatus')
    bbv2.GetChildStatuses(1234)
    get_build_status.assert_called_once_with(2341)

  def testGetStageFailures(self):
    """Test GetStageFailures logic."""
    bbv2 = buildbucket_v2.BuildbucketV2()
    fake_build_status = {'buildbucket_id': 1234,
                         'status': 'Passed!!',
                         'build_config': 'something-paladin',
                         'important': True}
    fake_stages = [
        {'name': 'stage_1', 'status':constants.BUILDER_STATUS_PASSED},
        {'name': 'stage_2', 'status':constants.BUILDER_STATUS_FAILED},
        {'name': 'stage_3', 'status':constants.BUILDER_STATUS_FORGIVEN},
    ]
    self.PatchObject(bbv2, 'GetBuildStatus', return_value=fake_build_status)
    self.PatchObject(bbv2, 'GetBuildStages', return_value=fake_stages)
    expected_answer = [
        {'stage_name': 'stage_2',
         'stage_status': constants.BUILDER_STATUS_FAILED,
         'buildbucket_id': 1234,
         'build_config': 'something-paladin',
         'build_status': 'Passed!!',
         'important': True}
    ]
    self.assertEqual(bbv2.GetStageFailures(1234), expected_answer)


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

  def testBuildStepToDict(self):
    """Test the working of BuildStepToDict."""
    build_values = {
        'buildbucket_id': 1234,
        'build_config': 'something-paladin'}
    start_time = Timestamp()
    start_time.GetCurrentTime()
    step = step_pb2.Step(name='stage_name', start_time=start_time,
                         status=2)
    expected_result = {
        'name': 'stage_name',
        'start_time': datetime.fromtimestamp(start_time.seconds),
        'finish_time': None,
        'buildbucket_id': 1234,
        'status': constants.BUILDER_STATUS_INFLIGHT,
        'build_config': 'something-paladin'}

    self.assertEqual(buildbucket_v2.BuildStepToDict(step, build_values),
                     expected_result)

  def testDateToTimeRangeNoneInput(self):
    self.assertIsNone(buildbucket_v2.DateToTimeRange(None))

  def testDateToTimeRangeStartDate(self):
    date_example = date(2019, 4, 15)
    result = buildbucket_v2.DateToTimeRange(start_date=date_example)
    self.assertEqual(result.start_time.seconds, 1555286400)
    self.assertEqual(result.end_time.seconds, 0)

  def testDateToTimeRangeEndDate(self):
    date_example = date(2019, 4, 15)
    result = buildbucket_v2.DateToTimeRange(end_date=date_example)
    self.assertEqual(result.end_time.seconds, 1555372740)
    self.assertEqual(result.start_time.seconds, 0)
