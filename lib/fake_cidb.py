# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fake CIDB for unit testing."""

from __future__ import print_function

import datetime

from chromite.lib import constants
from chromite.lib import cidb
from chromite.lib import failure_message_lib


class FakeCIDBConnection(object):
  """Fake connection to a Continuous Integration database.

  This class is a partial re-implementation of CIDBConnection, using
  in-memory lists rather than a backing database.
  """

  NUM_RESULTS_NO_LIMIT = -1

  def __init__(self):
    self.buildTable = []
    self.buildStageTable = {}
    self.failureTable = {}
    self.fake_time = None
    self.buildMessageTable = {}

  def _TrimStatus(self, status):
    """Trims a build row to keys that should be returned by GetBuildStatus"""
    return {k: v for (k, v) in status.items()
            if k in cidb.CIDBConnection.BUILD_STATUS_KEYS}

  def SetTime(self, fake_time):
    """Sets a fake time to be retrieved by GetTime.

    Args:
      fake_time: datetime.datetime object.
    """
    self.fake_time = fake_time

  def GetTime(self):
    """Gets the current database time."""
    return self.fake_time or datetime.datetime.now()

  def InsertBuild(self, builder_name, build_number,
                  build_config, bot_hostname, master_build_id=None,
                  timeout_seconds=None, status=constants.BUILDER_STATUS_PASSED,
                  important=None, buildbucket_id=None, milestone_version=None,
                  platform_version=None, start_time=None, build_type=None,
                  branch=None):
    """Insert a build row.

    Note this API slightly differs from cidb as we pass status to avoid having
    to have a later FinishBuild call in testing.
    """
    if start_time is None:
      start_time = datetime.datetime.now()

    deadline = None
    if timeout_seconds is not None:
      timediff = datetime.timedelta(seconds=timeout_seconds)
      deadline = start_time + timediff

    build_id = len(self.buildTable)
    row = {'id': build_id,
           'builder_name': builder_name,
           'buildbot_generation': constants.BUILDBOT_GENERATION,
           # While waterfall is nullable all non waterfall entries show empty
           # string, sticking to the convention.
           'waterfall': '',
           'build_number': build_number,
           'build_config' : build_config,
           'bot_hostname': bot_hostname,
           'start_time': start_time,
           'master_build_id' : master_build_id,
           'deadline': deadline,
           'status': status,
           'finish_time': start_time,
           'important': important,
           'buildbucket_id': buildbucket_id,
           'final': False,
           'milestone_version': milestone_version,
           'platform_version': platform_version,
           'build_type': build_type,
           'branch': branch}
    self.buildTable.append(row)
    return build_id

  def FinishBuild(self, build_id, status=None, summary=None, strict=True):
    """Update the build with finished status."""
    build = self.buildTable[build_id]

    if strict and build['final']:
      return 0

    values = {}
    if status is not None:
      values.update(status=status)
    if summary is not None:
      values.update(summary=summary)

    values.update(finish_time=datetime.datetime.now(), final=True)

    if values:
      build.update(values)
      return 1
    else:
      return 0

  def UpdateMetadata(self, build_id, metadata):
    """See cidb.UpdateMetadata.

    Args:
      build_id: The build to update.
      metadata: A cbuildbot metadata object. Or, a dictionary (note: using
                a dictionary is not supported by the base cidb API, but
                is provided for this fake class for ease of use in test
                set-up code).
    """
    d = metadata if isinstance(metadata, dict) else metadata.GetDict()
    versions = d.get('version') or {}
    self.buildTable[build_id].update(
        {'chrome_version': versions.get('chrome'),
         'milestone_version': versions.get('milestone'),
         'platform_version': versions.get('platform'),
         'full_version': versions.get('full'),
         'sdk_version': d.get('sdk-versions'),
         'toolchain_url': d.get('toolchain-url'),
         'build_type': d.get('build_type'),
         'important': d.get('important')})
    return 1

  def InsertBuildStage(self, build_id, name, board=None,
                       status=constants.BUILDER_STATUS_PLANNED):
    build_stage_id = len(self.buildStageTable)
    row = {'build_id': build_id,
           'name': name,
           'board': board,
           'status': status}
    self.buildStageTable[build_stage_id] = row
    return build_stage_id

  def InsertBoardPerBuild(self, build_id, board):
    # TODO(akeshet): Fill this placeholder.
    pass

  def InsertBuildMessage(self, build_id,
                         message_type=None, message_subtype=None,
                         message_value=None, board=None):
    """Insert a build message.

    Args:
      build_id: primary key of build recording this message.
      message_type: Optional str name of message type.
      message_subtype: Optional str name of message subtype.
      message_value: Optional value of message.
      board: Optional str name of the board.

    Returns:
      The build message id (string).
    """
    if message_type:
      message_type = message_type[:240]
    if message_subtype:
      message_subtype = message_subtype[:240]
    if message_value:
      message_value = message_value[:480]
    if board:
      board = board[:240]

    build_message_id = len(self.buildMessageTable)
    values = {'build_id': build_id,
              'message_type': message_type,
              'message_subtype': message_subtype,
              'message_value': message_value,
              'board': board}
    self.buildMessageTable[build_message_id] = values
    return build_message_id

  def GetBuildMessages(self, build_id, message_type=None, message_subtype=None):
    """Get the build messages of the given build id.

    Args:
      build_id: build id (string) of the build to get messages.
      message_type: Get messages with the specific message_type (string) if
        message_type is not None.
      message_subtype: Get messages with the specific message_subtype (stirng)
        if message_subtype is not None.

    Returns:
      A list of build messages (in the format of dict).
    """
    messages = []
    for v in self.buildMessageTable.values():
      if (v['build_id'] == build_id and
          (message_type is None or v['message_type'] == message_type) and
          (message_subtype is None or
           v['message_subtype'] == message_subtype)):
        messages.append(v)

    return messages

  def StartBuildStage(self, build_stage_id):
    if build_stage_id > len(self.buildStageTable):
      return

    self.buildStageTable[build_stage_id]['status'] = (
        constants.BUILDER_STATUS_INFLIGHT)

  def WaitBuildStage(self, build_stage_id):
    if build_stage_id > len(self.buildStageTable):
      return

    self.buildStageTable[build_stage_id]['status'] = (
        constants.BUILDER_STATUS_WAITING)

  def FinishBuildStage(self, build_stage_id, status):
    if build_stage_id > len(self.buildStageTable):
      return

    self.buildStageTable[build_stage_id]['status'] = status

  def GetBuildStatus(self, build_id):
    """Gets the status of the build."""
    try:
      return self._TrimStatus(self.buildTable[build_id])
    except IndexError:
      return None

  def GetBuildStatuses(self, build_ids):
    """Gets the status of the builds."""
    return [self.GetBuildStatus(x) for x in build_ids]

  def GetSlaveStatuses(self, master_build_id, buildbucket_ids=None):
    """Gets the slaves of given build."""
    if buildbucket_ids is None:
      return [self._TrimStatus(b) for b in self.buildTable
              if b['master_build_id'] == master_build_id]
    else:
      return [self._TrimStatus(b) for b in self.buildTable
              if b['master_build_id'] == master_build_id and
              b['buildbucket_id'] in buildbucket_ids]

  def GetBuildsStages(self, build_ids):
    """Quick implementation of fake GetBuildsStages."""
    build_stages = []
    build_statuses = {b['id']: b for b in self.buildTable
                      if b['id'] in build_ids}
    for _id in self.buildStageTable:
      build_id = self.buildStageTable[_id]['build_id']
      if build_id in build_ids:
        stage = self.buildStageTable[_id].copy()
        stage['build_config'] = build_statuses[build_id]['build_config']
        build_stages.append(stage)

    return build_stages

  def GetBuildsStagesWithBuildbucketIds(self, buildbucket_ids):
    """Quick implementation of fake GetBuildsStagesWithBuildbucketIds."""
    build_stages = []
    build_statuses = {b['id']: b for b in self.buildTable
                      if b['buildbucket_id'] in buildbucket_ids}
    for _id in self.buildStageTable:
      build_id = self.buildStageTable[_id]['build_id']
      if build_id in build_statuses:
        stage = self.buildStageTable[_id].copy()
        stage['build_config'] = build_statuses[build_id]['build_config']
        build_stages.append(stage)

    return build_stages

  # pylint: disable=unused-argument
  def GetBuildHistory(self, build_config, num_results,
                      ignore_build_id=None, start_date=None, end_date=None,
                      branch=None, milestone_version=None,
                      platform_version=None, starting_build_id=None,
                      ending_build_id=None, final=False, reverse=False):
    """Returns the build history for the given |build_config|."""
    return self.GetBuildsHistory(
        build_configs=[build_config], num_results=num_results,
        ignore_build_id=ignore_build_id, start_date=start_date,
        end_date=end_date, milestone_version=milestone_version,
        platform_version=platform_version, starting_build_id=starting_build_id,
        final=final, reverse=reverse)

  def GetBuildsHistory(self, build_configs, num_results,
                       ignore_build_id=None, start_date=None, end_date=None,
                       milestone_version=None,
                       platform_version=None, starting_build_id=None,
                       final=False, reverse=False):
    """Returns the build history for the given |build_configs|."""
    builds = sorted(self.buildTable, reverse=(not reverse),
                    key=lambda x: x['id'])

    # Filter results.
    if build_configs:
      builds = [b for b in builds if b['build_config'] in build_configs]
    if ignore_build_id is not None:
      builds = [b for b in builds if b['buildbucket_id'] != ignore_build_id]
    if start_date is not None:
      builds = [b for b in builds
                if b['start_time'].date() >= start_date]
    if end_date is not None:
      builds = [b for b in builds
                if 'finish_time' in b and
                b['finish_time'] and
                b['finish_time'].date() <= end_date]
    if milestone_version is not None:
      builds = [b for b in builds
                if b.get('milestone_version') == milestone_version]
    if platform_version is not None:
      builds = [b for b in builds
                if b.get('platform_version') == platform_version]
    if starting_build_id is not None:
      builds = [b for b in builds if b['id'] >= starting_build_id]
    if final:
      builds = [b for b in builds
                if b.get('final') is True]

    if num_results != -1:
      return builds[:num_results]
    else:
      return builds

  def GetBuildStatusesWithBuildbucketIds(self, buildbucket_ids):
    rows = []
    for row in self.buildTable:
      if (row['buildbucket_id'] in buildbucket_ids or
          str(row['buildbucket_id']) in buildbucket_ids):
        rows.append(self._TrimStatus(row))

    return rows

  def GetBuildsFailures(self, build_ids):
    """Gets the failure entries for all listed build_ids.

    Args:
      build_ids: list of build ids of the builds to fetch failures for.

    Returns:
      A list of failure_message_lib.StageFailure instances.
    """
    stage_failures = []
    for build_id in build_ids:
      b_dict = self.buildTable[build_id]
      bs_table = {k: v for k, v in self.buildStageTable.items()
                  if v['build_id'] == build_id}

      for f_dict in self.failureTable.values():
        if f_dict['build_stage_id'] in bs_table:
          bs_dict = bs_table[f_dict['build_stage_id']]
          stage_failures.append(
              failure_message_lib.StageFailure.GetStageFailureFromDicts(
                  f_dict, bs_dict, b_dict))

    return stage_failures

  def UpdateBoardPerBuildMetadata(self, build_id, board, board_metadata):
    """Update the given board-per-build metadata.

    This function is not being tested. A function stub to spare a
    unittest error.
    """
