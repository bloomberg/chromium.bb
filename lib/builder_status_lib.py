# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to manage builder statuses."""

from __future__ import print_function

import collections
import cPickle
import os

from chromite.lib import buildbucket_lib
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import gs


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
      message: A failures_lib.BuildFailureMessage object with details
               of builder failure. Or, None.
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
  """Operations to manage slave BuilderStatus."""

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
