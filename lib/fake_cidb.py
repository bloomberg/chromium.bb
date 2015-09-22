# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fake CIDB for unit testing."""

from __future__ import print_function

import datetime
import itertools

from chromite.cbuildbot import constants
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
                  timeout_seconds=None, status=constants.BUILDER_STATUS_PASSED):
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
           'finish_time': datetime.datetime.now()}
    self.buildTable.append(row)
    return build_id

  def UpdateMetadata(self, build_id, metadata):
    """See cidb.UpdateMetadata."""
    d = metadata.GetDict()
    versions = d.get('version') or {}
    self.buildTable[build_id - 1].update(
        {'chrome_version': versions.get('chrome'),
         'milestone_version': versions.get('milestone'),
         'platform_version': versions.get('platform'),
         'full_version': versions.get('full'),
         'sdk_version': d.get('sdk-versions'),
         'toolchain_url': d.get('toolchain-url'),
         'build_type': d.get('build_type')})
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
      rows.append({
          'build_id' : build_id,
          'change_source' : change_source,
          'change_number': change_number,
          'patch_number' : patch_number,
          'action' : action,
          'timestamp': timestamp or datetime.datetime.now(),
          'reason' : reason})

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
          item['timestamp'])
      values.append(row)

    return clactions.CLActionHistory(clactions.CLAction(*row) for row in values)

  def GetBuildStatus(self, build_id):
    """Gets the status of the build."""
    return self.buildTable[build_id - 1]

  def GetBuildStatuses(self, build_ids):
    """Gets the status of the builds."""
    return [self.buildTable[x -1] for x in build_ids]

  def GetSlaveStatuses(self, master_build_id):
    """Gets the slaves of given build."""
    return [b for b in self.buildTable
            if b['master_build_id'] == master_build_id]

  def GetBuildHistory(self, build_config, num_results,
                      ignore_build_id=None, start_date=None, end_date=None,
                      starting_build_number=None):
    """Returns the build history for the given |build_config|."""
    def ReduceToBuildConfig(new_list, current_build):
      """Filters a build list to only those of a given config."""
      if current_build['build_config'] == build_config:
        new_list.append(current_build)

      return new_list

    build_configs = reduce(ReduceToBuildConfig, self.buildTable, [])
    # Reverse sort as that's what's expected.
    build_configs = sorted(build_configs[-num_results:], reverse=True)

    # Filter results.
    if ignore_build_id is not None:
      build_configs = [b for b in build_configs if b['id'] != ignore_build_id]
    if start_date is not None:
      build_configs = [b for b in build_configs
                       if b['start_time'].date() >= start_date]
    if end_date is not None:
      build_configs = [b for b in build_configs
                       if 'finish_time' in b and
                       b['finish_time'] and
                       b['finish_time'].date() <= end_date]
    if starting_build_number is not None:
      build_configs = [b for b in build_configs
                       if b['build_number'] >= starting_build_number]

    return build_configs

  def GetTimeToDeadline(self, build_id):
    """Gets the time remaining until deadline."""
    now = datetime.datetime.now()
    deadline = self.buildTable[build_id]['deadline']
    return max(0, (deadline - now).total_seconds())

  def GetKeyVals(self):
    """Gets contents of keyvalTable."""
    return self.fake_keyvals
