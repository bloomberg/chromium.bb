# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for tracking relevant changes (i.e. CLs) to validate."""

from __future__ import print_function

from chromite.lib import builder_status_lib
from chromite.lib import clactions
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import patch as cros_patch
from chromite.lib import triage_lib


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

class TriageRelevantChanges(object):
  """Class to triage relevant changes within a CQ run..

  This class keeps track of relevant_changes of a list slave builds of given a
  master build. With the build information fetched from Buildbucket and CIDB,
  it performs relevant change triages, and returns a ShouldWait flag indicating
  whether it's still meaningful for the master build to wait for the slave
  builds. The triages include anaylizing whether the failed slave builds have
  passed the critial sync stage, whether the failures in failed slave builds
  are ignorable for changes, classifying changes into will_submit, might_submit
  and will_not_submit sets, and so on.
  More context: go/self-destructed-commit-queue
  """

  # Accepted statues of the critical stages
  ACCEPTED_STATUSES = {
      constants.BUILDER_STATUS_PASSED,
      constants.BUILDER_STATUS_SKIPPED
  }

  # TODO(nxia): crbug.com/694749
  # Get stage names from stage classes instead of duplicating them here.
  COMMIT_QUEUE_SYNC = 'CommitQueueSync'
  MASTER_SLAVE_LKGM_SYNC = 'MasterSlaveLKGMSync'
  COMMIT_QUEUE_COMPLETION = 'CommitQueueCompletion'

  STAGE_SYNC = {COMMIT_QUEUE_SYNC, MASTER_SLAVE_LKGM_SYNC}
  STAGE_COMPLETION = {COMMIT_QUEUE_COMPLETION}

  def __init__(self, master_build_id, db, config, version, build_root, changes,
               buildbucket_info_dict, cidb_status_dict, completed_builds,
               dependency_map):
    """Initialize an instance of TriageRelevantChanges.

    Args:
      master_build_id: The build_id of the master build.
      db: An instance of cidb.CIDBConnection to fetch data from CIDB.
      config: An instance of config_lib.BuildConfig. Config dict of this build.
      version: Current manifest version string. See the return type of
        VersionInfo.VersionString().
      build_root: Path to the build root directory.
      changes: A list of changes (GerritPatch instances) which have been applied
        to this build.
      buildbucket_info_dict: A dict mapping all slave build config names to
        their BuildbucketInfos (See SlaveStatus.GetAllSlaveBuildbucketInfo
        for details).
      cidb_status_dict: A dict mapping all slave build config names to their
        CIDBStatusInfos (See SlaveStatus.GetAllSlaveCIDBStatusInfo for details)
      completed_builds: A set of slave build config names (strings) which
        have completed and will not be retried.
      dependency_map: A dict mapping a change (patch.GerritPatch instance) to a
        set of changes (patch.GerritPatch instances) depending on this change.
        (See ValidationPool.GetDependMapForChanges for details.)
    """
    self.master_build_id = master_build_id
    self.db = db
    self.config = config
    self.version = version
    self.buildbucket_info_dict = buildbucket_info_dict
    self.cidb_status_dict = cidb_status_dict
    self.completed_builds = completed_builds
    self.build_root = build_root
    self.changes = changes
    self.dependency_map = dependency_map

    # Dict mapping slave config names to a list of stages
    self.slave_stages_dict = None
    # Dict mapping slave config names to relevant change sets.
    self.slave_changes_dict = None
    # Dict mapping slave config names to subsys sets.
    self.slave_subsys_dict = None

    # A set of changes which will be submitted by the master.
    self.will_submit = set()
    # A set of changes which are being tested by the slaves.
    self.might_submit = set(self.changes)
    # A set of chagnes which won't be submitted by the master.
    self.will_not_submit = set()

    # A dict mapping build config name to a set of changes which can ignore the
    # failures in the build.
    self.build_ignorable_changes_dict = {}

    self._UpdateSlaveInfo()

  def _UpdateSlaveInfo(self):
    """Update slave infomation with stages, relevant_changes, and subsys."""
    self.slave_stages_dict = self.GetSlaveStages(
        self.master_build_id, self.db, self.buildbucket_info_dict)
    self.slave_changes_dict = self._GetRelevantChanges(
        self.slave_stages_dict)
    self.slave_subsys_dict = RelevantChanges.GetSubsysResultForSlaves(
        self.master_build_id, self.db)

  @staticmethod
  def GetDependChanges(changes, dependency_map):
    """Get a set of changes depending on the given changes.

    Args:
      changes: A set of changes to get the dependent change set.
      dependency_map: A dict mapping a change (patch.GerritPatch instance) to a
        set of changes (patch.GerritPatch instances) directly or indirectly
        depending on this change. (See ValidationPool.GetDependMapForChanges for
        details.)

    Returns:
      A set of all changes directly or indirectly depending on the given
      changes.
    """
    return set().union(*[dependency_map.get(c, set()) for c in changes])

  # TODO(nxia): Consolidate with completion_stages._ShouldSubmitPartialPool
  @staticmethod
  def GetSlaveStages(master_build_id, db, buildbucket_info_dict):
    """Get slave stages from CIDB.

    Args:
      master_build_id: The build_id of the master build.
      db: An instance of cidb.CIDBConnection to fetch data from CIDB.
      buildbucket_info_dict: A dict mapping all slave build config names to
        their BuildbucketInfos (See SlaveStatus.GetAllSlaveBuildbucketInfo
        for details).

    Returns:
      A dict mapping all slave config names (strings) to their stages (a list
        of dicts, see cidb.CIDBConnection.GetSlaveStages for details.)
    """
    assert db, 'No database connection to use.'

    slave_stages_dict = {}
    slave_buildbucket_ids = []

    if buildbucket_info_dict is not None:
      for slave_config, buildbucket_info in buildbucket_info_dict.iteritems():
        # Set default value for all slaves, some may not have stages in CIDB.
        slave_stages_dict.setdefault(slave_config, [])
        slave_buildbucket_ids.append(buildbucket_info.buildbucket_id)

    stages = db.GetSlaveStages(master_build_id,
                               buildbucket_ids=slave_buildbucket_ids)
    for stage in stages:
      slave_stages_dict[stage['build_config']].append(stage)

    return slave_stages_dict

  @classmethod
  def PassedAnyOfStages(cls, stages, desired_stages):
    """Check if the stages have passed any stage from desired_stages.

    Args:
      stages: A list of stages (see the type of slave_stages_dict value part)
        to check.
      desired_stages: A set of desired stages (strings).

    Returns:
      True if the accepted stages in the given stages cover any stage in
      the desired_stages set; else, False.
    """
    accepted_stages = {stage['name'] for stage in stages
                       if stage['status'] in cls.ACCEPTED_STATUSES}

    return accepted_stages.intersection(desired_stages)

  @classmethod
  def GetBuildsPassedAnyOfStages(cls, slave_stages_dict, desired_stages):
    """Get builds which have passed any stage from desired_stages.

    Args:
      slave_stages_dict: A dict mapping slaves config names to their stage lists
        (see GetSlaveStages for details).
      desired_stages: A set of desired stages (strings).

    Returns:
      A set of build config names (strings) which have passed any stage in
      desired_stages.
    """
    return set(slave_config
               for slave_config, stages in slave_stages_dict.iteritems()
               if cls.PassedAnyOfStages(stages, desired_stages))

  def _GetRelevantChanges(self, slave_stages_dict):
    """Get relevant changes for slave builds.

    Args:
      slave_stages_dict: A dict mapping slaves config names (strings) to their
        stage lists. (see GetSlaveStages for details).

    Returns:
      A dict mapping all slave config names (strings) to sets of changes which
      are relevant to the slave builds. If a build has passed the STAGE_SYNC
      stage, it has recorded the CLs it picked up in the CIDB, it's mapped to
      its relevant change set. If a build failed to pass the STAGE_SYNC stage,
      we assume it's relevant to all changes, so it's mapped to the change set
      containing all the applied changes.
    """
    # If a build passed the sync stage, the picked up change stats are recorded.
    stat_builds = self.GetBuildsPassedAnyOfStages(
        slave_stages_dict, self.STAGE_SYNC)
    no_stat_builds = set(self.buildbucket_info_dict.keys()) - stat_builds
    slave_buildbucket_ids = [bb_info.buildbucket_id
                             for bb_info in self.buildbucket_info_dict.values()]
    slave_changes_dict = RelevantChanges.GetRelevantChangesForSlaves(
        self.master_build_id, self.db, self.config, self.changes,
        no_stat_builds, slave_buildbucket_ids)

    # Some slaves may not pick up any changes, update the value to set()
    for slave_config in self.buildbucket_info_dict:
      slave_changes_dict.setdefault(slave_config, set())

    return slave_changes_dict

  def _GetIgnorableChanges(self, build_config, builder_status,
                           relevant_changes):
    """Get changes that can ignore failures in BuilderStatus.

    Some projects are configured with ignored-stages in COMMIT_QUEUE.ini. The CQ
    can still submit changes from these projects if all failed statges are
    listed in ignored-stages. Please refer to
    triage_lib.GetStagesToIgnoreForChange for more details.

    1) if the builder_status is in 'pass' status, it means the build uploaded a
    'pass' builder_status but failed other steps in or after the completion
    stage. This is rare but still possible, and it should not blame any changes
    as the build has finishes its testing. Returns all changes in
    relevant_changes in this case.
    2) else if the builder_status is in 'fail' with failure messages, it
    calculates and returns all ignorable changes given the failure messages.
    3) else, the builder_status is either in 'fail' status without failure
    messages or in one of the 'inflight' and 'missing' statuses. It cannot
    calculate ignorable changes without any failure message so should just
    return an empty set.

    Args:
      build_config: The config name (string) of the build.
      builder_status: An instance of build_status.BuilderStatus.
      relevant_changes: A set of relevant changes for triage to get the
        ignorable changes.

    Returns:
      A set of ignorable changes (GerritPatch instances).
    """
    if builder_status.Passed():
      return relevant_changes
    elif builder_status.Failed() and builder_status.message:
      ignoreable_changes = set()
      for change in relevant_changes:
        ignore_result = triage_lib.CalculateSuspects.CanIgnoreFailures(
            [builder_status.message], change, self.build_root,
            self.slave_subsys_dict)

        if ignore_result[0]:
          logging.debug('change %s is ignoreable for failures of %s.',
                        cros_patch.GetChangesAsString([change]), build_config)
          ignoreable_changes.add(change)
      return ignoreable_changes
    else:
      return set()

  def _UpdateWillNotSubmitChanges(self, will_not_submit):
    """Update will_not_submit change set.

    Args:
      will_not_submit: A set of changes (GerritPatch instances) to add to
        will_not_submit.
    """
    self.will_not_submit.update(will_not_submit)
    self.might_submit.difference_update(will_not_submit)

  def _ProcessCompletedBuilds(self):
    """Process completed and not retriable builds.

    This method goes through all the builds which completed without SUCCESS
    result and will not be retried.
    1) if the failed build didn't pass the sync stage, move all changes to
    will_not_submit set;
    2) else, get BuilderStatus for the build (if there's no BuilderStatus
    pickle file in GS, a BuilderStatus with 'missing' status will be returned).
    Calculate ignorable changes given the BuilderStatus, move all not ignorable
    changes and their dependencies to will_not_submit set.
    """
    for build_config, bb_info in self.buildbucket_info_dict.iteritems():
      if (build_config in self.completed_builds and
          bb_info.status == constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED and
          bb_info.result != constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS):
        # This build didn't succeed and cannot be retired.
        logging.info('Process completed build %s status %s result %s',
                     build_config, bb_info.status, bb_info.result)

        stages = self.slave_stages_dict[build_config]
        if not self.PassedAnyOfStages(stages, self.STAGE_SYNC):
          # The build didn't pass sync stage. Will not submit all changes
          # as the build failed to pick relevant changes in a passed sync stage.
          self._UpdateWillNotSubmitChanges(set(self.changes))
          logging.info('Build %s didn\'t pass %s, will not submit changes.',
                       build_config, ' or '.join(self.STAGE_SYNC))
        else:
          # The build passed sync stage. Get builder_status and calculate
          # ignorable changes based on the builder_status. Move not ignorable
          # changes and their dependencies to will_not_submit set.
          relevant_changes = self.slave_changes_dict[build_config]
          builder_status = (
              builder_status_lib.BuilderStatusManager.GetBuilderStatus(
                  build_config, self.version))
          ignorable_changes = self._GetIgnorableChanges(
              build_config, builder_status, relevant_changes)
          self.build_ignorable_changes_dict[build_config] = ignorable_changes
          not_ignorable_changes = relevant_changes - ignorable_changes
          depend_changes = self.GetDependChanges(
              not_ignorable_changes, self.dependency_map)
          will_not_submit = not_ignorable_changes | depend_changes
          self._UpdateWillNotSubmitChanges(will_not_submit)
          logging.info('Build %s didn\'t pass %s, will not submit changes %s',
                       build_config, self.STAGE_COMPLETION,
                       cros_patch.GetChangesAsString(will_not_submit))

        if not self.might_submit:
          # No need to process other completed builds, might_submit is empty.
          return

  def _GetChangeToSlavesDict(self, slave_changes_dict):
    """Get change to relevant slaves dict.

    Args:
      slave_changes_dict: A dict mapping all slave config names (strings) to
        sets of changes (GerritPatch instances) which are relevant to the slave
        builds (See return type of _GetRelevantChanges for details).

    Returns:
      A dict mapping changes (GerritPatch instances) to sets of slave config
      names (strings) which are relevant to changes.
    """
    change_slaves_dict = {}
    for slave, changes in slave_changes_dict.iteritems():
      for change in changes:
        change_slaves_dict.setdefault(change, set()).add(slave)

    return change_slaves_dict

  def _ChangeCanBeSubmitted(self, change, relevant_slave_configs,
                            build_ignorable_changes_dict):
    """Verify whether the change can be submitted given its relevant slaves.

    A change can be submitted if it satisfies either of the conditions:
    1) all of its relevant slaves successfully completed;
    2) all of its relevant slaves completed, and the slaves marked as 'FAILURE'
    either uploaded 'passed' BuilderStatus pickle to GS or only contain failures
    which can be ignored by the change.

    Args:
      change: A change (GerritPatch instance) to check.
      relevant_slave_configs: A list of relevant slave config names (string) of
        this change.
      build_ignorable_changes_dict: A dict mapping build config name (string) to
        a set of changes (GerritPatch instances) which can ignore the failures
        in the build.

    Returns:
      True if the change can be submitted given the statues of its relevant
      slaves; else, False.
    """
    for slave_config in relevant_slave_configs:
      bb_info = self.buildbucket_info_dict[slave_config]
      if bb_info.status != constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED:
        return False
      if bb_info.result != constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS:
        # If the build uploaded 'passed' BuilderStatus pickle or the build
        # only contains failures which can be ignored by this change, change is
        # in the value set for slave_config in build_ignorable_changes_dict.
        if change not in build_ignorable_changes_dict.get(slave_config, set()):
          return False

    return True

  def _ProcessMightSubmitChanges(self):
    """Process changes in might_submit set.

    This method goes through all the changes in current might_submit set. For
    each change, get a set of its relevant slaves. If all the relevant slaves
    have completed with success, move the change to will_submit set.
    """
    if not self.might_submit:
      return

    logging.info('Processing changes which might be submitted.')
    change_slaves_dict = self._GetChangeToSlavesDict(self.slave_changes_dict)
    changes_to_submit = set()
    for change in self.might_submit:
      if self._ChangeCanBeSubmitted(
          change, change_slaves_dict.get(change, set()),
          self.build_ignorable_changes_dict):
        changes_to_submit.add(change)

    if changes_to_submit:
      self.will_submit.update(changes_to_submit)
      self.might_submit.difference_update(changes_to_submit)
      logging.info('Moving %s to will_submit set, because their relevant builds'
                   ' completed successfully or all failures are ignorable. '
                   'will_submit now contains %d changes,',
                   cros_patch.GetChangesAsString(changes_to_submit),
                   len(self.will_submit))

  def ShouldWait(self):
    """Process builds and relevant changes, decide whether to wait on slaves.

    Returns:
      True if the master should wait for running slaves to finish testing
      changes in might_submit set; False if the master shouldn't wait for any
      not completed slaves.
    """
    self._ProcessCompletedBuilds()
    self._ProcessMightSubmitChanges()

    if not self.might_submit:
      logging.warning('Might submit change set is empty.')
      return False

    return True
