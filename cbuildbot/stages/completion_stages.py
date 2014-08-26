# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the completion stages."""

import logging

from chromite.cbuildbot import commands
from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import portage_utilities
from chromite.cbuildbot import tree_status
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import alerts
from chromite.lib import cros_build_lib
from chromite.lib import git



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

  def _GetBuilderSuccessMap(self):
    """Get the pass/fail status of all builders.

    A builder is marked as passed if all of its steps ran all of the way to
    completion. We determine this by looking at whether all of the steps for
    all of the constituent boards ran to completion.

    In cases where a builder does not have any boards, or has child boards, we
    fall back and instead just look at whether the entire build was successful.

    Args:
      config_name: The name of the builder we wish to get the status of.

    Returns:
      A dict, mapping the builder names to whether they succeeded.
    """
    success_map = {}
    for run in [self._run] + self._run.GetChildren():
      if run.config.boards and not run.config.child_configs:
        success_map[run.config.name] = True
        for board in run.config.boards:
          board_runattrs = run.GetBoardRunAttrs(board)
          if not board_runattrs.HasParallel('success'):
            success_map[run.config.name] = False
      else:
        # If a builder does not have boards, or if it has child configs, we
        # will just use the overall status instead.
        success_map[run.config.name] = self.success
    return success_map

  def PerformStage(self):
    if not self.success:
      self.message = self.GetBuildFailureMessage()

    if not cbuildbot_config.IsPFQType(self._run.config.build_type):
      # Update the pass/fail status in the manifest-versions
      # repo. Suite scheduler checks the build status to schedule
      # suites.
      self._run.attrs.manifest_manager.UpdateStatus(
          success_map=self._GetBuilderSuccessMap(), message=self.message,
          dashboard_url=self.ConstructDashboardURL())


class ImportantBuilderFailedException(failures_lib.StepFailure):
  """Exception thrown when an important build fails to build."""


class MasterSlaveSyncCompletionStage(ManifestVersionedSyncCompletionStage):
  """Stage that records whether we passed or failed to build/test manifest."""

  # Max wait time for results from slaves.
  SLAVE_STATUS_TIMEOUT_SECONDS = 4 * 60 * 60
  # Max wait time for results for PFQ type builders. Note that this
  # does not include Chrome PFQ or CQ.
  PFQ_SLAVE_STATUS_TIMEOUT_SECONDS = 20 * 60
  SLAVE_CHECKING_PERIOD_SECONDS = constants.SLEEP_TIMEOUT


  def __init__(self, *args, **kwargs):
    super(MasterSlaveSyncCompletionStage, self).__init__(*args, **kwargs)
    self._slave_statuses = {}

  def _FetchSlaveStatuses(self):
    """Fetch and return build status for slaves of this build.

    If this build is not a master then return just the status of this build.

    Returns:
      A dict with "bot id" keys and BuilderStatus objects for values.  All keys
      will have valid BuilderStatus values, but builders that never started
      will have a BuilderStatus with status MISSING.
    """
    if not self._run.config.master:
      # This is a slave build, so return the status for this build.
      if self._run.options.debug:
        # In debug mode, nothing is uploaded to Google Storage, so we bypass
        # the extra hop and just look at what we have locally.
        status = manifest_version.BuilderStatus.GetCompletedStatus(self.success)
        status_obj = manifest_version.BuilderStatus(status, self.message)
        return {self._bot_id: status_obj}
      else:
        # Slaves only need to look at their own status.
        return self._run.attrs.manifest_manager.GetBuildersStatus(
            [self._bot_id])
    else:
      # This is a master build, so wait for all the slaves to finish
      # and return their statuses.
      if self._run.options.debug:
        # For debug runs, wait for three minutes to ensure most code
        # paths are executed.
        timeout = 3 * 60
      elif self._run.config.build_type == constants.PFQ_TYPE:
        timeout = self.PFQ_SLAVE_STATUS_TIMEOUT_SECONDS
      else:
        timeout = self.SLAVE_STATUS_TIMEOUT_SECONDS

      builders = self._GetSlaveConfigs()
      builder_names = [b['name'] for b in builders]

      manager = self._run.attrs.manifest_manager
      if sync_stages.MasterSlaveLKGMSyncStage.sub_manager:
        manager = sync_stages.MasterSlaveLKGMSyncStage.sub_manager

      return manager.GetBuildersStatus(builder_names, timeout=timeout)

  def _HandleStageException(self, exc_info):
    """Decide whether an exception should be treated as fatal."""
    # Besides the master, the completion stages also run on slaves, to report
    # their status back to the master. If the build failed, they throw an
    # exception here. For slave builders, marking this stage 'red' would be
    # redundant, since the build itself would already be red. In this case,
    # report a warning instead.
    # pylint: disable=W0212
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
        cbuildbot_config.IsPFQType(self._run.config.build_type) and
        self._run.config.master and
        self._run.manifest_branch == 'master' and
        self._run.config.build_type != constants.CHROME_PFQ_TYPE):
      self._run.attrs.manifest_manager.PromoteCandidate()
      if sync_stages.MasterSlaveLKGMSyncStage.sub_manager:
        sync_stages.MasterSlaveLKGMSyncStage.sub_manager.PromoteCandidate()

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
      cros_build_lib.PrintBuildbotStepWarnings()

    if failing:
      cros_build_lib.Warning('\n'.join([
          'The following builders failed with this manifest:',
          ', '.join(sorted(failing)),
          'Please check the logs of the failing builders for details.']))

    if inflight:
      cros_build_lib.Warning('\n'.join([
          'The following builders took too long to finish:',
          ', '.join(sorted(inflight)),
          'Please check the logs of these builders for details.']))

    if no_stat:
      cros_build_lib.Warning('\n'.join([
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

        cros_build_lib.PrintBuildbotLink(text, statuses[builder].dashboard_url)

    for builder in no_stat:
      cros_build_lib.PrintBuildbotStepText('%s did not start.' % builder)

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

  def ShouldDisableAlerts(self):
    """Return whether alerts should be disabled due to debug mode.

    This method only exists so that it can be overridden by tests.
    """
    return self._run.debug

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
  # This is used in MasterSlaveSyncCompletionStage._FetchSlaveStatuses()
  # as the max wait time for results from slaves. Canaries are scheduled
  # to run every 8 hours, so this timeout must be smaller than that.
  SLAVE_STATUS_TIMEOUT_SECONDS = (7 * 60 + 50) * 60

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
    msgs = [str(x) for x in self._GetFailedMessages(failing)]
    slaves = self._GetBuildersWithNoneMessages(failing)
    msgs += ['%s failed with unknown reason.' % x for x in slaves]
    msgs += ['%s timed out' % x for x in inflight]
    msgs += ['%s did not start' % x for x in no_stat]
    builder_name = self._run.config.name
    title = '%s has encountered failures:' % (builder_name,)
    msgs.insert(0, title)
    msgs.append('See %s' % self.ConstructDashboardURL())
    msg = '\n\n'.join(msgs)
    if not self.ShouldDisableAlerts():
      alerts.SendEmail('%s failures' % (builder_name,),
                       tree_status.GetHealthAlertRecipients(self._run),
                       message=msg,
                       smtp_server=constants.GOLO_SMTP_SERVER,
                       extra_fields={'X-cbuildbot-alert': 'canary-fail-alert'})

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
        ('did not start', no_stat),
        ('timed out', inflight),
        ('failed', failing),]
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
      # Use "cbuildbot" on "Canary master" so that it is a clickable
      # link on the waterfall.
      tree_status.ThrottleOrCloseTheTree(
          '"cbuildbot" on "Canary master"',
          self._ComposeTreeStatusMessage(failing, inflight, no_stat))


class CommitQueueCompletionStage(MasterSlaveSyncCompletionStage):
  """Commits or reports errors to CL's that failed to be validated."""

  def _HandleStageException(self, exc_info):
    """Decide whether an exception should be treated as fatal."""
    exc_type = exc_info[0]
    if isinstance(
        exc_type, validation_pool.FailedToSubmitAllChangesNonFatalException):
      return self._HandleExceptionAsWarning(exc_info)
    else:
      return super(CommitQueueCompletionStage, self)._HandleStageException(
          exc_info)

  def _AbortCQHWTests(self):
    """Abort any HWTests started by the CQ."""
    if (cbuildbot_config.IsCQType(self._run.config.build_type) and
        self._run.manifest_branch == 'master'):
      version = self._run.GetVersion()
      if not commands.HaveCQHWTestsBeenAborted(version):
        commands.AbortCQHWTests(version, self._run.options.debug)

  def HandleSuccess(self):
    if self._run.config.master:
      self.sync_stage.pool.SubmitPool()
      # After submitting the pool, update the commit hashes for uprevved
      # ebuilds.
      manifest = git.ManifestCheckout.Cached(self._build_root)
      portage_utilities.EBuild.UpdateCommitHashesForChanges(
          self.sync_stage.pool.changes, self._build_root, manifest)
      if cbuildbot_config.IsPFQType(self._run.config.build_type):
        super(CommitQueueCompletionStage, self).HandleSuccess()

  def SubmitPartialPool(self, messages):
    """Submit partial pool if possible.

    Args:
      messages: A list of BuildFailureMessage or NoneType objects from
        the failed slaves.

    Returns:
      The changes that were not submitted.
    """
    tracebacks = set()
    for message in messages:
      # If there are no tracebacks, that means that the builder did not
      # report its status properly. Don't submit anything.
      if not message or not message.tracebacks:
        break
      tracebacks.update(message.tracebacks)
    else:
      # SubmitPartialPool submit some changes (if it is applicable),
      # and returns changes that were not submitted.
      return self.sync_stage.pool.SubmitPartialPool(tracebacks)

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

    # Abort hardware tests to save time if we have already seen a failure,
    # except in the case where the only failure is a hardware test failure.
    #
    # When we're debugging hardware test failures, it's useful to see the
    # results on all platforms, to see if the failure is platform-specific.
    tracebacks = results_lib.Results.GetTracebacks()
    if not self.success and self._run.config['important']:
      if len(tracebacks) != 1 or tracebacks[0].failed_prefix != 'HWTest':
        self._AbortCQHWTests()

    if self._run.config.master:
      self.CQMasterHandleFailure(failing, inflight, no_stat)

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
    # Start with all the changes in the validation pool.
    changes = self.sync_stage.pool.changes

    self.SendInfraAlertIfNeeded(failing, inflight, no_stat)

    if failing and not inflight:
      # Even if there was a failure, we can submit the changes that indicate
      # that they don't care about this failure.
      changes = self.SubmitPartialPool(messages)

    tot_sanity = self._ToTSanity(
        self._run.config.sanity_check_slaves, self._slave_statuses)

    if not tot_sanity:
      # Sanity check slave failure may have been caused by bug(s)
      # in ToT or broken infrastructure. In any of those cases, we
      # should not reject any changes.
      logging.warning('Detected that a sanity-check builder failed. '
                      'Will not reject any changes.')

    if inflight:
      # Some slave(s) timed out due to unknown causes. We don't have
      # any more information, so reject all changes.
      # TODO: We should revise on how to handle timeouts.
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
      if not self.ShouldDisableAlerts():
        alerts.SendEmail('%s infra failures' % (builder_name,),
                         tree_status.GetHealthAlertRecipients(self._run),
                         message=msg,
                         smtp_server=constants.GOLO_SMTP_SERVER,
                         extra_fields={'X-cbuildbot-alert': 'cq-infra-alert'})

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
      self.sync_stage.pool.HandlePreCQSuccess()
    else:
      message = self.GetBuildFailureMessage()
      self.sync_stage.pool.HandleValidationFailure([message])


class PublishUprevChangesStage(generic_stages.BuilderStage):
  """Makes uprev changes from pfq live for developers."""

  def __init__(self, builder_run, success, **kwargs):
    """Constructor.

    Args:
      builder_run: BuilderRun object.
      success: Boolean indicating whether the build succeeded.
    """
    super(PublishUprevChangesStage, self).__init__(builder_run, **kwargs)
    self.success = success

  def PerformStage(self):
    overlays, push_overlays = self._ExtractOverlays()
    assert push_overlays, 'push_overlays must be set to run this stage'

    # If the build failed, we don't want to push our local changes, because
    # they might include some CLs that failed. Instead, clean up our local
    # changes and do a fresh uprev.
    if not self.success:
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

    # Push the uprev commit.
    commands.UprevPush(self._build_root, push_overlays, self._run.options.debug)
