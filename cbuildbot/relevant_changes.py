# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for tracking relevant changes (i.e. CLs) to validate."""

from __future__ import print_function

from chromite.lib import clactions
from chromite.lib import constants
from chromite.lib import cros_build_lib


class RelevantChanges(object):
  """Class that quries and tracks relevant changes."""

  @classmethod
  def _GetSlaveMappingAndCLActions(cls, master_build_id, db, config, changes,
                                   slave_buildbucket_ids, include_master=False):
    """Query CIDB to for slaves and CL actions.

    Args:
      master_build_id: Build id of this master build.
      db: Instance of cidb.CIDBConnection.
      config: Instance of config_lib.BuildConfig of this build.
      changes: A list of GerritPatch instances to examine.
      slave_buildbucket_ids: A list of buildbucket_ids (strings) of slave builds
                             scheduled by Buildbucket.
      include_master: Boolean indicating whether to include the master build in
                      the config_map. Default to False.

    Returns:
      A tuple of (config_map, action_history). The config_map is a dictionary
      mapping build_id to config name for all slaves in this run. If
      include_master is True, the config_map also includes master build. The
      action_history is a list of all CL actions associated with |changes|.
    """
    assert db, 'No database connection to use.'
    assert config.master, 'This is not a master build.'

    slave_list = db.GetSlaveStatuses(
        master_build_id, buildbucket_ids=slave_buildbucket_ids)

    # TODO(akeshet): We are getting the full action history for all changes that
    # were in this CQ run. It would make more sense to only get the actions from
    # build_ids of this master and its slaves.
    action_history = db.GetActionsForChanges(changes)

    config_map = dict()

    for d in slave_list:
      config_map[d['id']] = d['build_config']

    if include_master:
      config_map[master_build_id] = config.name

    return config_map, action_history

  @classmethod
  def GetRelevantChangesForSlaves(cls, master_build_id, db, config, changes,
                                  no_stat, slave_buildbucket_ids,
                                  include_master=False):
    """Compile a set of relevant changes for each slave.

    Args:
      master_build_id: Build id of this master build.
      db: Instance of cidb.CIDBConnection.
      config: Instance of config_lib.BuildConfig of this build.
      changes: A list of GerritPatch instances to examine.
      no_stat: Set of builder names of slave builders that had status None.
      slave_buildbucket_ids: A list of buildbucket_ids (strings) of slave builds
                             scheduled by Buildbucket.
      include_master: Boolean indicating whether to include the master build in
                      the config_map. Default to False.

    Returns:
      A dictionary mapping a slave config name to a set of relevant changes
      (as GerritPatch instances). If include_master is True, the dictionary
      includes the master build config and its relevant changes.
    """
    # Retrieve the slaves and clactions from CIDB.
    config_map, action_history = cls._GetSlaveMappingAndCLActions(
        master_build_id, db, config, changes, slave_buildbucket_ids,
        include_master=include_master)
    changes_by_build_id = clactions.GetRelevantChangesForBuilds(
        changes, action_history, config_map.keys())

    # Convert index from build_ids to config names.
    changes_by_config = dict()
    for k, v in changes_by_build_id.iteritems():
      changes_by_config[config_map[k]] = v

    for config in no_stat:
      # If a slave is in |no_stat|, it means that the slave never
      # finished applying the changes in the sync stage. Hence the CL
      # pickup actions for this slave may be
      # inaccurate. Conservatively assume all changes are relevant.
      changes_by_config[config] = set(changes)

    return changes_by_config

  @classmethod
  def GetSubsysResultForSlaves(cls, master_build_id, db):
    """Get the pass/fail HWTest subsystems results for each slave.

    Returns:
      A dictionary mapping a slave config name to a dictionary of the pass/fail
      subsystems. E.g.
      {'foo-paladin': {'pass_subsystems':{'A', 'B'},
                       'fail_subsystems':{'C'}}}
    """
    assert db, 'No database connection to use.'
    slave_msgs = db.GetSlaveBuildMessages(master_build_id)
    slave_subsys_msgs = ([m for m in slave_msgs
                          if m['message_type'] == constants.SUBSYSTEMS])
    subsys_by_config = dict()
    group_msg_by_config = cros_build_lib.GroupByKey(slave_subsys_msgs,
                                                    'build_config')
    for config, dict_list in group_msg_by_config.iteritems():
      d = subsys_by_config.setdefault(config, {})
      subsys_groups = cros_build_lib.GroupByKey(dict_list, 'message_subtype')
      for k, v in subsys_groups.iteritems():
        if k == constants.SUBSYSTEM_PASS:
          d['pass_subsystems'] = set([x['message_value'] for x in v])
        if k == constants.SUBSYSTEM_FAIL:
          d['fail_subsystems'] = set([x['message_value'] for x in v])
        # If message_subtype==subsystem_unused, keep d as an empty dict.
    return subsys_by_config
