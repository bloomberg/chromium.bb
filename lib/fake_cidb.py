# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fake CIDB for unit testing."""

from __future__ import print_function

import datetime
import itertools

from chromite.lib import constants
from chromite.lib import cidb
from chromite.lib import clactions


class FakeCIDBConnection(object):
  """Fake connection to a Continuous Integration database.

  This class is a partial re-implementation of CIDBConnection, using
  in-memory lists rather than a backing database.
  """

  NUM_RESULTS_NO_LIMIT = -1

  def __init__(self, fake_keyvals=None):
    self.buildTable = []
    self.clActionTable = []
    self.buildStageTable = {}
    self.failureTable = {}
    self.fake_time = None
    self.fake_keyvals = fake_keyvals or {}

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

  def InsertBuild(self, builder_name, waterfall, build_number,
                  build_config, bot_hostname, master_build_id=None,
                  timeout_seconds=None, status=constants.BUILDER_STATUS_PASSED,
                  important=None, buildbucket_id=None):
    """Insert a build row.

    Note this API slightly differs from cidb as we pass status to avoid having
    to have a later FinishBuild call in testing.
    """
    deadline = None
    if timeout_seconds is not None:
      timediff = datetime.timedelta(seconds=timeout_seconds)
      deadline = datetime.datetime.now() + timediff

    build_id = len(self.buildTable)
    row = {'id': build_id,
           'builder_name': builder_name,
           'buildbot_generation': constants.BUILDBOT_GENERATION,
           'waterfall': waterfall,
           'build_number': build_number,
           'build_config' : build_config,
           'bot_hostname': bot_hostname,
           'start_time': datetime.datetime.now(),
           'master_build_id' : master_build_id,
           'deadline': deadline,
           'status': status,
           'finish_time': datetime.datetime.now(),
           'important': important,
           'buildbucket_id': buildbucket_id,
           'final': False}
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

  def InsertCLActions(self, build_id, cl_actions, timestamp=None):
    """Insert a list of |cl_actions|."""
    if not cl_actions:
      return 0

    rows = []
    for cl_action in cl_actions:
      change_number = int(cl_action.change_number)
      patch_number = int(cl_action.patch_number)
      change_source = cl_action.change_source
      action = cl_action.action
      reason = cl_action.reason
      buildbucket_id = cl_action.buildbucket_id

      timestamp = cl_action.timestamp or timestamp or datetime.datetime.now()

      rows.append({
          'build_id' : build_id,
          'change_source' : change_source,
          'change_number': change_number,
          'patch_number' : patch_number,
          'action' : action,
          'timestamp': timestamp,
          'reason' : reason,
          'buildbucket_id': buildbucket_id})

    self.clActionTable.extend(rows)
    return len(rows)

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

  def InsertFailure(self, build_stage_id, exception_type, exception_message,
                    exception_category=constants.EXCEPTION_CATEGORY_UNKNOWN,
                    outer_failure_id=None,
                    extra_info=None):
    failure_id = len(self.failureTable)
    values = {'build_stage_id': build_stage_id,
              'exception_type': exception_type,
              'exception_message': exception_message,
              'exception_category': exception_category,
              'outer_failure_id': outer_failure_id,
              'extra_info': extra_info}
    self.failureTable[failure_id] = values
    return failure_id

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

  def ExtendDeadline(self, build_id, timeout):
    # No sanity checking in fake object.
    now = datetime.datetime.now()
    timediff = datetime.timedelta(seconds=timeout)
    self.buildStageTable[build_id]['deadline'] = now + timediff

  def FinishBuildStage(self, build_stage_id, status):
    if build_stage_id > len(self.buildStageTable):
      return

    self.buildStageTable[build_stage_id]['status'] = status

  def GetActionsForChanges(self, changes):
    """Gets all the actions for the given changes."""
    clauses = set()
    for change in changes:
      change_source = 'internal' if change.internal else 'external'
      clauses.add((int(change.gerrit_number), change_source))
    values = []
    for row in self.GetActionHistory():
      if (row.change_number, row.change_source) in clauses:
        values.append(row)
    return values

  def GetActionHistory(self, *args, **kwargs):
    """Get all the actions for all changes."""
    # pylint: disable=W0613
    values = []
    for item, action_id in zip(self.clActionTable, itertools.count()):
      row = (
          action_id,
          item['build_id'],
          item['action'],
          item['reason'],
          self.buildTable[item['build_id']]['build_config'],
          item['change_number'],
          item['patch_number'],
          item['change_source'],
          item['timestamp'],
          item['buildbucket_id'])
      values.append(row)

    return clactions.CLActionHistory(clactions.CLAction(*row) for row in values)

  def GetBuildStatus(self, build_id):
    """Gets the status of the build."""
    try:
      return self._TrimStatus(self.buildTable[build_id])
    except IndexError:
      return None

  def GetBuildStatuses(self, build_ids):
    """Gets the status of the builds."""
    return [self._TrimStatus(self.buildTable[x]) for x in build_ids]

  def GetSlaveStatuses(self, master_build_id, buildbucket_ids=None):
    """Gets the slaves of given build."""
    if buildbucket_ids is None:
      return [self._TrimStatus(b) for b in self.buildTable
              if b['master_build_id'] == master_build_id]
    else:
      return [self._TrimStatus(b) for b in self.buildTable
              if b['master_build_id'] == master_build_id and
              b['buildbucket_id'] in buildbucket_ids]

  def GetBuildStage(self, build_stage_id):
    """Get build stage given the build_stage_id.

    Args:
      build_stage_id: The build_stage_id to get the stage.

    Returns:
      A dict prensenting the stage if the build_stage_id exists in
      the buildStageTable; else, None.
    """
    return self.buildStageTable.get(build_stage_id)

  def GetBuildStages(self, build_id):
    """Gets build stages given the build_id"""
    return [self.buildStageTable[_id]
            for _id in self.buildStageTable
            if self.buildStageTable[_id]['build_id'] == build_id]

  def GetSlaveStages(self, master_build_id, buildbucket_ids=None):
    """Get the slave stages of the given build.

    Args:
      master_build_id: The build id (string) of the master build.
      buildbucket_ids: A list of buildbucket ids (strings) of the slaves.

    Returns:
      A list of slave stages (in format of dicts).
    """
    slave_builds = []

    if buildbucket_ids is None:
      slave_builds = {b['id']: b for b in self.buildTable
                      if b['master_build_id'] == master_build_id}
    else:
      slave_builds = {b['id']: b for b in self.buildTable
                      if b['master_build_id'] == master_build_id and
                      b['buildbucket_id'] in buildbucket_ids}

    slave_stages = []
    for _id in self.buildStageTable:
      build_id = self.buildStageTable[_id]['build_id']
      if build_id in slave_builds:
        stage = self.buildStageTable[_id].copy()
        stage['build_config'] = slave_builds[build_id]['build_config']
        slave_stages.append(stage)

    return slave_stages

  def GetBuildHistory(self, build_config, num_results,
                      ignore_build_id=None, start_date=None, end_date=None,
                      starting_build_number=None, milestone_version=None):
    """Returns the build history for the given |build_config|."""
    builds = [b for b in self.buildTable
              if b['build_config'] == build_config]
    # Reverse sort as that's what's expected.
    builds = sorted(builds[-num_results:], reverse=True)

    # Filter results.
    if ignore_build_id is not None:
      builds = [b for b in builds if b['id'] != ignore_build_id]
    if start_date is not None:
      builds = [b for b in builds
                if b['start_time'].date() >= start_date]
    if end_date is not None:
      builds = [b for b in builds
                if 'finish_time' in b and
                b['finish_time'] and
                b['finish_time'].date() <= end_date]
    if starting_build_number is not None:
      builds = [b for b in builds
                if b['build_number'] >= starting_build_number]
    if milestone_version is not None:
      builds = [b for b in builds
                if b['milestone_version'] == milestone_version]

    return builds

  def GetTimeToDeadline(self, build_id):
    """Gets the time remaining until deadline."""
    now = datetime.datetime.now()
    deadline = self.buildTable[build_id]['deadline']
    return max(0, (deadline - now).total_seconds())

  def GetKeyVals(self):
    """Gets contents of keyvalTable."""
    return self.fake_keyvals

  def GetBuildStatusWithBuildbucketId(self, buildbucket_id):
    for row in self.buildTable:
      if row['buildbucket_id'] == buildbucket_id:
        return self._TrimStatus(row)

    return None

  def HasFailureMsgForStage(self, build_stage_id):
    """Determine whether a build stage has failure messages in failureTable.

    Args:
      build_stage_id: The id of the build_stage to query for.

    Returns:
      True if there're failures reported to failureTable for this build stage
      to cidb; else, False.
    """
    stages = self.failureTable.values()
    for stage in stages:
      if stage['build_stage_id'] == build_stage_id:
        return True

    return False
