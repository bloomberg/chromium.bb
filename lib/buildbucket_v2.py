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

from ssl import SSLError
import ast
import socket

from google.protobuf import field_mask_pb2

from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import retry_util
from chromite.lib.luci import utils
from chromite.lib.luci.prpc.client import Client, ProtocolError

from infra_libs.buildbucket.proto import rpc_pb2, build_pb2, common_pb2
from infra_libs.buildbucket.proto.rpc_prpc_pb2 import BuildsServiceDescription

BBV2_URL_ENDPOINT_PROD = (
    'cr-buildbucket.appspot.com'
)
BBV2_URL_ENDPOINT_TEST = (
    'cr-buildbucket-test.appspot.com'
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
    critical=None, cidb_id=None, chrome_version=None, milestone_version=None,
    platform_version=None, full_version=None, toolchain_url=None,
    build_type=None, unibuild=None, suite_scheduling=None,
    killed_child_builds=None, board=None, main_firmware_version=None,
    ec_firmware_version=None, metadata_url=None):
  """Update build.output.properties for the current build.

  Sends the property values to buildbucket via
  UpdateSelfBuildPropertiesNonBlocking. All arguments are optional.

  Args:
    critical: (Optional) |important| flag of the build.
    cidb_id: (Optional) CIDB ID of the build.
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
    metadata_url: (Optional) google storage url to metadata.json of the build.
  """
  if critical is not None:
    critical = 1 if critical in [1, True] else 0
    UpdateSelfBuildPropertiesNonBlocking('critical', critical)
  if cidb_id is not None:
    UpdateSelfBuildPropertiesNonBlocking('cidb_id', cidb_id)
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
  if metadata_url is not None:
    UpdateSelfBuildPropertiesNonBlocking('metadata_url', metadata_url)

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

def BuildStepToDict(step, build_values=None):
  """Extract information from a Buildbucket Step instance.

  Reference:
  https://chromium.googlesource.com/infra/luci/luci-go/+/master/buildbucket/proto/step.proto

  Args:
    step: A step_pb2.Step instance from Buildbucket to be extracted.
    build_values: A dictionary of values to be included in the result.

  Returns:
    A dictionary with the following keys: name, status, start_time and end_time
    in addition to the keys in build_values.
  """
  step_info = {'name': step.name}
  if step.status is not None and step.status in BB_STATUS_DICT:
    step_info['status'] = BB_STATUS_DICT[step.status]
  else:
    step_info['status'] = None
  step_info['start_time'] = utils.TimestampToDatetime(step.start_time)
  step_info['finish_time'] = utils.TimestampToDatetime(step.end_time)

  if build_values:
    step_info.update(build_values)
  return step_info

def DateToTimeRange(start_date=None, end_date=None):
  """Convert two datetime.date objects into a TimeRange instance.

  Args:
    start_date: datetime.date instance to mark the start of the TimeRange.
    end_date: datetime.date instance to mark the end of the TimeRange.

  Returns:
    A TimeRange object corresponding to the time interval
    (start_date, end_date).
  """
  if not (start_date or end_date):
    return None
  if start_date:
    start_timestamp = utils.DatetimeToTimestamp(start_date)
  else:
    start_timestamp = None
  if end_date:
    end_timestamp = utils.DatetimeToTimestamp(end_date, end_of_day=True)
  else:
    end_timestamp = None
  return common_pb2.TimeRange(start_time=start_timestamp,
                              end_time=end_timestamp)

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

  @retry_util.WithRetry(max_retry=3, sleep=0.2, exception=SSLError)
  @retry_util.WithRetry(max_retry=3, sleep=0.2, exception=socket.error)
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
          build_properties['killed_child_builds'] is not None):
        return ast.literal_eval(build_properties['killed_child_builds'])

  def GetBuildStages(self, buildbucket_id):
    """Get all the Recipe steps/CBuildbot stages of a build.

    Args:
      buildbucket_id: ID of the build in buildbucket.

    Returns:
      A dictionary with keys (id, name, status, last_updated,
      start_time, finish_time).
    """
    properties = 'steps'
    build_with_steps = self.GetBuild(buildbucket_id, properties=properties)
    build_status = self.GetBuildStatus(buildbucket_id)
    build_values = {'buildbucket_id': buildbucket_id}
    build_values['build_config'] = build_status['build_config']
    return [BuildStepToDict(step, build_values)
            for step in build_with_steps.steps]

  def GetBuildStatus(self, buildbucket_id):
    """Retrieve the build status for build corresponding to buildbucket_id.

    Args:
      buildbucket_id: Buildbucket ID of the build to be queried for.

    Returns:
      A Dictionary with keys (build_config, start_time, finish_time, status,
      platform_version, full_version, milestone_version, critical,
      buildbucket_id, summary, master_build_id, bot_hostname,
      deadline, build_type, metadata_url, toolchain_url, branch).
    """
    CIDB_TO_BB_PROPERTIES_MAP = {
        # A mapping of CIDB property names to their Buildbucket v2 equivalents.
        'bot_id': 'bot_hostname',
        'cidb_id': 'id',
        'cbb_branch': 'branch',
        'cbb_config': 'build_config',
        'cbb_master_build_id': 'master_build_id',
        'chrome_version': 'chrome_version',
        'platform_version': 'platform_version',
        'full_version': 'full_version',
        'milestone_version': 'milestone_version',
        'toolchain_url': 'toolchain_url',
        'metadata_url': 'metadata_url',
        'critical': 'important',
        'build_type': 'build_type',
        'summary': 'summary',
    }
    try:
      properties = 'output.properties'
      build_with_properties = self.GetBuild(buildbucket_id, properties)
      build = self.GetBuild(buildbucket_id)
    except ProtocolError:
      logging.error('Could not fetch Buildbucket status for %d', buildbucket_id)
      status_shell = {
          'buildbucket_id': buildbucket_id,
          'start_time': None,
          'finish_time': None,
          'status': None,
          'builder_name': None,
          'build_number': None,
          'buildbot_generation': None,
          'waterfall': None,
          'metadata_url': None,
          'deadline': None,
      }
      for _, status_name in CIDB_TO_BB_PROPERTIES_MAP.items():
        status_shell[status_name] = None
      return status_shell

    build_status = {'buildbucket_id': build.id}
    build_status['start_time'] = utils.TimestampToDatetime(build.start_time)
    build_status['finish_time'] = utils.TimestampToDatetime(build.end_time)
    build_status['status'] = build.status

    if build_with_properties.output.HasField('properties'):
      build_properties = build_with_properties.output.properties
      for property_name, status_name in CIDB_TO_BB_PROPERTIES_MAP.items():
        if (property_name in build_properties and
            build_properties[property_name] is not None):
          build_status[status_name] = str(build_properties[property_name])
        else:
          build_status[status_name] = None
    else:
      logging.error('Could not fetch Buildbucket properties for %d',
                    buildbucket_id)
      build_status.update({
          'builder_name': None,
          'build_number': None,
          'buildbot_generation': None,
          'waterfall': None,
          'metadata_url': None,
          'deadline': None,
      })
      for _, status_name in CIDB_TO_BB_PROPERTIES_MAP.items():
        build_status[status_name] = None
      return build_status

    # Including the now-defunct columns of CIDB Table so as to not break logic.
    # TODO(dhanyaganesh): remove these one at a time.
    build_status['builder_name'] = None
    build_status['build_number'] = None
    build_status['buildbot_generation'] = None
    build_status['waterfall'] = None
    build_status['deadline'] = None
    # Post-processing some properties.
    if (build_status['status'] is not None and
        build_status['status'] in BB_STATUS_DICT):
      build_status['status'] = BB_STATUS_DICT[build_status['status']]
    if build_status['important'] == 'True':
      build_status['important'] = 1
    for int_property in ['id', 'master_build_id', 'important']:
      if build_status[int_property]:
        try:
          build_status[int_property] = int(build_status[int_property])
        except ValueError:
          build_status[int_property] = None
    return build_status

  @retry_util.WithRetry(max_retry=3, sleep=0.2, exception=SSLError)
  @retry_util.WithRetry(max_retry=3, sleep=0.2, exception=socket.error)
  def SearchBuild(self, build_predicate, fields=None, page_size=100):
    """SearchBuild RPC call wrapping function.

    Args:
      build_predicate: Message containing builder, gerrit_changes and/or
        git_commits among other things.
      fields: A FieldMask instance to dictate the format of the response.
      page_size: Number of builds to return.

    Returns:
      A SearchBuildResponse instance corresponding to the query.
    """
    assert isinstance(build_predicate, rpc_pb2.BuildPredicate)
    if fields is not None:
      assert isinstance(fields, field_mask_pb2.FieldMask)
    assert isinstance(page_size, int)
    if fields is None:
      search_build_request = rpc_pb2.SearchBuildsRequest(
          predicate=build_predicate, page_size=page_size)
    else:
      search_build_request = rpc_pb2.SearchBuildsRequest(
          predicate=build_predicate, fields=fields, page_size=page_size)

    return self.client.SearchBuilds(search_build_request)

  def GetBuildHistory(self, build_config, num_results, ignore_build_id=None,
                      start_date=None, end_date=None, branch=None,
                      start_build_id=None):
    """Returns basic information about most recent builds for build config.

    By default this function returns the most recent builds. Some arguments can
    restrict the result to older builds.

    Args:
      build_config: config name of the build to get history.
      num_results: Number of builds to search back. Set this to
          CIDBConnection.NUM_RESULTS_NO_LIMIT to request no limit on the number
          of results.
      ignore_build_id: (Optional) Ignore a specific build. This is most useful
          to ignore the current build when querying recent past builds from a
          build in flight.
      start_date: (Optional, type: datetime.date) Get builds that occured on or
          after this date.
      end_date: (Optional, type:datetime.date) Get builds that occured on or
          before this date.
      branch: (Optional) Return only results for this branch.
      start_build_id: (Optional) The oldest build for which data should
          be retrieved.

    Returns:
      A sorted list of dicts containing up to |number| dictionaries for
      build statuses in descending order (if |reverse| is True, ascending
      order).
    """
    builder = build_pb2.BuilderID(project='chromeos', bucket='general')
    tags = [common_pb2.StringPair(key='cbb_config',
                                  value=build_config)]
    create_time = DateToTimeRange(start_date, end_date)
    if ignore_build_id:
      num_results += 1
    if branch:
      tags.append(common_pb2.StringPair(key='cbb_branch',
                                        value=branch))
    build = None
    if start_build_id:
      build = rpc_pb2.BuildRange(start_build_id=int(start_build_id))
    build_predicate = rpc_pb2.BuildPredicate(
        builder=builder, tags=tags, create_time=create_time, build=build)
    search_result = self.SearchBuild(build_predicate, page_size=num_results)
    build_ids = [build.id for build in search_result.builds]
    if ignore_build_id:
      if ignore_build_id in build_ids:
        build_ids = [x for x in build_ids if ignore_build_id != x]
      else:
        # If we do not find ignore_build_id, we ignore the last (i.e. oldest)
        # build in order to return num_results elements.
        build_ids = build_ids[:-1]

    return [self.GetBuildStatus(x) for x in build_ids]

  def GetChildStatuses(self, buildbucket_id):
    """Retrieve statuses of all the child builds.

    Args:
      buildbucket_id: buildbucket_id of the parent/master build.

    Returns:
      A list of dictionary corresponding to each child build with keys like
      start_time, end_time, status, version info, critical, build_config, etc.
    """
    builder = build_pb2.BuilderID(project='chromeos', bucket='general')
    tag = common_pb2.StringPair(key='cbb_master_buildbucket_id',
                                value=str(buildbucket_id))
    build_predicate = rpc_pb2.BuildPredicate(
        builder=builder, tags=[tag])
    child_builds = self.SearchBuild(build_predicate, page_size=200)
    return [self.GetBuildStatus(child_build.id)
            for child_build in child_builds.builds]

  def GetStageFailures(self, buildbucket_id):
    """Report on the failed stages of a build.

    Args:
      buildbucket_id: buildbucket_id of the build to query for.

    Returns:
      A list of dictionary corresponding to each failed stage with following
      keys - stage_name, stage_status, buildbucket_id, build_config,
      build_status, important.
    """
    build_status = self.GetBuildStatus(buildbucket_id)
    stage_status = self.GetBuildStages(buildbucket_id)

    build_info = {
        'build_status': build_status['status'],
        'build_config': build_status['build_config'],
        'important': build_status['important'],
        'buildbucket_id': buildbucket_id,
    }

    stage_failures = []
    for stage in stage_status:
      if stage['status'] == constants.BUILDER_STATUS_FAILED:
        failed_stage_info = {'stage_status': constants.BUILDER_STATUS_FAILED}
        failed_stage_info['stage_name'] = stage['name']
        failed_stage_info.update(build_info)
        stage_failures.append(failed_stage_info)

    return stage_failures
