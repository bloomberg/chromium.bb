# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the report stages."""

import logging
import os
import sys

from chromite.cbuildbot import cbuildbot_commands as commands
from chromite.cbuildbot import cbuildbot_metadata
from chromite.cbuildbot import cbuildbot_results as results_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import alerts
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import toolchain


class ReportBuildStartStage(generic_stages.BuilderStage,
                            generic_stages.ArchivingStageMixin):
  """Uploads partial metadata artifact describing what will be built.

  This stage should be the first stage run after the final cbuildbot
  bootstrap/reexecution. By the time this stage is run, all sync stages
  are complete and version numbers of chrome and chromeos are known.

  Where possible, metadata that is already known at this time should be
  written at this time rather than in ReportStage.
  """
  def init(self, builder_run, **kwargs):
    super(ReportBuildStartStage, self).__init__(builder_run, **kwargs)

  def PerformStage(self):
    config = self._run.config
    build_root = self._build_root

    # Flat list of all child config boards. Since child configs
    # are not allowed to have children, it is not necessary to search
    # deeper than one generation.
    child_configs = [{'name': c['name'], 'boards' : c['boards']}
                     for c in config['child_configs']]

    sdk_verinfo = cros_build_lib.LoadKeyValueFile(
        os.path.join(build_root, constants.SDK_VERSION_FILE),
        ignore_missing=True)

    start_time = results_lib.Results.start_time
    start_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=start_time)

    verinfo = self._run.GetVersionInfo(build_root)
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
        # Data for this build.
        'bot-config': config['name'],
        'bot-hostname': cros_build_lib.GetHostName(fully_qualified=True),
        'boards': config['boards'],
        'build-number': self._run.buildnumber,
        'builder-name': os.environ.get('BUILDBOT_BUILDERNAME', ''),
        'child-configs': child_configs,
        'time': {
            'start': start_time_stamp,
        },
        'version': version,

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
    self.UploadMetadata(filename=constants.PARTIAL_METADATA_JSON)


class ReportStage(generic_stages.BuilderStage,
                  generic_stages.ArchivingStageMixin):
  """Summarize all the builds."""

  _HTML_HEAD = """<html>
<head>
 <title>Archive Index: %(board)s / %(version)s</title>
</head>
<body>
<h2>Artifacts Index: %(board)s / %(version)s (%(config)s config)</h2>"""

  def __init__(self, builder_run, sync_instance, completion_instance, **kwargs):
    super(ReportStage, self).__init__(builder_run, **kwargs)

    # TODO(mtennant): All these should be retrieved from builder_run instead.
    # Or, more correctly, the info currently retrieved from these stages should
    # be stored and retrieved from builder_run instead.
    self._sync_instance = sync_instance
    self._completion_instance = completion_instance

  def _UpdateRunStreak(self, builder_run, final_status):
    """Update the streak counter for this builder, if applicable, and notify.

    Update the pass/fail streak counter for the builder.  If the new
    streak should trigger a notification email then send it now.

    Args:
      builder_run: BuilderRun for this run.
      final_status: Final status string for this run.
    """

    # Exclude tryjobs from streak counting.
    if not builder_run.options.remote_trybot and not builder_run.options.local:
      streak_value = self._UpdateStreakCounter(
          final_status=final_status, counter_name=builder_run.config.name,
          dry_run=self._run.debug)
      verb = 'passed' if streak_value > 0 else 'failed'
      cros_build_lib.Info('Builder %s has %s %s time(s) in a row.',
                          builder_run.config.name, verb, abs(streak_value))
      # See if updated streak should trigger a notification email.
      if (builder_run.config.health_alert_recipients and
          streak_value <= -builder_run.config.health_threshold):
        cros_build_lib.Info(
            'Builder failed %i consecutive times, sending health alert email '
            'to %s.',
            -streak_value,
            builder_run.config.health_alert_recipients)

        if not self._run.debug:
          alerts.SendEmail('%s health alert' % builder_run.config.name,
                           builder_run.config.health_alert_recipients,
                           message=self._HealthAlertMessage(-streak_value),
                           smtp_server=constants.GOLO_SMTP_SERVER,
                           extra_fields={'X-cbuildbot-alert': 'cq-health'})

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
    counter_url = os.path.join(constants.MANIFEST_VERSIONS_GS_URL,
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

  def _UploadMetadataForRun(self, final_status):
    """Upload metadata.json for this entire run.

    Args:
      final_status: Final status string for this run.
    """
    self._run.attrs.metadata.UpdateWithDict(
        self.GetReportMetadata(final_status=final_status,
                               sync_instance=self._sync_instance,
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
                        sync_instance=None, completion_instance=None):
    """Generate ReportStage metadata.

   Args:
      config: The build config for this run.  Defaults to self._run.config.
      stage: The stage name that this metadata file is being uploaded for.
      final_status: Whether the build passed or failed. If None, the build
        will be treated as still running.
      sync_instance: The stage instance that was used for syncing the source
        code. This should be a derivative of SyncStage. If None, the list of
        commit queue patches will not be included in the metadata.
      completion_instance: The stage instance that was used to wait for slave
        completion. Used to add slave build information to master builder's
        metadata. If None, no such status information will be included. It not
        None, this should be a derivative of MasterSlaveSyncCompletionStage.

    Returns:
      A JSON-able dictionary representation of the metadata object.
    """
    builder_run = self._run
    config = config or builder_run.config

    commit_queue_stages = (sync_stages.CommitQueueSyncStage,
                           sync_stages.PreCQSyncStage)
    get_changes_from_pool = (sync_instance and
                             isinstance(sync_instance, commit_queue_stages) and
                             sync_instance.pool)

    get_statuses_from_slaves = (
        config['master'] and
        completion_instance and
        isinstance(completion_instance,
                   completion_stages.MasterSlaveSyncCompletionStage)
    )

    return cbuildbot_metadata.CBuildbotMetadata.GetReportMetadataDict(
        builder_run, get_changes_from_pool,
        get_statuses_from_slaves, config, stage, final_status, sync_instance,
        completion_instance)

  def PerformStage(self):
    # Make sure local archive directory is prepared, if it was not already.
    # TODO(mtennant): It is not clear how this happens, but a CQ master run
    # that never sees an open tree somehow reaches Report stage without a
    # set up archive directory.
    if not os.path.exists(self.archive_path):
      self.archive.SetupArchivePath()

    if results_lib.Results.BuildSucceededSoFar():
      final_status = constants.FINAL_STATUS_PASSED
    else:
      final_status = constants.FINAL_STATUS_FAILED

    # Upload metadata, and update the pass/fail streak counter for the main
    # run only. These aren't needed for the child builder runs.
    self._UploadMetadataForRun(final_status)
    self._UpdateRunStreak(self._run, final_status)

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
        # Also update the LATEST files, since this run did archive something.

        # Check if the builder_run is tied to any boards and if so get all
        # upload urls.
        if builder_run.config['boards']:
          upload_urls = self._GetUploadUrls(
              'LATEST-*', board=builder_run.config['boards'][0])
        else:
          upload_urls = [self.upload_url]
        archive = builder_run.GetArchive()
        archive.UpdateLatestMarkers(builder_run.manifest_branch,
                                    builder_run.debug,
                                    upload_urls=upload_urls)

    version = getattr(self._run.attrs, 'release_tag', '')
    results_lib.Results.Report(sys.stdout, archive_urls=archive_urls,
                               current_version=version)


class RefreshPackageStatusStage(generic_stages.BuilderStage):
  """Stage for refreshing Portage package status in online spreadsheet."""
  def PerformStage(self):
    commands.RefreshPackageStatus(buildroot=self._build_root,
                                  boards=self._boards,
                                  debug=self._run.options.debug)
