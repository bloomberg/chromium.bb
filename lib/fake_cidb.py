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
      change_number = cl_action.change_number
      patch_number = cl_action.patch_number
      change_source = cl_action.change_source
      action = cl_action.action
      reason = cl_action.reason
      rows.append({
          'build_id' : build_id,
          'change_source' : change_source,
          'change_number': change_number,
          'patch_number' : patch_number,
          'action' : action,
          'timestamp': datetime.datetime.now(),
          'reason' : reason})

    self.clActionTable.extend(rows)
    return len(rows)

  def GetActionsForChange(self, change):
    """Gets all the actions for the given change."""
    change_number = int(change.gerrit_number)
    change_source = 'internal' if change.internal else 'external'
    values = []
    for item, action_id in zip(self.clActionTable, itertools.count()):
      if (item['change_number'] == change_number and
          item['change_source'] == change_source):
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

    return [clactions.CLAction(*row) for row in values]
