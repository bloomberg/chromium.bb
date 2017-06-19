# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to manage builder statuses."""

from __future__ import print_function

import collections
import cPickle
import os

from chromite.lib import buildbucket_lib
from chromite.lib import build_failure_message
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import failure_message_lib
from chromite.lib import gs
from chromite.lib import tree_status


site_config = config_lib.GetConfig()

BUILD_STATUS_URL = (
    '%s/builder-status' % site_config.params.MANIFEST_VERSIONS_GS_URL)
NUM_RETRIES = 20

# Namedtupe to store CIDB status info.
CIDBStatusInfo = collections.namedtuple(
    'CIDBStatusInfo',
    ['build_id', 'status', 'build_number'])


class BuilderStatus(object):
  """Object representing the status of a build."""

  def __init__(self, status, message, dashboard_url=None):
    """Constructor for BuilderStatus.

    Args:
      status: Status string (should be one of BUILDER_STATUS_FAILED,
              BUILDER_STATUS_PASSED, BUILDER_STATUS_INFLIGHT, or
              BUILDER_STATUS_MISSING).
      message: A BuildFailureMessage object with details of builder failure.
               Or, None.
      dashboard_url: Optional url linking to builder dashboard for this build.
    """
    self.status = status
    self.message = message
    self.dashboard_url = dashboard_url

  # Helper methods to make checking the status object easy.

  def Failed(self):
    """Returns True if the Builder failed."""
    return self.status == constants.BUILDER_STATUS_FAILED

  def Passed(self):
    """Returns True if the Builder passed."""
    return self.status == constants.BUILDER_STATUS_PASSED

  def Inflight(self):
    """Returns True if the Builder is still inflight."""
    return self.status == constants.BUILDER_STATUS_INFLIGHT

  def Missing(self):
    """Returns True if the Builder is missing any status."""
    return self.status == constants.BUILDER_STATUS_MISSING

  def Completed(self):
    """Returns True if the Builder has completed."""
    return self.status in constants.BUILDER_COMPLETED_STATUSES

  @classmethod
  def GetCompletedStatus(cls, success):
    """Return the appropriate status constant for a completed build.

    Args:
      success: Whether the build was successful or not.
    """
    if success:
      return constants.BUILDER_STATUS_PASSED
    else:
      return constants.BUILDER_STATUS_FAILED

  def AsFlatDict(self):
    """Returns a flat json-able representation of this builder status.

    Returns:
      A dictionary of the form {'status' : status, 'message' : message,
      'dashboard_url' : dashboard_url} where all values are guaranteed
      to be strings. If dashboard_url is None, the key will be excluded.
    """
    flat_dict = {'status' : str(self.status),
                 'message' : str(self.message),
                 'reason' : str(None if self.message is None
                                else self.message.reason)}
    if self.dashboard_url is not None:
      flat_dict['dashboard_url'] = str(self.dashboard_url)
    return flat_dict

  def AsPickledDict(self):
    """Returns a pickled dictionary representation of this builder status."""
    return cPickle.dumps(dict(status=self.status, message=self.message,
                              dashboard_url=self.dashboard_url))


class BuilderStatusManager(object):
  """Operations to manage BuilderStatus."""

  @staticmethod
  def GetStatusUrl(builder, version):
    """Get the status URL in Google Storage for a given builder / version."""
    return os.path.join(BUILD_STATUS_URL, version, builder)

  @staticmethod
  def _UnpickleBuildStatus(pickle_string):
    """Returns a builder_status_lib.BuilderStatus obj from a pickled string."""
    try:
      status_dict = cPickle.loads(pickle_string)
    except (cPickle.UnpicklingError, AttributeError, EOFError,
            ImportError, IndexError, TypeError) as e:
      # The above exceptions are listed as possible unpickling exceptions
      # by http://docs.python.org/2/library/pickle
      # In addition to the exceptions listed in the doc, we've also observed
      # TypeError in the wild.
      logging.warning('Failed with %r to unpickle status file.', e)
      return BuilderStatus(
          constants.BUILDER_STATUS_FAILED, message=None)

    return BuilderStatus(**status_dict)

  @staticmethod
  def GetBuilderStatus(builder, version, retries=NUM_RETRIES):
    """Returns a builder_status_lib.BuilderStatus obj for the given the builder.

    Args:
      builder: Builder to look at.
      version: Version string.
      retries: Number of retries for getting the status.

    Returns:
      A builder_status_lib.BuilderStatus instance containing the builder status
      and any optional message associated with the status passed by the builder.
      If no status is found for this builder then the returned
      builder_status_lib.BuilderStatus object will have status STATUS_MISSING.
    """
    url = BuilderStatusManager.GetStatusUrl(builder, version)
    ctx = gs.GSContext(retries=retries)
    try:
      output = ctx.Cat(url)
    except gs.GSNoSuchKey:
      return BuilderStatus(
          constants.BUILDER_STATUS_MISSING, None)

    return BuilderStatusManager._UnpickleBuildStatus(output)

class SlaveBuilderStatus(object):
  """Operations to manage slave BuilderStatus.

  This class fetches slave statuses and slave failures from Buildbucket and
  CIDB, generates BuilderStatus instances for slave builds. This class only
  fetches BuilderStatus information for important slaves.
  """

  def __init__(self, master_build_id, db, config, metadata, buildbucket_client,
               builders_array, dry_run):
    """Create an instance of SlaveBuilderStatus for a given master build.

    Args:
      master_build_id: The build_id of the master build.
      db: An instance of cidb.CIDBConnection to fetch data from CIDB.
      config: Instance of config_lib.BuildConfig. Config dict of this build.
      metadata: Instance of metadata_lib.CBuildbotMetadata. Metadata of this
                build.
      buildbucket_client: Instance of buildbucket_lib.buildbucket_client.
      builders_array: List of the expected and important slave builds.
      dry_run: Boolean indicating whether it's a dry run. Default to True.
    """
    self.master_build_id = master_build_id
    self.db = db
    self.config = config
    self.metadata = metadata
    self.buildbucket_client = buildbucket_client
    self.builders_array = builders_array
    self.dry_run = dry_run

    self.buildbucket_info_dict = None
    self.cidb_info_dict = None
    self.slave_failures_dict = None
    self._InitSlaveInfo()

  def _GetSlaveFailures(self, buildbucket_info_dict):
    """Get a dict mapping slave builds to their build failures.

    Args:
      buildbucket_info_dict: A dict mapping slave build config names
        (strings) to their BuildbucketInfos.

    Returns:
      A dict mapping the slave build config names (strings) to stage failure
      messages (See return type of
      FailureMessageManager.ConstructStageFailureMessages)
    """
    slave_failures_dict = {}

    slave_buildbucket_ids = (
        None if buildbucket_info_dict is None else
        [bb_info.buildbucket_id for bb_info in buildbucket_info_dict.values()])

    stage_failures = self.db.GetSlaveFailures(
        self.master_build_id, buildbucket_ids=slave_buildbucket_ids)
    stage_failures_by_build = cros_build_lib.GroupNamedtuplesByKey(
        stage_failures, 'build_config')

    failure_msg_manager = failure_message_lib.FailureMessageManager()
    for build_config, stage_failures in stage_failures_by_build.items():
      slave_failures_dict[build_config] = (
          failure_msg_manager.ConstructStageFailureMessages(stage_failures))

    return slave_failures_dict

  def _InitSlaveInfo(self):
    """Init slave info including buildbucket info, cidb info and failures."""
    if config_lib.UseBuildbucketScheduler(self.config):
      scheduled_buildbucket_info_dict = buildbucket_lib.GetBuildInfoDict(
          self.metadata)
      self.buildbucket_info_dict = self.GetAllSlaveBuildbucketInfo(
          self.buildbucket_client, scheduled_buildbucket_info_dict,
          dry_run=self.dry_run)
      self.builders_array = self.buildbucket_info_dict.keys()

    self.cidb_info_dict = self.GetAllSlaveCIDBStatusInfo(
        self.db, self.master_build_id, self.buildbucket_info_dict)

    self.slave_failures_dict = self._GetSlaveFailures(
        self.buildbucket_info_dict)

  def _GetStatus(self, build_config, cidb_info_dict, buildbucket_info_dict):
    """Get status of a given build.

    Args:
      build_config: Build config name (string) of a slave build.
      cidb_info_dict: A dict mapping slave build config names (strings) to their
        cidb infos (in the format of CIDBStatusInfo).
      buildbucket_info_dict: A dict mapping slave build config names (strings)
        to their Buildbucket infos (in the format of BuildbucketInfo).

    Returns:
      Builder status of the given build.
    """
    cidb_info = cidb_info_dict.get(build_config)
    if cidb_info is None:
      return constants.BUILDER_STATUS_MISSING
    elif (cidb_info.status in (constants.BUILDER_STATUS_PASSED,
                               constants.BUILDER_STATUS_FAILED)):
      return cidb_info.status
    else:
      if buildbucket_info_dict is not None:
        if (buildbucket_info_dict[build_config].status ==
            constants.BUILDBUCKET_BUILDER_STATUS_STARTED):
          return constants.BUILDER_STATUS_INFLIGHT
        else:
          return constants.BUILDER_STATUS_FAILED
      else:
        return constants.BUILDER_STATUS_INFLIGHT

  # TODO(nxia): Buildbucket response returns luci-milo instead buildbot urls.
  def _GetDashboardUrl(self, build_config, cidb_info_dict,
                       buildbucket_info_dict):
    """Get dashboard url of a given build.

    Args:
      build_config: Build config name (string) of a slave build.
      cidb_info_dict: A dict mapping slave build config names (strings) to their
        cidb infos (in the format of CIDBStatusInfo).
      buildbucket_info_dict: A dict mapping slave build config names (strings)
        to their Buildbucket infos (in the format of BuildbucketInfo).

    Returns:
      Dashboard url of the given build. None if no entry found for this given
      build in CIDB and buildbucket_info_dict is None.
    """
    if build_config in cidb_info_dict:
      build_number = cidb_info_dict[build_config].build_number

      return tree_status.ConstructDashboardURL(
          site_config[build_config].active_waterfall,
          build_config,
          build_number)
    elif buildbucket_info_dict is not None:
      # If no entry found in CIDB, get the buildbot url from Buildbucket.
      return buildbucket_info_dict[build_config].url

  @staticmethod
  def CreateBuildFailureMessage(build_config, overlays, dashboard_url,
                                failure_messages):
    """Creates a message summarizing the failures.

    Args:
      build_config: Build config name (string) of a slave build.
      overlays: The overlays used for the build.
      dashboard_url: The URL of the build.
      failure_messages: A list of stage failure messages (instances of
        StageFailureMessage or its sub-classes) of the given build.

    Returns:
      A build_failure_message.BuildFailureMessage object.
    """
    internal = overlays in [constants.PRIVATE_OVERLAYS,
                            constants.BOTH_OVERLAYS]
    details = []

    if failure_messages:
      for x in failure_messages:
        details.append('The %s stage failed: %s' % (
            x.stage_name, x.exception_message))

    if not details:
      details = ['cbuildbot failed']

    # reason does not include builder name or URL. This is mainly for
    # populating the "failure message" column in the stats sheet.
    reason = ' '.join(details)
    details.append('in %s' % dashboard_url)
    msg_summary = '%s: %s' % (build_config, ' '.join(details))

    return build_failure_message.BuildFailureMessage(
        msg_summary, failure_messages, internal, reason, build_config)

  def _GetMessage(self, build_config, status, dashboard_url,
                  slave_failures_dict):
    """Get BuildFailureMessage of a given build.

    Args:
      build_config: Build config name (string) of a slave build.
      status: The status of the build (See return type of self._GetStatus())
      dashboard_url: The URL of the build.
      slave_failures_dict: A dict mapping the slave build config names (strings)
        to stage failure messages (See return type of _GetSlaveFailures)

    Returns:
      A build_failure_message.BuildFailureMessage object if the status is
      constants.BUILDER_STATUS_FAILED; else, None.
    """
    if status == constants.BUILDER_STATUS_FAILED:
      failure_messages = slave_failures_dict.get(build_config)
      overlays = site_config[build_config].overlays

      return self.CreateBuildFailureMessage(
          build_config, overlays, dashboard_url, failure_messages)

  def GetBuilderStatusForBuild(self, build_config):
    """Get BuilderStatus for a given build.

    Args:
      build_config: Build config name (string) of a slave build.

    Returns:
      An instance of BuilderStatus of the given build.
    """
    status = self._GetStatus(
        build_config, self.cidb_info_dict, self.buildbucket_info_dict)
    dashboard_url = self._GetDashboardUrl(
        build_config, self.cidb_info_dict, self.buildbucket_info_dict)
    message = self._GetMessage(
        build_config, status, dashboard_url, self.slave_failures_dict)

    return BuilderStatus(status, message, dashboard_url=dashboard_url)

  @staticmethod
  def GetAllSlaveBuildbucketInfo(buildbucket_client,
                                 scheduled_buildbucket_info_dict,
                                 dry_run=True):
    """Get buildbucket info from Buildbucket for all scheduled slave builds.

    For each build in the scheduled builds dict, get build status and build
    result from Buildbucket and return a updated buildbucket_info_dict.

    Args:
      buildbucket_client: Instance of buildbucket_lib.buildbucket_client.
      scheduled_buildbucket_info_dict: A dict mapping scheduled slave build
        config name to its buildbucket information in the format of
        BuildbucketInfo (see buildbucket.GetBuildInfoDict for details).
      dry_run: Boolean indicating whether it's a dry run. Default to True.

    Returns:
      A dict mapping all scheduled slave build config names to their
      BuildbucketInfos (The BuildbucketInfo of the most recently retried one of
      there're multiple retries for a slave build config).
    """
    assert buildbucket_client is not None, 'buildbucket_client is None'

    all_buildbucket_info_dict = {}
    for build_config, build_info in scheduled_buildbucket_info_dict.iteritems():
      buildbucket_id = build_info.buildbucket_id
      retry = build_info.retry
      created_ts = build_info.created_ts
      status = None
      result = None
      url = None

      try:
        content = buildbucket_client.GetBuildRequest(buildbucket_id, dry_run)
        status = buildbucket_lib.GetBuildStatus(content)
        result = buildbucket_lib.GetBuildResult(content)
        url = buildbucket_lib.GetBuildURL(content)
      except buildbucket_lib.BuildbucketResponseException as e:
        # If we have a temporary issue accessing the build status from the
        # Buildbucket, log the error and continue with other builds.
        # SlaveStatus will handle the missing builds in ShouldWait().
        logging.error('Failed to get status for build %s id %s: %s',
                      build_config, buildbucket_id, e)

      all_buildbucket_info_dict[build_config] = buildbucket_lib.BuildbucketInfo(
          buildbucket_id, retry, created_ts, status, result, url)

    return all_buildbucket_info_dict

  @staticmethod
  def GetAllSlaveCIDBStatusInfo(db, master_build_id,
                                all_buildbucket_info_dict):
    """Get build status information from CIDB for all slaves.

    Args:
      db: An instance of cidb.CIDBConnection.
      master_build_id: The build_id of the master build for slaves.
      all_buildbucket_info_dict: A dict mapping all build config names to their
        information fetched from Buildbucket server (in the format of
        BuildbucketInfo).

    Returns:
      A dict mapping build config names to their cidb infos (in the format of
      CIDBStatusInfo). If all_buildbucket_info_dict is not None, the returned
      map only contains slave builds which are associated with buildbucket_ids
      recorded in all_buildbucket_info_dict.
    """
    all_cidb_status_dict = {}
    if db is not None:
      buildbucket_ids = None if all_buildbucket_info_dict is None else [
          info.buildbucket_id for info in all_buildbucket_info_dict.values()]

      slave_statuses = db.GetSlaveStatuses(
          master_build_id, buildbucket_ids=buildbucket_ids)

      all_cidb_status_dict = {s['build_config']: CIDBStatusInfo(
          s['id'], s['status'], s['build_number']) for s in slave_statuses}

    return all_cidb_status_dict
