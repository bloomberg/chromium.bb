# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fake CIDB for unit testing."""

from __future__ import print_function

import datetime
import itertools

from chromite.cbuildbot import constants


class FakeCIDBConnection(object):
  """Fake connection to a Continuous Integration database.

  This class is a partial re-implementation of CIDBConnection, using
  in-memory lists rather than a backing database.
  """
  def __init__(self):
    self.buildTable = []
    self.clActionTable = []

  def InsertBuild(self, builder_name, waterfall, build_number,
                  build_config, bot_hostname,  master_build_id=None):
    """Insert a build row."""
    row = {'builder_name': builder_name,
           'buildbot_generation': constants.BUILDBOT_GENERATION,
           'waterfall': waterfall,
           'build_number': build_number,
           'build_config' : build_config,
           'bot_hostname': bot_hostname,
           'start_time': datetime.datetime.now(),
           'master_build_id' : master_build_id}
    build_id = len(self.buildTable)
    self.buildTable.append(row)
    return build_id

  def InsertCLActions(self, build_id, cl_actions):
    """Insert a list of |cl_actions|."""
    if not cl_actions:
      return 0

    rows = []
    for cl_action in cl_actions:
      change_source = 'internal' if cl_action[0]['internal'] else 'external'
      change_number = cl_action[0]['gerrit_number']
      patch_number = cl_action[0]['patch_number']
      action = cl_action[1]
      reason = cl_action[3]
      rows.append({
          'build_id' : build_id,
          'change_source' : change_source,
          'change_number': int(change_number),
          'patch_number' : int(patch_number),
          'action' : action,
          'reason' : reason,
          'timestamp': datetime.datetime.now()})

    self.clActionTable.extend(rows)
    return len(rows)

  def GetActionsForChange(self, change):
    """Gets all the actions for the given change."""
    change_number = int(change.gerrit_number)
    change_source = 'internal' if change.internal else 'external'
    columns = ['id', 'build_id', 'action', 'build_config', 'change_number',
               'patch_number', 'change_source', 'timestamp']
    values = []
    for item, action_id in zip(self.clActionTable, itertools.count()):
      if (item['change_number'] == change_number and
          item['change_source'] == change_source):
        row = (
            action_id,
            item['build_id'],
            item['action'],
            self.buildTable[item['build_id']]['build_config'],
            item['change_number'],
            item['patch_number'],
            item['change_source'],
            item['timestamp'])
        values.append(row)

    return [dict(zip(columns, row)) for row in values]
