# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing class for recording metadata about a run."""

import datetime
import json
import logging
import math
import os
import time

from chromite.buildbot import cbuildbot_archive
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import toolchain

class CBuildbotMetadata(object):
  """Class for recording metadata about a run."""

  def __init__(self, metadata_dict=None, multiprocess_manager=None):
    """Constructor for CBuildbotMetadata.

    Args:
      metadata_dict: Optional dictionary containing initial metadata,
                     as returned by loading metadata from json.
      multiprocess_manager: Optional multiprocess.Manager instance. If
                            supplied, the metadata instance will use
                            multiprocess containers so that its state
                            is correctly synced across processes.
    """
    super(CBuildbotMetadata, self).__init__()
    if multiprocess_manager:
      self._metadata_dict = multiprocess_manager.dict()
      self._cl_action_list = multiprocess_manager.list()
    else:
      self._metadata_dict = {}
      self._cl_action_list = []

    if metadata_dict:
      self.UpdateWithDict(metadata_dict)

  def UpdateWithDict(self, metadata_dict):
    """Update metadata dictionary with values supplied in |metadata_dict|"""
    # This is effectively the inverse of the dictionar construction in GetDict,
    # to reconstruct the correct internal representation of a metadata
    # object.
    metadata_dict = metadata_dict.copy()
    cl_action_list = metadata_dict.pop('cl_actions', None)
    self._metadata_dict.update(metadata_dict)
    if cl_action_list:
      self._cl_action_list.extend(cl_action_list)

  def GetDict(self):
    """Returns a dictionary representation of metadata."""
    # CL actions are be stored in self._cl_action_list instead of
    # in self._metadata_dict['cl_actions'], because _cl_action_list
    # is potentially a multiprocess.lis. So, _cl_action_list needs to
    # be copied into a normal list.
    temp = self._metadata_dict.copy()
    temp['cl_actions'] = list(self._cl_action_list)
    return temp

  def GetJSON(self):
    """Return a JSON string representation of metadata."""
    return json.dumps(self.GetDict())

  def RecordCLAction(self, change, action, timestamp=None, reason=None):
    """Record an action that was taken on a CL, to the metadata.

    Args:
      change: A GerritPatch object for the change acted on.
      action: The action taken, should be one of constants.CL_ACTIONS
      timestamp: An integer timestamp such as int(time.time()) at which
                 the action was taken. Default: Now.
      reason: Description of the reason the action was taken. Default: ''
    """
    cl_action = (self._ChangeAsSmallDictionary(change),
                 action,
                 timestamp or int(time.time()),
                 reason or '')

    self._cl_action_list.append(cl_action)

  @staticmethod
  def _ChangeAsSmallDictionary(change):
    """Returns a small dictionary representation of a gerrit change.

    Args:
      change: A GerritPatch object.

    Returns:
      A dictionary of the form {'gerrit_number': change.gerrit_number,
                                'patch_number': change.patch_number,
                                 'internal': change.internal}
    """
    return  {'gerrit_number': change.gerrit_number,
             'patch_number': change.patch_number,
             'internal': change.internal}

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


def FindLatestFullVersion(builder, version):
  """Find the latest full version number built by |builder| on |version|.

  Args:
    builder: Builder to load information from. E.g. daisy-release
    version: Version that we are interested in. E.g. 5602.0.0

  Returns:
    The latest corresponding full version number, including milestone prefix.
    E.g. R35-5602.0.0. For some builders, this may also include a -rcN or
    -bNNNN suffix.
  """
  gs_ctx = gs.GSContext()
  config = cbuildbot_config.config[builder]
  base_url = cbuildbot_archive.GetBaseUploadURI(config)
  latest_file_url = os.path.join(base_url, 'LATEST-%s' % version)
  try:
    return gs_ctx.Cat(latest_file_url).output.strip()
  except gs.GSNoSuchKey:
    return None


def GetBuildMetadata(builder, full_version):
  """Fetch the metadata.json object for |builder| and |full_version|.

  Args:
    builder: Builder to load information from. E.g. daisy-release
    full_version: Version that we are interested in, including milestone
        prefix. E.g. R35-5602.0.0. For some builders, this may also include a
        -rcN or -bNNNN suffix.

  Returns:
    A newly created CBuildbotMetadata object with the metadata from the given
    |builder| and |full_version|.
  """
  gs_ctx = gs.GSContext()
  config = cbuildbot_config.config[builder]
  base_url = cbuildbot_archive.GetBaseUploadURI(config)
  try:
    archive_url = os.path.join(base_url, full_version)
    metadata_url = os.path.join(archive_url, constants.METADATA_JSON)
    output = gs_ctx.Cat(metadata_url).output
    return CBuildbotMetadata(json.loads(output))
  except gs.GSNoSuchKey:
    return None
