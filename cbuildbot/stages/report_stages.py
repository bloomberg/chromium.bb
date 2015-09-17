# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the report stages."""


from __future__ import print_function

import os
import sys

from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import metadata_lib
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import tree_status
from chromite.cbuildbot import triage_lib
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cidb
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import graphite
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import patch as cros_patch
from chromite.lib import portage_util
from chromite.lib import retry_stats
from chromite.lib import toolchain


site_config = config_lib.GetConfig()


def WriteBasicMetadata(builder_run):
  """Writes basic metadata that should be known at start of execution.

  This method writes to |build_run|'s metadata instance the basic metadata
  values that should be known at the beginning of the first cbuildbot
  execution, prior to any reexecutions.

  In particular, this method does not write any metadata values that depend
  on the builder config, as the config may be modified by patches that are
  applied before the final reexectuion.

  This method is safe to run more than once (for instance, once per cbuildbot
  execution) because it will write the same data each time.

  Args:
    builder_run: The BuilderRun instance for this build.
  """
  start_time = results_lib.Results.start_time
  start_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=start_time)

  metadata = {
      # Data for this build.
      'bot-hostname': cros_build_lib.GetHostName(fully_qualified=True),
      'build-number': builder_run.buildnumber,
      'builder-name': builder_run.GetBuilderName(),
      'buildbot-url': os.environ.get('BUILDBOT_BUILDBOTURL', ''),
      'buildbot-master-name':
          os.environ.get('BUILDBOT_MASTERNAME', ''),
      'bot-config': builder_run.config['name'],
      'time': {
          'start': start_time_stamp,
      },
      'master_build_id': builder_run.options.master_build_id,
  }

  builder_run.attrs.metadata.UpdateWithDict(metadata)


def GetChildConfigListMetadata(child_configs, config_status_map):
  """Creates a list for the child configs metadata.

  This creates a list of child config dictionaries from the given child
  configs, optionally adding the final status if the success map is
  specified.

  Args:
    child_configs: The list of child configs for this build.
    config_status_map: The map of config name to final build status.

  Returns:
    List of child config dictionaries, with optional final status
  """
  child_config_list = []
  for c in child_configs:
    pass_fail_status = None
    if config_status_map:
      if config_status_map[c['name']]:
        pass_fail_status = constants.FINAL_STATUS_PASSED
      else:
        pass_fail_status = constants.FINAL_STATUS_FAILED
    child_config_list.append({'name': c['name'],
                              'boards': c['boards'],
                              'status': pass_fail_status})
  return child_config_list


class BuildStartStage(generic_stages.BuilderStage):
  """The first stage to run.

  This stage writes a few basic metadata values that are known at the start of
  build, and inserts the build into the database, if appropriate.
  """

  def _GetBuildTimeoutSeconds(self):
    """Get the overall build timeout to be published to cidb.

    Returns:
      Timeout in seconds. None if no sensible timeout can be inferred.
    """
    timeout_seconds = self._run.options.timeout
    if self._run.config.master:
      master_timeout = constants.MASTER_BUILD_TIMEOUT_SECONDS.get(
          self._run.config.build_type,
          constants.MASTER_BUILD_TIMEOUT_DEFAULT_SECONDS)
      if timeout_seconds > 0:
        master_timeout = min(master_timeout, timeout_seconds)
      return master_timeout

    return timeout_seconds if timeout_seconds > 0 else None

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    if self._run.config['doc']:
      logging.PrintBuildbotLink('Builder documentation',
                                self._run.config['doc'])

    WriteBasicMetadata(self._run)
    d = self._run.attrs.metadata.GetDict()

    # BuildStartStage should only run once per build. But just in case it
    # is somehow running a second time, we do not want to insert an additional
    # database entry. Detect if a database entry has been inserted already
    # and if so quit the stage.
    if 'build_id' in d:
      logging.info('Already have build_id %s, not inserting an entry.',
                   d['build_id'])
      return

    graphite.StatsFactory.GetInstance().Counter('build_started').increment(
        self._run.config['name'] or 'NO_CONFIG')

    # Note: In other build stages we use self._run.GetCIDBHandle to fetch
    # a cidb handle. However, since we don't yet have a build_id, we can't
    # do that here.
    if cidb.CIDBConnectionFactory.IsCIDBSetup():
      db_type = cidb.CIDBConnectionFactory.GetCIDBConnectionType()
      db = cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()
      if db:
        waterfall = d['buildbot-master-name']
        assert waterfall in constants.CIDB_KNOWN_WATERFALLS
        build_id = db.InsertBuild(
            builder_name=d['builder-name'],
            waterfall=waterfall,
            build_number=d['build-number'],
            build_config=d['bot-config'],
            bot_hostname=d['bot-hostname'],
            master_build_id=d['master_build_id'],
            timeout_seconds=self._GetBuildTimeoutSeconds())
        self._run.attrs.metadata.UpdateWithDict({'build_id': build_id,
                                                 'db_type': db_type})
        logging.info('Inserted build_id %s into cidb database type %s.',
                     build_id, db_type)
        logging.PrintBuildbotStepText('database: %s, build_id: %s' %
                                      (db_type, build_id))

        master_build_id = d['master_build_id']
        if master_build_id is not None:
          master_build_status = db.GetBuildStatus(master_build_id)
          master_waterfall_url = constants.WATERFALL_TO_DASHBOARD[
              master_build_status['waterfall']]

          master_url = tree_status.ConstructDashboardURL(
              master_waterfall_url,
              master_build_status['builder_name'],
              master_build_status['build_number'])
          logging.PrintBuildbotLink('Link to master build', master_url)

  def HandleSkip(self):
    """Ensure that re-executions use the same db instance as initial db."""
    metadata_dict = self._run.attrs.metadata.GetDict()
    if 'build_id' in metadata_dict:
      db_type = cidb.CIDBConnectionFactory.GetCIDBConnectionType()
      if not 'db_type' in metadata_dict:
        # This will only execute while this CL is in the commit queue. After
        # this CL lands, this block can be removed.
        self._run.attrs.metadata.UpdateWithDict({'db_type': db_type})
        return

      if db_type != metadata_dict['db_type']:
        cidb.CIDBConnectionFactory.InvalidateCIDBSetup()
        raise AssertionError('Invalid attempt to switch from database %s to '
                             '%s.' % (metadata_dict['db_type'], db_type))


class SlaveFailureSummaryStage(generic_stages.BuilderStage):
  """Stage which summarizes and links to the failures of slave builds."""

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    if not self._run.config.master:
      logging.info('This stage is only meaningful for master builds. '
                   'Doing nothing.')
      return

    build_id, db = self._run.GetCIDBHandle()

    if not db:
      logging.info('No cidb connection for this build. '
                   'Doing nothing.')
      return

    slave_failures = db.GetSlaveFailures(build_id)
    failures_by_build = cros_build_lib.GroupByKey(slave_failures, 'build_id')
    for build_id, build_failures in sorted(failures_by_build.items()):
      failures_by_stage = cros_build_lib.GroupByKey(build_failures,
                                                    'build_stage_id')
      # Surface a link to each slave stage that failed, in stage_id sorted
      # order.
      for stage_id in sorted(failures_by_stage):
        failure = failures_by_stage[stage_id][0]
        # Ignore failures that did not cause their enclosing stage to fail.
        # TODO(akeshet) revisit this approach, if we seem to be suppressing
        # useful information as a result of it.
        if failure['stage_status'] != constants.BUILDER_STATUS_FAILED:
          continue
        waterfall_url = constants.WATERFALL_TO_DASHBOARD[failure['waterfall']]
        slave_stage_url = tree_status.ConstructDashboardURL(
            waterfall_url,
            failure['builder_name'],
            failure['build_number'],
            failure['stage_name'])
        logging.PrintBuildbotLink('%s %s' % (failure['build_config'],
                                             failure['stage_name']),
                                  slave_stage_url)


class BuildReexecutionFinishedStage(generic_stages.BuilderStage,
                                    generic_stages.ArchivingStageMixin):
  """The first stage to run after the final cbuildbot reexecution.

  This stage is the first stage run after the final cbuildbot
  bootstrap/reexecution. By the time this stage is run, the sync stages
  are complete and version numbers of chromeos are known (though chrome
  version may not be known until SyncChrome).

  This stage writes metadata values that are first known after the final
  reexecution (such as those that come from the config). This stage also
  updates the build's cidb entry if appropriate.

  Where possible, metadata that is already known at this time should be
  written at this time rather than in ReportStage.
  """

  def _AbortPreviousHWTestSuites(self):
    """Abort any outstanding synchronous hwtest suites from this builder."""
    # Only try to clean up previous HWTests if this is really running on one of
    # our builders in a non-trybot build.
    debug = (self._run.options.remote_trybot or
             (not self._run.options.buildbot) or
             self._run.options.debug)
    build_id, db = self._run.GetCIDBHandle()
    if db:
      builds = db.GetBuildHistory(self._run.config.name, 2,
                                  ignore_build_id=build_id)
      for build in builds:
        old_version = build['full_version']
        if old_version is None:
          continue
        for suite_config in self._run.config.hw_tests:
          if not suite_config.async:
            commands.AbortHWTests(self._run.config.name, old_version,
                                  debug, suite_config.suite)

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    config = self._run.config
    build_root = self._build_root

    # Flat list of all child config boards. Since child configs
    # are not allowed to have children, it is not necessary to search
    # deeper than one generation.
    child_configs = GetChildConfigListMetadata(
        child_configs=config['child_configs'], config_status_map=None)

    sdk_verinfo = cros_build_lib.LoadKeyValueFile(
        os.path.join(build_root, constants.SDK_VERSION_FILE),
        ignore_missing=True)

    verinfo = self._run.GetVersionInfo()
    platform_tag = getattr(self._run.attrs, 'release_tag')
    if not platform_tag:
      platform_tag = verinfo.VersionString()

    version = {
        'full': self._run.GetVersion(),
        'milestone': verinfo.chrome_branch,
        'platform': platform_tag,
    }

    metadata = {
        # Version of the metadata format.
        'metadata-version': '2',
        'boards': config['boards'],
        'child-configs': child_configs,
        'build_type': config['build_type'],

        # Data for the toolchain used.
        'sdk-version': sdk_verinfo.get('SDK_LATEST_VERSION', '<unknown>'),
        'toolchain-url': sdk_verinfo.get('TC_PATH', '<unknown>'),
    }

    if len(config['boards']) == 1:
      toolchains = toolchain.GetToolchainsForBoard(config['boards'][0],
                                                   buildroot=build_root)
      metadata['toolchain-tuple'] = (
          toolchain.FilterToolchains(toolchains, 'default', True).keys() +
          toolchain.FilterToolchains(toolchains, 'default', False).keys())

    logging.info('Metadata being written: %s', metadata)
    self._run.attrs.metadata.UpdateWithDict(metadata)
    # Update 'version' separately to avoid overwriting the existing
    # entries in it (e.g. PFQ builders may have written the Chrome
    # version to uprev).
    logging.info("Metadata 'version' being written: %s", version)
    self._run.attrs.metadata.UpdateKeyDictWithDict('version', version)

    # Ensure that all boards and child config boards have a per-board
    # metadata subdict.
    for b in config['boards']:
      self._run.attrs.metadata.UpdateBoardDictWithDict(b, {})

    for cc in child_configs:
      for b in cc['boards']:
        self._run.attrs.metadata.UpdateBoardDictWithDict(b, {})

    # Upload build metadata (and write it to database if necessary)
    self.UploadMetadata(filename=constants.PARTIAL_METADATA_JSON)

    # Write child-per-build and board-per-build rows to database
    build_id, db = self._run.GetCIDBHandle()
    if db:
      # TODO(akeshet): replace this with a GetValue call once crbug.com/406522
      # is resolved
      per_board_dict = self._run.attrs.metadata.GetDict()['board-metadata']
      for board, board_metadata in per_board_dict.items():
        db.InsertBoardPerBuild(build_id, board)
        if board_metadata:
          db.UpdateBoardPerBuildMetadata(build_id, board, board_metadata)
      for child_config in self._run.attrs.metadata.GetValue('child-configs'):
        db.InsertChildConfigPerBuild(build_id, child_config['name'])

      # If this build has a master build, ensure that the master full_version
      # is the same as this build's full_version. This is a sanity check to
      # avoid bugs in master-slave logic.
      master_id = self._run.attrs.metadata.GetDict().get('master_build_id')
      if master_id is not None:
        master_full_version = db.GetBuildStatus(master_id)['full_version']
        my_full_version = self._run.attrs.metadata.GetValue('version').get(
            'full')
        if master_full_version != my_full_version:
          raise failures_lib.MasterSlaveVersionMismatchFailure(
              'Master build id %s has full_version %s, while slave version is '
              '%s.' % (master_id, master_full_version, my_full_version))

    # Abort previous hw test suites. This happens after reexecution as it
    # requires chromite/third_party/swarming.client, which is not available
    # untill after reexecution.
    self._AbortPreviousHWTestSuites()


class ReportStage(generic_stages.BuilderStage,
                  generic_stages.ArchivingStageMixin):
  """Summarize all the builds."""

  _HTML_HEAD = """<html>
<head>
 <title>Archive Index: %(board)s / %(version)s</title>
</head>
<body>
<h2>Artifacts Index: %(board)s / %(version)s (%(config)s config)</h2>"""

  def __init__(self, builder_run, completion_instance, **kwargs):
    super(ReportStage, self).__init__(builder_run, **kwargs)

    # TODO(mtennant): All these should be retrieved from builder_run instead.
    # Or, more correctly, the info currently retrieved from these stages should
    # be stored and retrieved from builder_run instead.
    self._completion_instance = completion_instance

  def _UpdateRunStreak(self, builder_run, final_status):
    """Update the streak counter for this builder, if applicable, and notify.

    Update the pass/fail streak counter for the builder.  If the new
    streak should trigger a notification email then send it now.

    Args:
      builder_run: BuilderRun for this run.
      final_status: Final status string for this run.
    """
    if builder_run.InProduction():
      streak_value = self._UpdateStreakCounter(
          final_status=final_status, counter_name=builder_run.config.name,
          dry_run=self._run.debug)
      verb = 'passed' if streak_value > 0 else 'failed'
      logging.info('Builder %s has %s %s time(s) in a row.',
                   builder_run.config.name, verb, abs(streak_value))
      # See if updated streak should trigger a notification email.
      if (builder_run.config.health_alert_recipients and
          builder_run.config.health_threshold > 0 and
          streak_value <= -builder_run.config.health_threshold):
        logging.info('Builder failed %i consecutive times, sending health '
                     'alert email to %s.', -streak_value,
                     builder_run.config.health_alert_recipients)

        subject = '%s health alert' % builder_run.config.name
        body = self._HealthAlertMessage(-streak_value)
        extra_fields = {'X-cbuildbot-alert': 'cq-health'}
        tree_status.SendHealthAlert(builder_run, subject, body,
                                    extra_fields=extra_fields)

  def _UpdateStreakCounter(self, final_status, counter_name,
                           dry_run=False):
    """Update the given streak counter based on the final status of build.

    A streak counter counts the number of consecutive passes or failures of
    a particular builder. Consecutive passes are indicated by a positive value,
    consecutive failures by a negative value.

    Args:
      final_status: String indicating final status of build,
                    constants.FINAL_STATUS_PASSED indicating success.
      counter_name: Name of counter to increment, typically the name of the
                    build config.
      dry_run: Pretend to update counter only. Default: False.

    Returns:
      The new value of the streak counter.
    """
    gs_ctx = gs.GSContext(dry_run=dry_run)
    counter_url = os.path.join(site_config.params.MANIFEST_VERSIONS_GS_URL,
                               constants.STREAK_COUNTERS,
                               counter_name)
    gs_counter = gs.GSCounter(gs_ctx, counter_url)

    if final_status == constants.FINAL_STATUS_PASSED:
      streak_value = gs_counter.StreakIncrement()
    else:
      streak_value = gs_counter.StreakDecrement()

    return streak_value

  def _HealthAlertMessage(self, fail_count):
    """Returns the body of a health alert email message."""
    return 'The builder named %s has failed %i consecutive times. See %s' % (
        self._run.config['name'], fail_count, self.ConstructDashboardURL())

  def _SendPreCQInfraAlertMessageIfNeeded(self):
    """Send alerts on Pre-CQ infra failures."""
    msg = completion_stages.CreateBuildFailureMessage(
        self._run.config.overlays,
        self._run.config.name,
        self._run.ConstructDashboardURL())
    pre_cq = self._run.config.pre_cq
    if pre_cq and msg.HasFailureType(failures_lib.InfrastructureFailure):
      name = self._run.config.name
      title = 'pre-cq infra failures'
      body = ['%s failed on %s' % (name, cros_build_lib.GetHostName()),
              '%s' % msg]
      extra_fields = {'X-cbuildbot-alert': 'pre-cq-infra-alert'}
      tree_status.SendHealthAlert(self._run, title, '\n\n'.join(body),
                                  extra_fields=extra_fields)

  def _UploadMetadataForRun(self, final_status):
    """Upload metadata.json for this entire run.

    Args:
      final_status: Final status string for this run.
    """
    self._run.attrs.metadata.UpdateWithDict(
        self.GetReportMetadata(final_status=final_status,
                               completion_instance=self._completion_instance))
    self.UploadMetadata()

  def _UploadArchiveIndex(self, builder_run):
    """Upload an HTML index for the artifacts at remote archive location.

    If there are no artifacts in the archive then do nothing.

    Args:
      builder_run: BuilderRun object for this run.

    Returns:
      If an index file is uploaded then a dict is returned where each value
        is the same (the URL for the uploaded HTML index) and the keys are
        the boards it applies to, including None if applicable.  If no index
        file is uploaded then this returns None.
    """
    archive = builder_run.GetArchive()
    archive_path = archive.archive_path

    config = builder_run.config
    boards = config.boards
    if boards:
      board_names = ' '.join(boards)
    else:
      boards = [None]
      board_names = '<no board>'

    # See if there are any artifacts found for this run.
    uploaded = os.path.join(archive_path, commands.UPLOADED_LIST_FILENAME)
    if not os.path.exists(uploaded):
      # UPLOADED doesn't exist.  Normal if Archive stage never ran, which
      # is possibly normal.  Regardless, no archive index is needed.
      logging.info('No archived artifacts found for %s run (%s)',
                   builder_run.config.name, board_names)

    else:
      # Prepare html head.
      head_data = {
          'board': board_names,
          'config': config.name,
          'version': builder_run.GetVersion(),
      }
      head = self._HTML_HEAD % head_data

      files = osutils.ReadFile(uploaded).splitlines() + [
          '.|Google Storage Index',
          '..|',
      ]
      index = os.path.join(archive_path, 'index.html')
      # TODO (sbasi) crbug.com/362776: Rework the way we do uploading to
      # multiple buckets. Currently this can only be done in the Archive Stage
      # therefore index.html will only end up in the normal Chrome OS bucket.
      commands.GenerateHtmlIndex(index, files, url_base=archive.download_url,
                                 head=head)
      commands.UploadArchivedFile(
          archive_path, [archive.upload_url], os.path.basename(index),
          debug=self._run.debug, acl=self.acl)
      return dict((b, archive.download_url + '/index.html') for b in boards)

  def GetReportMetadata(self, config=None, stage=None, final_status=None,
                        completion_instance=None):
    """Generate ReportStage metadata.

    Args:
      config: The build config for this run.  Defaults to self._run.config.
      stage: The stage name that this metadata file is being uploaded for.
      final_status: Whether the build passed or failed. If None, the build
        will be treated as still running.
      completion_instance: The stage instance that was used to wait for slave
        completion. Used to add slave build information to master builder's
        metadata. If None, no such status information will be included. It not
        None, this should be a derivative of MasterSlaveSyncCompletionStage.

    Returns:
      A JSON-able dictionary representation of the metadata object.
    """
    builder_run = self._run
    config = config or builder_run.config

    get_statuses_from_slaves = (
        config['master'] and
        completion_instance and
        isinstance(completion_instance,
                   completion_stages.MasterSlaveSyncCompletionStage)
    )

    child_configs_list = GetChildConfigListMetadata(
        child_configs=config['child_configs'],
        config_status_map=completion_stages.GetBuilderSuccessMap(self._run,
                                                                 final_status))

    return metadata_lib.CBuildbotMetadata.GetReportMetadataDict(
        builder_run, get_statuses_from_slaves,
        config, stage, final_status, completion_instance,
        child_configs_list)

  def ArchiveResults(self, final_status):
    """Archive our build results.

    Args:
      final_status: constants.FINAL_STATUS_PASSED or
                    constants.FINAL_STATUS_FAILED

    Returns:
      A dictionary with the aggregated _UploadArchiveIndex results.
    """
    # Make sure local archive directory is prepared, if it was not already.
    if not os.path.exists(self.archive_path):
      self.archive.SetupArchivePath()

    # Upload metadata, and update the pass/fail streak counter for the main
    # run only. These aren't needed for the child builder runs.
    self._UploadMetadataForRun(final_status)
    self._UpdateRunStreak(self._run, final_status)

    # Alert if the Pre-CQ has infra failures.
    if final_status == constants.FINAL_STATUS_FAILED:
      self._SendPreCQInfraAlertMessageIfNeeded()

    # Iterate through each builder run, whether there is just the main one
    # or multiple child builder runs.
    archive_urls = {}
    for builder_run in self._run.GetUngroupedBuilderRuns():
      # Generate an index for archived artifacts if there are any.  All the
      # archived artifacts for one run/config are in one location, so the index
      # is only specific to each run/config.  In theory multiple boards could
      # share that archive, but in practice it is usually one board.  A
      # run/config without a board will also usually not have artifacts to
      # archive, but that restriction is not assumed here.
      run_archive_urls = self._UploadArchiveIndex(builder_run)
      if run_archive_urls:
        archive_urls.update(run_archive_urls)
        # Check if the builder_run is tied to any boards and if so get all
        # upload urls.
        if final_status == constants.FINAL_STATUS_PASSED:
          # Update the LATEST files if the build passed.
          try:
            upload_urls = self._GetUploadUrls(
                'LATEST-*', builder_run=builder_run)
          except portage_util.MissingOverlayException as e:
            # If the build failed prematurely, some overlays might be
            # missing. Ignore them in this stage.
            logging.warning(e)
          else:
            archive = builder_run.GetArchive()
            archive.UpdateLatestMarkers(builder_run.manifest_branch,
                                        builder_run.debug,
                                        upload_urls=upload_urls)

    return archive_urls

  def PerformStage(self):
    """Perform the actual work for this stage.

    This includes final metadata archival, and update CIDB with our final status
    as well as producting a logged build result summary.
    """
    if results_lib.Results.BuildSucceededSoFar():
      final_status = constants.FINAL_STATUS_PASSED
    else:
      final_status = constants.FINAL_STATUS_FAILED

    if not hasattr(self._run.attrs, 'release_tag'):
      # If, for some reason, sync stage was not completed and
      # release_tag was not set. Set it to None here because
      # ArchiveResults() depends the existence of this attr.
      self._run.attrs.release_tag = None

    # Some operations can only be performed if a valid version is available.
    try:
      self._run.GetVersionInfo()
      archive_urls = self.ArchiveResults(final_status)
      metadata_url = os.path.join(self.upload_url, constants.METADATA_JSON)
    except cbuildbot_run.VersionNotSetError:
      logging.error('A valid version was never set for this run. '
                    'Can not archive results.')
      archive_urls = ''
      metadata_url = ''


    results_lib.Results.Report(
        sys.stdout, archive_urls=archive_urls,
        current_version=(self._run.attrs.release_tag or ''))

    retry_stats.ReportStats(sys.stdout)

    build_id, db = self._run.GetCIDBHandle()
    if db:
      # TODO(akeshet): Eliminate this status string translate once
      # these differing status strings are merged, crbug.com/318930
      translateStatus = lambda s: (constants.BUILDER_STATUS_PASSED
                                   if s == constants.FINAL_STATUS_PASSED
                                   else constants.BUILDER_STATUS_FAILED)
      status_for_db = translateStatus(final_status)

      child_metadatas = self._run.attrs.metadata.GetDict().get(
          'child-configs', [])
      for child_metadata in child_metadatas:
        db.FinishChildConfig(build_id, child_metadata['name'],
                             translateStatus(child_metadata['status']))

      # TODO(pprabhu): After BuildData and CBuildbotMetdata are merged, remove
      # this extra temporary object creation.
      # XXX:HACK We're creating a BuildData with an empty URL. Don't try to
      # MarkGathered this object.
      build_data = metadata_lib.BuildData("",
                                          self._run.attrs.metadata.GetDict())
      # TODO(akeshet): Find a clearer way to get the "primary upload url" for
      # the metadata.json file. One alternative is _GetUploadUrls(...)[0].
      # Today it seems that element 0 of its return list is the primary upload
      # url, but there is no guarantee or unit test coverage of that.
      db.FinishBuild(build_id, status=status_for_db,
                     summary=build_data.failure_message,
                     metadata_url=metadata_url)


class RefreshPackageStatusStage(generic_stages.BuilderStage):
  """Stage for refreshing Portage package status in online spreadsheet."""

  def PerformStage(self):
    commands.RefreshPackageStatus(buildroot=self._build_root,
                                  boards=self._boards,
                                  debug=self._run.options.debug)


class DetectIrrelevantChangesStage(generic_stages.BoardSpecificBuilderStage):
  """Stage to detect irrelevant changes for slave per board base.

  This stage will get the irrelevant changes for the current board of the build,
  and record the irrelevant changes and the subsystem of the relevant changes
  test to board_metadata.
  """

  def __init__(self, builder_run, board, changes, suffix=None, **kwargs):
    super(DetectIrrelevantChangesStage, self).__init__(builder_run, board,
                                                       suffix=suffix, **kwargs)
    # changes is a list of GerritPatch instances.
    self.changes = changes

  def _GetIrrelevantChangesBoardBase(self, changes):
    """Calculates irrelevant changes to the current board.

    Returns:
      A subset of |changes| which are irrelevant to current board.
    """
    manifest = git.ManifestCheckout.Cached(self._build_root)
    packages = self._GetPackagesUnderTestForCurrentBoard()

    irrelevant_changes = triage_lib.CategorizeChanges.GetIrrelevantChanges(
        changes, self._run.config, self._build_root, manifest, packages)
    return irrelevant_changes

  def _GetPackagesUnderTestForCurrentBoard(self):
    """Get a list of packages used in this build for current board.

    Returns:
      A set of packages used in this build. E.g.,
      set(['chromeos-base/chromite-0.0.1-r1258']); returns None if
      the information is missing for any board in the current config.
    """
    packages_under_test = set()

    for run in [self._run] + self._run.GetChildren():
      board_runattrs = run.GetBoardRunAttrs(self._current_board)
      if not board_runattrs.HasParallel('packages_under_test'):
        logging.warning('Packages under test were not recorded correctly')
        return None
      packages_under_test.update(
          board_runattrs.GetParallel('packages_under_test'))

    return packages_under_test

  def GetSubsystemToTest(self, relevant_changes):
    """Get subsystems from relevant cls for current board, write to BOARD_ATTRS.

    Args:
      relevant_changes: A set of changes that are relevant to current board.

    Returns:
      A set of the subsystems. An empty set indicates that all subsystems should
      be tested.
    """
    # Go through all the relevant changes, collect subsystem info from them. If
    # there exists a change without subsystem info, we assume it affects all
    # subsystems. Then set the superset of all the subsystems to be empty, which
    # means that need to test all subsystems.
    subsystem_set = set()
    for change in relevant_changes:
      sys_lst = triage_lib.GetTestSubsystemForChange(self._build_root, change)
      if sys_lst:
        subsystem_set = subsystem_set.union(sys_lst)
      else:
        subsystem_set = set()
        break

    return subsystem_set

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    """Run DetectIrrelevantChangesStage."""
    irrelevant_changes = None
    if not self._run.config.master:
      # Slave writes the irrelevant changes to current board to metadata.
      irrelevant_changes = self._GetIrrelevantChangesBoardBase(self.changes)
      change_dict_list = [c.GetAttributeDict() for c in irrelevant_changes]
      change_dict_list = sorted(change_dict_list,
                                key=lambda x: (x[cros_patch.ATTR_GERRIT_NUMBER],
                                               x[cros_patch.ATTR_PATCH_NUMBER],
                                               x[cros_patch.ATTR_REMOTE]))

      self._run.attrs.metadata.UpdateBoardDictWithDict(
          self._current_board, {'irrelevant_changes': change_dict_list})

    if irrelevant_changes:
      relevant_changes = list(set(self.changes) - irrelevant_changes)
      logging.info('Below are the irrelevant changes for board: %s.',
                   self._current_board)
      (validation_pool.ValidationPool.
       PrintLinksToChanges(list(irrelevant_changes)))
    else:
      relevant_changes = self.changes

    subsystem_set = self.GetSubsystemToTest(relevant_changes)
    logging.info('Subsystems need to be tested: %s. Empty set represents '
                 'testing all subsystems.', subsystem_set)
    # Record subsystems to metadata
    self._run.attrs.metadata.UpdateBoardDictWithDict(
        self._current_board, {'subsystems_to_test': list(subsystem_set)})
