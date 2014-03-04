# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing class for recording metadata about a run."""

import datetime
import json
import logging
import math
import os

from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib
from chromite.lib import toolchain

class CBuildbotMetadata(object):
  """Class for recording metadata about a run."""
  @staticmethod
  def GetMetadataDict(builder_run, build_root, get_changes_from_pool,
                      get_statuses_from_slaves, config=None, stage=None,
                      final_status=None, sync_instance=None,
                      completion_instance=None):
    """Return a metadata dictionary summarizing a build.

    This method replaces code that used to exist in the ArchivingStageMixin
    class from cbuildbot_stage. It contains all the Archive-stage-time
    metadata construction logic. The logic here is intended to be gradually
    refactored out so that the metadata is constructed gradually by the
    stages that are responsible for pieces of data, as they run.

    Args:
      builder_run: BuilderRun instance for this run.
      build_root: Path to build root for this run.
      get_changes_from_pool: If True, information about patches in the
                             sync_instance.pool will be recorded.
      get_statuses_from_slaves: If True, status information of slave
                                builders will be recorded.
      config: The build config for this run.  Defaults to self._run.config.
      stage: The stage name that this metadata file is being uploaded for.
      final_status: Whether the build passed or failed. If None, the build
                    will be treated as still running.
      sync_instance: The stage instance that was used for syncing the source
                     code. This should be a derivative of SyncStage. If None,
                     the list of commit queue patches will not be included
                     in the metadata.
      completion_instance: The stage instance that was used to wait for slave
                           completion. Used to add slave build information to
                           master builder's metadata. If None, no such status
                           information will be included. It not None, this
                           should be a derivative of
                           MasterSlaveSyncCompletionStage.

    Returns:
       A metadata dictionary suitable to be json-serialized.
    """
    config = config or builder_run.config

    start_time = results_lib.Results.start_time
    current_time = datetime.datetime.now()
    start_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=start_time)
    current_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=current_time)
    duration = '%s' % (current_time - start_time,)

    sdk_verinfo = cros_build_lib.LoadKeyValueFile(
        os.path.join(build_root, constants.SDK_VERSION_FILE),
        ignore_missing=True)
    verinfo = builder_run.GetVersionInfo(build_root)
    platform_tag = getattr(builder_run.attrs, 'release_tag')
    if not platform_tag:
      platform_tag = verinfo.VersionString()
    metadata = {
        # Version of the metadata format.
        'metadata-version': '2',
        # Data for this build.
        'bot-config': config['name'],
        'bot-hostname': cros_build_lib.GetHostName(fully_qualified=True),
        'boards': config['boards'],
        'build-number': builder_run.buildnumber,
        'builder-name': os.environ.get('BUILDBOT_BUILDERNAME', ''),
        'status': {
            'current-time': current_time_stamp,
            'status': final_status if final_status else 'running',
            'summary': stage or '',
        },
        'time': {
            'start': start_time_stamp,
            'finish': current_time_stamp if final_status else '',
            'duration': duration,
        },
        'version': {
            'chrome': builder_run.attrs.chrome_version,
            'full': builder_run.GetVersion(),
            'milestone': verinfo.chrome_branch,
            'platform': platform_tag,
        },
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

    metadata['results'] = []
    for entry in results_lib.Results.Get():
      timestr = datetime.timedelta(seconds=math.ceil(entry.time))
      if entry.result in results_lib.Results.NON_FAILURE_TYPES:
        status = constants.FINAL_STATUS_PASSED
      else:
        status = constants.FINAL_STATUS_FAILED
      metadata['results'].append({
          'name': entry.name,
          'status': status,
          # The result might be a custom exception.
          'summary': str(entry.result),
          'duration': '%s' % timestr,
          'description': entry.description,
          'log': builder_run.ConstructDashboardURL(stage=entry.name),
      })

    if get_changes_from_pool:
      changes = []
      pool = sync_instance.pool
      for change in pool.changes:
        details = {'gerrit_number': change.gerrit_number,
                   'patch_number': change.patch_number,
                   'internal': change.internal}
        for latest_patchset_only in (False, True):
          prefix = '' if latest_patchset_only else 'total_'
          for status in (pool.STATUS_FAILED, pool.STATUS_PASSED):
            count = pool.GetCLStatusCount(pool.bot, change, status,
                                          latest_patchset_only)
            details['%s%s' % (prefix, status.lower())] = count
        changes.append(details)
      metadata['changes'] = changes

    # If we were a CQ master, then include a summary of the status of slave cq
    # builders in metadata
    if get_statuses_from_slaves:
      statuses = completion_instance.GetSlaveStatuses()
      if not statuses:
        logging.warning('completion_instance did not have any statuses '
                        'to report. Will not add slave status to metadata.')

      metadata['slave_targets'] = {}
      for builder, status in statuses.iteritems():
        metadata['slave_targets'][builder] = status.AsFlatDict()

    return metadata