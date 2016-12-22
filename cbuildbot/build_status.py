# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for tracking and querying build status."""

from __future__ import print_function

import cPickle
import collections
import datetime

from chromite.cbuildbot import buildbucket_lib
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import metrics


# Namedtupe to store buildbucket related info.
BuildbucketInfo = collections.namedtuple(
    'BuildbucketInfo',
    ['buildbucket_id', 'retry', 'status', 'result'])

# Namedtupe to store CIDB status info.
CIDBStatusInfo = collections.namedtuple(
    'CIDBStatusInfo',
    ['build_id', 'status'])


class SlaveStatus(object):
  """Keep track of statuses of all slaves from CIDB and Buildbucket(optional).

  For the master build scheduling slave builds through Buildbucket, it will
  interpret slave statuses by querying CIDB and Buildbucket; otherwise,
  it will only interpret slave statuses by querying CIDB.
  """

  BUILD_START_TIMEOUT_MIN = 5

  ACCEPTED_STATUSES = (constants.BUILDER_STATUS_PASSED,
                       constants.BUILDER_STATUS_SKIPPED,)

  def __init__(self, start_time, builders_array, master_build_id, db,
               config=None, metadata=None, buildbucket_client=None,
               dry_run=True):
    """Initializes a SlaveStatus instance.

    Args:
      start_time: datetime.datetime object of when the build started.
      builders_array: List of the expected slave builds.
      master_build_id: The build_id of the master build.
      db: An instance of cidb.CIDBConnection to fetch data from CIDB.
      config: Instance of config_lib.BuildConfig. Config dict of this build.
      metadata: Instance of metadata_lib.CBuildbotMetadata. Metadata of this
                build.
      buildbucket_client: Instance of buildbucket_lib.buildbucket_client.
      dry_run: Boolean indicating whether it's a dry run. Default to True.
    """
    self.start_time = start_time
    self.builders_array = builders_array
    self.master_build_id = master_build_id
    self.db = db
    self.config = config
    self.metadata = metadata
    self.buildbucket_client = buildbucket_client
    self.dry_run = dry_run

    # A set of completed builds which will not be retried any more.
    self.completed_builds = set()
    self.cidb_status = None
    self.missing_builds = None
    self.scheduled_builds = None
    self.builds_to_retry = None
    self.buildbucket_info_dict = None
    self.status_buildset_dict = None

    self.UpdateSlaveStatus()

  def _GetSlaveStatusesFromCIDB(self, buildbucket_info_dict):
    """Get statuses of slaves (not in completed_builds set) from CIDB.

    Args:
      buildbucket_info_dict: A dict mapping build config names to their
        information fetched from Buildbucket server (in format of
        BuildbucketInfo).

    Returns:
      A dict mapping the slave build config name to its CIDBStatusInfo
      (status information fetched from CIDB).
      The dict only contains builds not in completed_builds.
    """
    cidb_status = {}
    if self.db is not None:
      buildbucket_ids = None if buildbucket_info_dict is None else [
          info.buildbucket_id for info in buildbucket_info_dict.values()]

      status_list = self.db.GetSlaveStatuses(
          self.master_build_id, buildbucket_ids=buildbucket_ids)

      for d in status_list:
        if d['build_config'] not in self.completed_builds:
          cidb_status[d['build_config']] = CIDBStatusInfo(d['id'], d['status'])
    return cidb_status

  def _GetSlaveStatusesFromBuildbucket(self):
    """Get statues of slaves (not in completed_builds set) from Buildbucket.

    For slave builds scheduled by Buildbucket, query the build information
    ('status' and 'result') from Buildbucket.

    Returns:
       A dict mapping slave build config name to its BuildbucketInfo.
       The dict only contains builds not in completed_builds.
    """
    assert self.buildbucket_client is not None, 'buildbucket_client is None'

    buildbucket_info_dict = buildbucket_lib.GetBuildInfoDict(self.metadata)
    updated_buildbucket_info_dict = {}

    for build_config in (set(buildbucket_info_dict.keys()) -
                         self.completed_builds):
      buildbucket_id = buildbucket_info_dict[build_config]['buildbucket_id']
      retry = buildbucket_info_dict[build_config]['retry']
      status = None
      result = None

      try:
        content = self.buildbucket_client.GetBuildRequest(
            buildbucket_id, self.dry_run)
        status = buildbucket_lib.GetBuildStatus(content)
        result = buildbucket_lib.GetBuildResult(content)
      except buildbucket_lib.BuildbucketResponseException as e:
        # If we have a temporary issue accessing the build status from the
        # Buildbucket, log the error and continue with other builds.
        # SlaveStatus will handle the missing builds in ShouldWait().
        logging.error('Failed to get status for build %s id %s: %s',
                      build_config, buildbucket_id, e)

      updated_buildbucket_info_dict[build_config] = BuildbucketInfo(
          buildbucket_id, retry, status, result)

    return updated_buildbucket_info_dict

  def _SetStatusBuildsDict(self):
    """Set status_buildset_dict by sorting the builds into their status set."""
    self.status_buildset_dict = {}
    for build, info in self.buildbucket_info_dict.iteritems():
      if info.status is not None:
        self.status_buildset_dict.setdefault(info.status, set())
        self.status_buildset_dict[info.status].add(build)

  def UpdateSlaveStatus(self):
    """Update slave statuses by querying CIDB and Buildbucket(if supported)."""
    if (self.config is not None and
        self.metadata is not None and
        config_lib.UseBuildbucketScheduler(self.config)):
      self.buildbucket_info_dict = self._GetSlaveStatusesFromBuildbucket()
      self._SetStatusBuildsDict()

    self.cidb_status = self._GetSlaveStatusesFromCIDB(
        self.buildbucket_info_dict)

    self.missing_builds = self._GetMissingBuilds()
    self.scheduled_builds = self._GetScheduledBuilds()
    self.builds_to_retry = self._GetBuildsToRetry()
    self.completed_builds = self._GetCompletedBuilds()

  def GetBuildbucketBuilds(self, build_status):
    """Get the buildbucket builds which are in the build_status status.

    Args:
      build_status: The status of the builds to get. The status must
                    be a member of constants.BUILDBUCKET_BUILDER_STATUSES.

    Returns:
      A set of builds in build_status status.
    """
    if build_status not in constants.BUILDBUCKET_BUILDER_STATUSES:
      raise ValueError(
          '%s is not a member of %s '
          % (build_status, constants.BUILDBUCKET_BUILDER_STATUSES))

    return self.status_buildset_dict.get(build_status, set())

  def _GetMissingBuilds(self):
    """Returns the missing builds.

    For builds scheduled by Buildbucket, missing refers to builds without
    'status' from Buildbucket.
    For builds not scheduled by Buildbucket, missing refers builds without
    reporting status to CIDB.

    Returns:
      A set of the config names of missing builds.
    """
    if self.buildbucket_info_dict is not None:
      return set(build for build, info in self.buildbucket_info_dict.iteritems()
                 if info.status is None)
    else:
      return (set(self.builders_array) - set(self.cidb_status.keys()) -
              self.completed_builds)

  def _GetScheduledBuilds(self):
    """Returns the scheduled builds.

    Returns:
      For builds scheduled by Buildbucket, a set of config names of builds
      with 'SCHEDULED' status in Buildbucket;
      For other builds, None.
    """
    if self.buildbucket_info_dict is not None:
      return self.GetBuildbucketBuilds(
          constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED)
    else:
      return None

  def _GetRetriableBuilds(self, completed_builds):
    """Get retriable builds from completed builds.

    Args:
      completed_builds: a set of builds with 'COMPLETED' status in Buildbucket.

    Returns:
      A set of config names of retriable builds.
    """
    builds_to_retry = set()

    for build in completed_builds:
      build_result = self.buildbucket_info_dict[build].result
      if build_result == constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS:
        logging.info('Not retriable build %s completed with result %s.',
                     build, build_result)
        continue

      build_retry = self.buildbucket_info_dict[build].retry
      if build_retry >= constants.BUILDBUCKET_BUILD_RETRY_LIMIT:
        logging.info('Not retriable build %s reached the build retry limit %d.',
                     build, constants.BUILDBUCKET_BUILD_RETRY_LIMIT)
        continue

      # If build is in self.status, it means a build tuple has been
      # inserted into CIDB buildTable.
      if build in self.cidb_status:
        if not config_lib.RetryAlreadyStartedSlaves(self.config):
          logging.info('Not retriable build %s started already.', build)
          continue

        assert self.db is not None

        build_stages = self.db.GetBuildStages(self.cidb_status[build].build_id)
        accepted_stages = {stage['name'] for stage in build_stages
                           if stage['status'] in self.ACCEPTED_STATUSES}

        # A failed build is not retriable if it passed the critical stage.
        if config_lib.GetCriticalStageForRetry(self.config) in accepted_stages:
          continue

      builds_to_retry.add(build)

    return builds_to_retry

  def _GetBuildsToRetry(self):
    """Get the config names of the builds to retry.

    Returns:
      A set config names of builds to be retried.
    """
    if self.buildbucket_info_dict is not None:
      return self._GetRetriableBuilds(
          self.GetBuildbucketBuilds(
              constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED))
    else:
      return None

  def _GetCompletedBuilds(self):
    """Returns the builds that have completed and will not be retried.

    Returns:
      A set of config names of completed and not retriable builds.
    """
    current_completed = None
    if self.buildbucket_info_dict is not None:
      assert self.builds_to_retry is not None

      current_completed_all = self.GetBuildbucketBuilds(
          constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED)

      current_completed = current_completed_all - self.builds_to_retry
    else:
      current_completed = set(
          b for b, s in self.cidb_status.iteritems()
          if s.status in constants.BUILDER_COMPLETED_STATUSES and
          b in self.builders_array)

    # Logging of the newly complete builders.
    for build in current_completed:
      status = (self.buildbucket_info_dict[build].result
                if self.buildbucket_info_dict is not None
                else self.cidb_status[build].status)
      logging.info('Build config %s completed with status "%s".',
                   build, status)

    completed_builds = self.completed_builds | current_completed

    return completed_builds

  def _Completed(self):
    """Returns a bool if all builds have completed successfully.

    Returns:
      A bool of True if all builds successfully completed, False otherwise.
    """
    return len(self.completed_builds) == len(self.builders_array)

  def _ShouldFailForBuilderStartTimeout(self, current_time):
    """Decides if we should fail if a build hasn't started within 5 mins.

    If a build hasn't started within BUILD_START_TIMEOUT_MIN and the rest of
    the builds have finished, let the caller know that we should fail.

    Args:
      current_time: A datetime.datetime object letting us know the current time.

    Returns:
      A bool saying True that we should fail, False otherwise.
    """
    # Check that we're at least past the start timeout.
    builder_start_deadline = datetime.timedelta(
        minutes=self.BUILD_START_TIMEOUT_MIN)
    past_deadline = current_time - self.start_time > builder_start_deadline

    # Check that we have missing builders and logging who they are.
    for builder in self.missing_builds:
      logging.error('No status found for build config %s.', builder)

    if self.buildbucket_info_dict is not None:
      # All scheduled builds added in buildbucket_info_dict are
      # either in completed status or still in scheduled status.
      other_builders_completed = (
          len(self.scheduled_builds) + len(self.completed_builds) ==
          len(self.builders_array))

      for builder in self.scheduled_builds:
        logging.error('Builder not started %s.', builder)

      return (past_deadline and other_builders_completed and
              self.scheduled_builds)
    else:
      # Check that aside from the missing builders the rest have completed.
      other_builders_completed = (
          len(self.missing_builds) + len(self.completed_builds) ==
          len(self.builders_array))

      return (past_deadline and other_builders_completed and
              self.missing_builds)

  def _RetryBuilds(self, builds):
    """Retry builds with Buildbucket.

    Args:
      builds: config names of the builds to retry with Buildbucket.

    Returns:
      A set of retried builds.
    """
    assert builds is not None

    new_scheduled_slaves = []
    for build in builds:
      try:
        buildbucket_id = self.buildbucket_info_dict[build].buildbucket_id
        build_retry = self.buildbucket_info_dict[build].retry

        logging.info('Going to retry build %s buildbucket_id %s '
                     'with retry # %d',
                     build, buildbucket_id, build_retry + 1)

        if not self.dry_run:
          fields = {'build_type': self.config.build_type,
                    'build_name': self.config.name}
          metrics.Counter(constants.MON_BB_RETRY_BUILD_COUNT).increment(
              fields=fields)

        content = self.buildbucket_client.RetryBuildRequest(
            buildbucket_id, dryrun=self.dry_run)

        new_buildbucket_id = buildbucket_lib.GetBuildId(content)
        new_created_ts = buildbucket_lib.GetBuildCreated_ts(content)
        new_scheduled_slaves.append((build, new_buildbucket_id, new_created_ts))

        logging.info('Retried build %s buildbucket_id %s created_ts %s',
                     build, new_buildbucket_id, new_created_ts)
      except buildbucket_lib.BuildbucketResponseException as e:
        logging.error('Failed to retry build %s buildbucket_id %s: %s',
                      build, buildbucket_id, e)

    if new_scheduled_slaves:
      self.metadata.ExtendKeyListWithList(
          constants.METADATA_SCHEDULED_SLAVES, new_scheduled_slaves)

    return set([build for build, _, _ in new_scheduled_slaves])

  def ShouldWait(self):
    """Decides if we should continue to wait for the builds to finish.

    This will be the retry function for timeout_util.WaitForSuccess, basically
    this function will return False if all builds finished or we see a problem
    with the builds. Otherwise it returns True to continue polling
    for the builds statuses. If the slave builds are scheduled by Buildbucket
    and there're builds to retry, call RetryBuilds on those builds.

    Returns:
      A bool of True if we should continue to wait and False if we should not.
    """
    # Check if all builders completed.
    if self._Completed():
      return False

    current_time = datetime.datetime.now()

    # Guess there are some builders building, check if there is a problem.
    if self._ShouldFailForBuilderStartTimeout(current_time):
      logging.error('Ending build since at least one builder has not started '
                    'within 5 mins.')
      return False

    # We got here which means no problems, we should still wait.
    logging.info('Still waiting for the following builds to complete: %r',
                 sorted(set(self.builders_array) - self.completed_builds))

    if self.builds_to_retry:
      retried_builds = self._RetryBuilds(self.builds_to_retry)
      self.builds_to_retry -= retried_builds

    return True


class BuilderStatus(object):
  """Object representing the status of a build."""

  def __init__(self, status, message, dashboard_url=None):
    """Constructor for BuilderStatus.

    Args:
      status: Status string (should be one of STATUS_FAILED, STATUS_PASSED,
              STATUS_INFLIGHT, or STATUS_MISSING).
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
