# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the completion stages."""

from __future__ import print_function

from chromite.cbuildbot import chroot_lib
from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import prebuilts
from chromite.cbuildbot import tree_status
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import clactions
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import patch as cros_patch


def GetBuilderSuccessMap(builder_run, overall_success):
  """Get the pass/fail status of all builders.

  A builder is marked as passed if all of its steps ran all of the way to
  completion. We determine this by looking at whether all of the steps for
  all of the constituent boards ran to completion.

  In cases where a builder does not have any boards, or has child boards, we
  fall back and instead just look at whether the entire build was successful.

  Args:
    builder_run: The builder run we wish to get the status of.
    overall_success: The overall status of the build.

  Returns:
    A dict, mapping the builder names to whether they succeeded.
  """
  success_map = {}
  for run in [builder_run] + builder_run.GetChildren():
    if run.config.boards and not run.config.child_configs:
      success_map[run.config.name] = True
      for board in run.config.boards:
        board_runattrs = run.GetBoardRunAttrs(board)
        if not board_runattrs.HasParallel('success'):
          success_map[run.config.name] = False
    else:
      # If a builder does not have boards, or if it has child configs, we
      # will just use the overall status instead.
      success_map[run.config.name] = overall_success
  return success_map


def CreateBuildFailureMessage(overlays, builder_name, dashboard_url):
  """Creates a message summarizing the failures.

  Args:
    overlays: The overlays used for the build.
    builder_name: The name of the builder.
    dashboard_url: The URL of the build.

  Returns:
    A failures_lib.BuildFailureMessage object.
  """
  internal = overlays in [constants.PRIVATE_OVERLAYS,
                          constants.BOTH_OVERLAYS]
  details = []
  tracebacks = tuple(results_lib.Results.GetTracebacks())
  for x in tracebacks:
    if isinstance(x.exception, failures_lib.CompoundFailure):
      # We do not want the textual tracebacks included in the
      # stringified CompoundFailure instance because this will be
      # printed on the waterfall.
      ex_str = x.exception.ToSummaryString()
    else:
      ex_str = str(x.exception)
    # Truncate displayed failure reason to 1000 characters.
    ex_str = ex_str[:200]
    details.append('The %s stage failed: %s' % (x.failed_stage, ex_str))
  if not details:
    details = ['cbuildbot failed']

  # reason does not include builder name or URL. This is mainly for
  # populating the "failure message" column in the stats sheet.
  reason = ' '.join(details)
  details.append('in %s' % dashboard_url)
  msg = '%s: %s' % (builder_name, ' '.join(details))

  return failures_lib.BuildFailureMessage(msg, tracebacks, internal, reason,
                                          builder_name)


class ManifestVersionedSyncCompletionStage(
    generic_stages.ForgivingBuilderStage):
  """Stage that records board specific results for a unique manifest file."""

  option_name = 'sync'

  def __init__(self, builder_run, sync_stage, success, **kwargs):
    super(ManifestVersionedSyncCompletionStage, self).__init__(
        builder_run, **kwargs)
    self.sync_stage = sync_stage
    self.success = success
    # Message that can be set that well be sent along with the status in
    # UpdateStatus.
    self.message = None

  def GetBuildFailureMessage(self):
    """Returns message summarizing the failures."""
    return CreateBuildFailureMessage(self._run.config.overlays,
                                     self._run.config.name,
                                     self._run.ConstructDashboardURL())

  def PerformStage(self):
    if not self.success:
      self.message = self.GetBuildFailureMessage()

    if not config_lib.IsPFQType(self._run.config.build_type):
      # Update the pass/fail status in the manifest-versions
      # repo. Suite scheduler checks the build status to schedule
      # suites.
      self._run.attrs.manifest_manager.UpdateStatus(
          success_map=GetBuilderSuccessMap(self._run, self.success),
          message=self.message, dashboard_url=self.ConstructDashboardURL())


class ImportantBuilderFailedException(failures_lib.StepFailure):
  """Exception thrown when an important build fails to build."""


class MasterSlaveSyncCompletionStage(ManifestVersionedSyncCompletionStage):
  """Stage that records whether we passed or failed to build/test manifest."""

  def __init__(self, *args, **kwargs):
    super(MasterSlaveSyncCompletionStage, self).__init__(*args, **kwargs)
    self._slave_statuses = {}

  def _GetLocalBuildStatus(self):
    """Return the status for this build as a dictionary."""
    status = manifest_version.BuilderStatus.GetCompletedStatus(self.success)
    status_obj = manifest_version.BuilderStatus(status, self.message)
    return {self._bot_id: status_obj}

  def _FetchSlaveStatuses(self):
    """Fetch and return build status for slaves of this build.

    If this build is not a master then return just the status of this build.

    Returns:
      A dict of build_config name -> BuilderStatus objects, for all important
      slave build configs. Build configs that never started will have a
      BuilderStatus of MISSING.
    """
    # Wait for slaves if we're a master, in production or mock-production.
    # Otherwise just look at our own status.
    slave_statuses = self._GetLocalBuildStatus()
    if not self._run.config.master:
      # The slave build returns its own status.
      logging.warning('The build is not a master.')
    elif self._run.options.mock_slave_status or not self._run.options.debug:
      # The master build.
      builders = self._GetSlaveConfigs()
      builder_names = [b.name for b in builders]
      timeout = None
      build_id, db = self._run.GetCIDBHandle()
      if db:
        timeout = db.GetTimeToDeadline(build_id)
      if timeout is None:
        # Catch-all: This could happen if cidb is not setup, or the deadline
        # query fails.
        timeout = self._run.config.build_timeout

      if self._run.options.debug:
        # For debug runs, wait for three minutes to ensure most code
        # paths are executed.
        logging.info('Waiting for 3 minutes only for debug run. '
                     'Would have waited for %s seconds.', timeout)
        timeout = 3 * 60

      manager = self._run.attrs.manifest_manager
      if sync_stages.MasterSlaveLKGMSyncStage.external_manager:
        manager = sync_stages.MasterSlaveLKGMSyncStage.external_manager
      slave_statuses.update(manager.GetBuildersStatus(
          self._run.attrs.metadata.GetValue('build_id'),
          builder_names,
          timeout=timeout))
    return slave_statuses

  def _HandleStageException(self, exc_info):
    """Decide whether an exception should be treated as fatal."""
    # Besides the master, the completion stages also run on slaves, to report
    # their status back to the master. If the build failed, they throw an
    # exception here. For slave builders, marking this stage 'red' would be
    # redundant, since the build itself would already be red. In this case,
    # report a warning instead.
    # pylint: disable=protected-access
    exc_type = exc_info[0]
    if (issubclass(exc_type, ImportantBuilderFailedException) and
        not self._run.config.master):
      return self._HandleExceptionAsWarning(exc_info)
    else:
      # In all other cases, exceptions should be treated as fatal. To
      # implement this, we bypass ForgivingStage and call
      # generic_stages.BuilderStage._HandleStageException explicitly.
      return generic_stages.BuilderStage._HandleStageException(self, exc_info)

  def HandleSuccess(self):
    """Handle a successful build.

    This function is called whenever the cbuildbot run is successful.
    For the master, this will only be called when all slave builders
    are also successful. This function may be overridden by subclasses.
    """
    # We only promote for the pfq, not chrome pfq.
    # TODO(build): Run this logic in debug mode too.
    if (not self._run.options.debug and
        config_lib.IsPFQType(self._run.config.build_type) and
        self._run.config.master and
        self._run.manifest_branch == 'master' and
        self._run.config.build_type != constants.CHROME_PFQ_TYPE):
      self._run.attrs.manifest_manager.PromoteCandidate()
      if sync_stages.MasterSlaveLKGMSyncStage.external_manager:
        sync_stages.MasterSlaveLKGMSyncStage.external_manager.PromoteCandidate()

  def HandleFailure(self, failing, inflight, no_stat):
    """Handle a build failure.

    This function is called whenever the cbuildbot run fails.
    For the master, this will be called when any slave fails or times
    out. This function may be overridden by subclasses.

    Args:
      failing: The names of the failing builders.
      inflight: The names of the builders that are still running.
      no_stat: Set of builder names of slave builders that had status None.
    """
    if failing or inflight or no_stat:
      logging.PrintBuildbotStepWarnings()

    if failing:
      logging.warning('\n'.join([
          'The following builders failed with this manifest:',
          ', '.join(sorted(failing)),
          'Please check the logs of the failing builders for details.']))

    if inflight:
      logging.warning('\n'.join([
          'The following builders took too long to finish:',
          ', '.join(sorted(inflight)),
          'Please check the logs of these builders for details.']))

    if no_stat:
      logging.warning('\n'.join([
          'The following builders did not start or failed prematurely:',
          ', '.join(sorted(no_stat)),
          'Please check the logs of these builders for details.']))

  def PerformStage(self):
    super(MasterSlaveSyncCompletionStage, self).PerformStage()

    # Upload our pass/fail status to Google Storage.
    self._run.attrs.manifest_manager.UploadStatus(
        success=self.success, message=self.message,
        dashboard_url=self.ConstructDashboardURL())

    statuses = self._FetchSlaveStatuses()
    self._slave_statuses = statuses
    no_stat = set(builder for builder, status in statuses.iteritems()
                  if status.Missing())
    failing = set(builder for builder, status in statuses.iteritems()
                  if status.Failed())
    inflight = set(builder for builder, status in statuses.iteritems()
                   if status.Inflight())

    # If all the failing or inflight builders were sanity checkers
    # then ignore the failure.
    fatal = self._IsFailureFatal(failing, inflight, no_stat)

    if fatal:
      self._AnnotateFailingBuilders(failing, inflight, no_stat, statuses)
      self.HandleFailure(failing, inflight, no_stat)
      raise ImportantBuilderFailedException()
    else:
      self.HandleSuccess()

  def _IsFailureFatal(self, failing, inflight, no_stat):
    """Returns a boolean indicating whether the build should fail.

    Args:
      failing: Set of builder names of slave builders that failed.
      inflight: Set of builder names of slave builders that are inflight
      no_stat: Set of builder names of slave builders that had status None.

    Returns:
      True if any of the failing or inflight builders are not sanity check
      builders for this master, or if there were any non-sanity-check builders
      with status None.
    """
    sanity_builders = self._run.config.sanity_check_slaves or []
    sanity_builders = set(sanity_builders)
    return not sanity_builders.issuperset(failing | inflight | no_stat)

  def _AnnotateFailingBuilders(self, failing, inflight, no_stat, statuses):
    """Add annotations that link to either failing or inflight builders.

    Adds buildbot links to failing builder dashboards. If no builders are
    failing, adds links to inflight builders. Adds step text for builders
    with status None.

    Args:
      failing: Set of builder names of slave builders that failed.
      inflight: Set of builder names of slave builders that are inflight.
      no_stat: Set of builder names of slave builders that had status None.
      statuses: A builder-name->status dictionary, which will provide
                the dashboard_url values for any links.
    """
    builders_to_link = set.union(failing, inflight)
    for builder in builders_to_link:
      if statuses[builder].dashboard_url:
        if statuses[builder].message:
          text = '%s: %s' % (builder, statuses[builder].message.reason)
        else:
          text = '%s: timed out' % builder

        logging.PrintBuildbotLink(text, statuses[builder].dashboard_url)

    for builder in no_stat:
      logging.PrintBuildbotStepText('%s did not start.' % builder)

  def GetSlaveStatuses(self):
    """Returns cached slave status results.

    Cached results are populated during PerformStage, so this function
    should only be called after PerformStage has returned.

    Returns:
      A dictionary from build names to manifest_version.BuilderStatus
      builder status objects.
    """
    return self._slave_statuses

  def _GetFailedMessages(self, failing):
    """Gathers the BuildFailureMessages from the |failing| builders.

    Args:
      failing: Names of the builders that failed.

    Returns:
      A list of BuildFailureMessage or NoneType objects.
    """
    return [self._slave_statuses[x].message for x in failing]

  def _GetBuildersWithNoneMessages(self, failing):
    """Returns a list of failed builders with NoneType failure message.

    Args:
      failing: Names of the builders that failed.

    Returns:
      A list of builder names.
    """
    return [x for x in failing if self._slave_statuses[x].message is None]


class CanaryCompletionStage(MasterSlaveSyncCompletionStage):
  """Collect build slave statuses and handle the failures."""

  def HandleFailure(self, failing, inflight, no_stat):
    """Handle a build failure or timeout in the Canary builders.

    Args:
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
      no_stat: Set of builder names of slave builders that had status None.
    """
    # Print out the status about what builds failed or not.
    MasterSlaveSyncCompletionStage.HandleFailure(
        self, failing, inflight, no_stat)

    if self._run.config.master:
      self.CanaryMasterHandleFailure(failing, inflight, no_stat)

  def SendCanaryFailureAlert(self, failing, inflight, no_stat):
    """Send an alert email to summarize canary failures.

    Args:
      failing: The names of the failing builders.
      inflight: The names of the builders that are still running.
      no_stat: The names of the builders that had status None.
    """
    builder_name = 'Canary Master'
    title = '%s has detected build failures:' % builder_name
    msgs = [str(x) for x in self._GetFailedMessages(failing)]
    slaves = self._GetBuildersWithNoneMessages(failing)
    msgs += ['%s failed with unknown reason.' % x for x in slaves]
    msgs += ['%s timed out' % x for x in inflight]
    msgs += ['%s did not start' % x for x in no_stat]
    msgs.insert(0, title)
    msgs.append('You can also view the summary of the slave failures from '
                'the %s stage of %s. Click on the failure message to go '
                'to an individual slave\'s build status page: %s' % (
                    self.name, builder_name, self.ConstructDashboardURL()))
    msg = '\n\n'.join(msgs)
    logging.warning(msg)
    extra_fields = {'X-cbuildbot-alert': 'canary-fail-alert'}
    tree_status.SendHealthAlert(self._run, 'Canary builder failures', msg,
                                extra_fields=extra_fields)

  def _ComposeTreeStatusMessage(self, failing, inflight, no_stat):
    """Composes a tres status message.

    Args:
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
      no_stat: Set of builder names of slave builders that had status None.

    Returns:
      A string.
    """
    slave_status_list = [
        ('did not start', list(no_stat)),
        ('timed out', list(inflight)),
        ('failed', list(failing)),]
    # Print maximum 2 slaves for each category to not clutter the
    # message.
    max_num = 2
    messages = []
    for status, slaves in slave_status_list:
      if not slaves:
        continue
      slaves_str = ','.join(slaves[:max_num])
      if len(slaves) <= max_num:
        messages.append('%s %s' % (slaves_str, status))
      else:
        messages.append('%s and %d others %s' % (slaves_str,
                                                 len(slaves) - max_num,
                                                 status))
    return '; '.join(messages)

  def CanaryMasterHandleFailure(self, failing, inflight, no_stat):
    """Handles the failure by sending out an alert email.

    Args:
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
      no_stat: Set of builder names of slave builders that had status None.
    """
    if self._run.manifest_branch == 'master':
      self.SendCanaryFailureAlert(failing, inflight, no_stat)
      # Note: We used to throttle the tree here. As of
      # https://chromium-review.googlesource.com/#/c/325821/ we no longer do.

  def _HandleStageException(self, exc_info):
    """Decide whether an exception should be treated as fatal."""
    # Canary master already updates the tree status for slave
    # failures. There is no need to mark this stage red. For slave
    # builders, the build itself would already be red. In this case,
    # report a warning instead.
    # pylint: disable=protected-access
    exc_type = exc_info[0]
    if issubclass(exc_type, ImportantBuilderFailedException):
      return self._HandleExceptionAsWarning(exc_info)
    else:
      # In all other cases, exceptions should be treated as fatal.
      return super(CanaryCompletionStage, self)._HandleStageException(exc_info)


class CommitQueueCompletionStage(MasterSlaveSyncCompletionStage):
  """Commits or reports errors to CL's that failed to be validated."""

  # These stages are required to have run at least once and to never have
  # failed, on each important slave. Otherwise, we may have incomplete
  # information on which CLs affect which builders, and thus skip all
  # board-aware submission.
  _CRITICAL_STAGES = ('CommitQueueSync',)

  def HandleSuccess(self):
    if self._run.config.master:
      self.sync_stage.pool.SubmitPool(reason=constants.STRATEGY_CQ_SUCCESS)
      if config_lib.IsPFQType(self._run.config.build_type):
        super(CommitQueueCompletionStage, self).HandleSuccess()

    manager = self._run.attrs.manifest_manager
    version = manager.current_version
    if version:
      chroot_manager = chroot_lib.ChrootManager(self._build_root)
      chroot_manager.SetChrootVersion(version)

  def HandleFailure(self, failing, inflight, no_stat):
    """Handle a build failure or timeout in the Commit Queue.

    This function performs any tasks that need to happen when the Commit Queue
    fails:
      - Abort the HWTests if necessary.
      - Push any CLs that indicate that they don't care about this failure.
      - Determine what CLs to reject.

    See MasterSlaveSyncCompletionStage.HandleFailure.

    Args:
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
      no_stat: Set of builder names of slave builders that had status None.
    """
    # Print out the status about what builds failed or not.
    MasterSlaveSyncCompletionStage.HandleFailure(
        self, failing, inflight, no_stat)

    if self._run.config.master:
      self.CQMasterHandleFailure(failing, inflight, no_stat)

  def _GetSlaveMappingAndCLActions(self, changes):
    """Query CIDB to for slaves and CL actions.

    Args:
      changes: A list of GerritPatch instances to examine.

    Returns:
      A tuple of (config_map, action_history), where the config_map
      is a dictionary mapping build_id to config name for all slaves
      in this run plus the master, and action_history is a list of all
      CL actions associated with |changes|.
    """
    # build_id is the master build id for the run.
    build_id, db = self._run.GetCIDBHandle()
    assert db, 'No database connection to use.'
    slave_list = db.GetSlaveStatuses(build_id)
    # TODO(akeshet): We are getting the full action history for all changes that
    # were in this CQ run. It would make more sense to only get the actions from
    # build_ids of this master and its slaves.
    action_history = db.GetActionsForChanges(changes)

    config_map = dict()

    # Build the build_id to config_name mapping. Note that if add the
    # "relaunch" feature in cbuildbot, there may be multiple build ids
    # for the same slave config. We will have to make sure
    # GetSlaveStatuses() returns only the valid slaves (e.g. with
    # latest start time).
    for d in slave_list:
      config_map[d['id']] = d['build_config']

    # TODO(akeshet): We are giving special treatment to the CQ master, which
    # makes this logic CQ specific. We only use this logic in the CQ anyway at
    # the moment, but may need to reconsider if we need to generalize to other
    # master-slave builds.
    assert self._run.config.name == constants.CQ_MASTER
    config_map[build_id] = constants.CQ_MASTER

    return config_map, action_history

  def GetRelevantChangesForSlaves(self, changes, no_stat):
    """Compile a set of relevant changes for each slave.

    Args:
      changes: A list of GerritPatch instances to examine.
      no_stat: Set of builder names of slave builders that had status None.

    Returns:
      A dictionary mapping a slave config name to a set of relevant changes.
    """
    # Retrieve the slaves and clactions from CIDB.
    config_map, action_history = self._GetSlaveMappingAndCLActions(changes)
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

  def GetSubsysResultForSlaves(self):
    """Get the pass/fail HWTest subsystems results for each slave.

    Returns:
      A dictionary mapping a slave config name to a dictionary of the pass/fail
      subsystems. E.g.
      {'foo-paladin': {'pass_subsystems':{'A', 'B'},
                       'fail_subsystems':{'C'}}}
    """
    # build_id is the master build id for the run
    build_id, db = self._run.GetCIDBHandle()
    assert db, 'No database connection to use.'
    slave_msgs = db.GetSlaveBuildMessages(build_id)
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

  def _ShouldSubmitPartialPool(self):
    """Determine whether we should attempt or skip SubmitPartialPool.

    Returns:
      True if all important, non-sanity-check slaves ran and completed all
      critical stages, and hence it is safe to attempt SubmitPartialPool. False
      otherwise.
    """
    # sanity_check_slaves should not block board-aware submission, since they do
    # not actually apply test patches.
    sanity_check_slaves = set(self._run.config.sanity_check_slaves)
    all_slaves = set([x.name for x in self._GetSlaveConfigs()])
    all_slaves -= sanity_check_slaves
    assert self._run.config.name not in all_slaves

    # Get slave stages.
    build_id, db = self._run.GetCIDBHandle()
    assert db, 'No database connection to use.'
    slave_stages = db.GetSlaveStages(build_id)

    should_submit = True
    ACCEPTED_STATUSES = (constants.BUILDER_STATUS_PASSED,
                         constants.BUILDER_STATUS_SKIPPED,)

    # Configs that have passed critical stages.
    configs_per_stage = {stage: set() for stage in self._CRITICAL_STAGES}

    for stage in slave_stages:
      if (stage['name'] in self._CRITICAL_STAGES and
          stage['status'] in ACCEPTED_STATUSES):
        configs_per_stage[stage['name']].add(stage['build_config'])

    for stage in self._CRITICAL_STAGES:
      missing_configs = all_slaves - configs_per_stage[stage]
      if missing_configs:
        logging.warning('Config(s) %s did not complete critical stage %s.',
                        ' '.join(missing_configs), stage)
        should_submit = False

    return should_submit

  def CQMasterHandleFailure(self, failing, inflight, no_stat):
    """Handle changes in the validation pool upon build failure or timeout.

    This function determines whether to reject CLs and what CLs to
    reject based on the category of the failures and whether the
    sanity check builder(s) passed.

    Args:
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
      no_stat: Set of builder names of slave builders that had status None.
    """
    messages = self._GetFailedMessages(failing)
    self.SendInfraAlertIfNeeded(failing, inflight, no_stat)

    changes = self.sync_stage.pool.applied

    do_partial_submission = self._ShouldSubmitPartialPool()

    if do_partial_submission:
      changes_by_config = self.GetRelevantChangesForSlaves(changes, no_stat)
      subsys_by_config = self.GetSubsysResultForSlaves()

      # Even if there was a failure, we can submit the changes that indicate
      # that they don't care about this failure.
      changes = self.sync_stage.pool.SubmitPartialPool(
          changes, messages, changes_by_config, subsys_by_config,
          failing, inflight, no_stat)
    else:
      logging.warning('Not doing any partial submission, due to critical stage '
                      'failure(s).')
      title = 'CQ encountered a critical failure.'
      msg = ('CQ encountered a critical failure, and hence skipped '
             'board-aware submission. See %s' % self.ConstructDashboardURL())
      tree_status.SendHealthAlert(self._run, title, msg)

    sanity_check_slaves = set(self._run.config.sanity_check_slaves)
    tot_sanity = self._ToTSanity(sanity_check_slaves, self._slave_statuses)

    if not tot_sanity:
      # Sanity check slave failure may have been caused by bug(s)
      # in ToT or broken infrastructure. In any of those cases, we
      # should not reject any changes.
      logging.warning('Detected that a sanity-check builder failed. '
                      'Will not reject any changes.')

    # If the tree was not open when we acquired a pool, do not assume that
    # tot was sane.
    if not self.sync_stage.pool.tree_was_open:
      logging.info('The tree was not open when changes were acquired so we are '
                   'attributing failures to the broken tree rather than the '
                   'changes.')
      tot_sanity = False

    if inflight:
      # Some slave(s) timed out due to unknown causes, so only reject infra
      # changes (probably just chromite changes).
      self.sync_stage.pool.HandleValidationTimeout(sanity=tot_sanity,
                                                   changes=changes)
      return

    # Some builder failed, or some builder did not report stats, or
    # the intersection of both. Let HandleValidationFailure decide
    # what changes to reject.
    self.sync_stage.pool.HandleValidationFailure(
        messages, sanity=tot_sanity, changes=changes, no_stat=no_stat)

  def _GetInfraFailMessages(self, failing):
    """Returns a list of messages containing infra failures.

    Args:
      failing: The names of the failing builders.

    Returns:
      A list of BuildFailureMessage objects.
    """
    msgs = self._GetFailedMessages(failing)
    # Filter out None messages because we cannot analyze them.
    return [x for x in msgs if x and
            x.HasFailureType(failures_lib.InfrastructureFailure)]

  def SendInfraAlertIfNeeded(self, failing, inflight, no_stat):
    """Send infra alerts if needed.

    Args:
      failing: The names of the failing builders.
      inflight: The names of the builders that are still running.
      no_stat: The names of the builders that had status None.
    """
    msgs = [str(x) for x in self._GetInfraFailMessages(failing)]
    # Failed to report a non-None messages is an infra failure.
    slaves = self._GetBuildersWithNoneMessages(failing)
    msgs += ['%s failed with unknown reason.' % x for x in slaves]
    msgs += ['%s timed out' % x for x in inflight]
    msgs += ['%s did not start' % x for x in no_stat]
    if msgs:
      builder_name = self._run.config.name
      title = '%s has encountered infra failures:' % (builder_name,)
      msgs.insert(0, title)
      msgs.append('See %s' % self.ConstructDashboardURL())
      msg = '\n\n'.join(msgs)
      subject = '%s infra failures' % (builder_name,)
      extra_fields = {'X-cbuildbot-alert': 'cq-infra-alert'}
      tree_status.SendHealthAlert(self._run, subject, msg,
                                  extra_fields=extra_fields)

  @staticmethod
  def _ToTSanity(sanity_check_slaves, slave_statuses):
    """Returns False if any sanity check slaves failed.

    Args:
      sanity_check_slaves: Names of slave builders that are "sanity check"
        builders for the current master.
      slave_statuses: Dict of BuilderStatus objects by builder name keys.

    Returns:
      True if no sanity builders ran and failed.
    """
    sanity_check_slaves = sanity_check_slaves or []
    return not any([x in slave_statuses and slave_statuses[x].Failed() for
                    x in sanity_check_slaves])

  def GetIrrelevantChanges(self, board_metadata):
    """Calculates irrelevant changes.

    Args:
      board_metadata: A dictionary of board specific metadata.

    Returns:
      A set of irrelevant changes to the build.
    """
    if not board_metadata:
      return set()
    # changes irrelevant to all the boards are irrelevant to the build
    changeset_per_board_list = list()
    for v in board_metadata.values():
      changes_dict_list = v.get('irrelevant_changes', None)
      if changes_dict_list:
        changes_set = set(cros_patch.GerritFetchOnlyPatch.FromAttrDict(d) for d
                          in changes_dict_list)
        changeset_per_board_list.append(changes_set)
      else:
        # If any board has no irrelevant change, the whole build not have also.
        return set()

    return set.intersection(*changeset_per_board_list)

  def PerformStage(self):
    """Run CommitQueueCompletionStage."""
    if (not self._run.config.master and
        not self._run.config.do_not_apply_cq_patches):
      # Slave needs to record what change are irrelevant to this build.
      board_metadata = self._run.attrs.metadata.GetDict().get('board-metadata')
      irrelevant_changes = self.GetIrrelevantChanges(board_metadata)
      self.sync_stage.pool.RecordIrrelevantChanges(irrelevant_changes)

    super(CommitQueueCompletionStage, self).PerformStage()


class PreCQCompletionStage(generic_stages.BuilderStage):
  """Reports the status of a trybot run to Google Storage and Gerrit."""

  def __init__(self, builder_run, sync_stage, success, **kwargs):
    super(PreCQCompletionStage, self).__init__(builder_run, **kwargs)
    self.sync_stage = sync_stage
    self.success = success

  def GetBuildFailureMessage(self):
    """Returns message summarizing the failures."""
    return CreateBuildFailureMessage(self._run.config.overlays,
                                     self._run.config.name,
                                     self._run.ConstructDashboardURL())

  def PerformStage(self):
    # Update Gerrit and Google Storage with the Pre-CQ status.
    if self.success:
      self.sync_stage.pool.HandlePreCQPerConfigSuccess()
    else:
      message = self.GetBuildFailureMessage()
      self.sync_stage.pool.HandleValidationFailure([message])


class PublishUprevChangesStage(generic_stages.BuilderStage):
  """Makes uprev changes from pfq live for developers."""

  def __init__(self, builder_run, success, stage_push=False, **kwargs):
    """Constructor.

    Args:
      builder_run: BuilderRun object.
      success: Boolean indicating whether the build succeeded.
      stage_push: Indicating whether to stage the push instead of pushing
                  it to master, default to False.
    """
    super(PublishUprevChangesStage, self).__init__(builder_run, **kwargs)
    self.success = success
    self.stage_push = stage_push

  def CheckMasterBinhostTest(self, db, build_id):
    """Check whether the master builder has passed BinhostTest stage.

    Args:
      db: cidb.CIDBConnection object.
      build_id: build_id of the master build to check for.

    Returns:
      True if the status of the master build BinhostTest stage is 'pass';
      else, False.
    """
    stage_name = 'BinhostTest'

    if self._build_stage_id is not None and db is not None:
      stages = db.GetBuildStages(build_id)

      # No stages found. BinhostTest stage didn't start or got skipped,
      # in both case we don't need to push commits to the temp pfq branch.
      if not stages:
        logging.warning('no %s stage found in build %s' % (
            stage_name, build_id))
        return False

      stage_status = [s for s in stages if (
          s['name'] == stage_name and
          s['status'] == constants.BUILDER_STATUS_PASSED)]
      if stage_status:
        logging.info('build %s passed stage %s with %s' % (
            build_id, stage_name, stage_status))
        return True
      else:
        logging.warning('build %s stage %s result %s' % (
            build_id, stage_name, stage_status))
        return False

    logging.warning('Not valid build_stage_id %s or db %s or no %s found' % (
        self._build_stage_id, db, stage_name))
    return False

  def CheckSlaveUploadPrebuiltsTest(self, db, build_id):
    """Check if the slaves have passed UploadPrebuilts stage.

    Given the master build id, check if all the important slaves have passed
    the UploadPrebuilts stage.

    Args:
      db: cidb.CIDBConnection object.
      build_id: build_id of the master build to check for.

    Returns:
      True if all the important slaves have passed the stage;
      True if it's in debug environment;
      else, False.
    """
    stage_name = 'UploadPrebuilts'

    if not self._run.config.master:
      logging.warning('The build is not a master')
      return False
    elif self._run.options.buildbot and self._run.options.debug:
      # If it's in debug environment, no slave builds would be triggered,
      # in order to cover the testing on pushing commits to a remote
      # temp branch, return True.
      logging.info('In debug environment, return CheckSlaveUploadPrebuiltsTest'
                   'as True')
      return True
    elif self._build_stage_id is not None and db is not None:
      slave_configs = self._GetSlaveConfigs()
      important_set = set([slave['name'] for slave in slave_configs])
      stages = db.GetSlaveStages(build_id)

      passed_set = set([s['build_config'] for s in stages if (
          s['name'] == stage_name and
          s['status'] == constants.BUILDER_STATUS_PASSED)])

      if passed_set.issuperset(important_set):
        logging.info('All the important slaves passed %s' % stage_name)
        return True
      else:
        remaining_set = important_set.difference(passed_set)
        logging.warning('slave %s didn\'t pass %s' % (
            remaining_set, stage_name))
        return False
    else:
      logging.warning('Not valid build_stage_id %s or db %s ' % (
          self._build_stage_id, db))
      return False

  def PerformStage(self):
    overlays, push_overlays = self._ExtractOverlays()

    staging_branch = None
    if self.stage_push:
      if not config_lib.IsMasterChromePFQ(self._run.config):
        raise ValueError('This build must be a master chrome PFQ build '
                         'when stage_push is True.')
      build_id, db = self._run.GetCIDBHandle()

      # If the master passed BinHostTest and all the important slaves passed
      # UploadPrebuiltsTest, push uprev commits to a staging_branch.
      if (self.CheckMasterBinhostTest(db, build_id) and
          self.CheckSlaveUploadPrebuiltsTest(db, build_id)):
        staging_branch = ('refs/' + constants.PFQ_REF + '/' +
                          constants.STAGING_PFQ_BRANCH_PREFIX + str(build_id))

    assert push_overlays, 'push_overlays must be set to run this stage'

    # If we're a commit queue, we should clean out our local changes, resync,
    # and reapply our uprevs. This is necessary so that 1) we are sure to point
    # at the remote SHA1s, not our local SHA1s; 2) we can avoid doing a
    # rebase; 3) in the case of failure and staging_branch is None, we don't
    # submit the changes that were committed locally.
    #
    # If we're not a commit queue and the build succeeded, we can skip the
    # cleanup here. This is a cheap trick so that the Chrome PFQ pushes its
    # earlier uprev from the SyncChrome stage (it would be a bit tricky to
    # replicate the uprev here, so we'll leave it alone).

    # If we're not a commit queue and staging_branch is not None, we can skip
    # the cleanup here. When staging_branch is not None, we're going to push
    # the local commits generated in AFDOUpdateEbuild stage to the
    # staging_branch, cleaning up repository here will wipe out the local
    # commits.
    if (config_lib.IsCQType(self._run.config.build_type) or
        not (self.success or staging_branch is not None)):
      # Clean up our root and sync down the latest changes that were
      # submitted.
      commands.BuildRootGitCleanup(self._build_root)

      # Sync down the latest changes we have submitted.
      if self._run.options.sync:
        next_manifest = self._run.config.manifest
        repo = self.GetRepoRepository()
        repo.Sync(next_manifest)

      # Commit an uprev locally.
      if self._run.options.uprev and self._run.config.uprev:
        commands.UprevPackages(self._build_root, self._boards, overlays)

    # When prebuilts is True, if it's a successful run or staging_branch is
    # not None for a master-chrome-pfq run, update binhost conf
    if (self._run.config.prebuilts and
        (self.success or staging_branch is not None)):
      confwriter = prebuilts.BinhostConfWriter(self._run)
      confwriter.Perform()

    # Push the uprev and binhost commits.
    commands.UprevPush(self._build_root, push_overlays,
                       self._run.options.debug,
                       staging_branch=staging_branch)
