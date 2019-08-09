# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Database interface for all calls from Chromite.

BuildStore will be the interface which communicates between CIDB,
Buildbucket as the underlying databases and Chromite and other callers
as the clients of the data.
"""

from __future__ import print_function

import os

from chromite.lib import buildbucket_v2
from chromite.lib import cidb
from chromite.lib import constants
from chromite.lib import failure_message_lib
from chromite.lib import fake_cidb


class BuildStoreException(Exception):
  """General exception class for this module."""


class BuildIdentifier(object):
  """The class maintains all the IDs corresponding to a build."""

  def __init__(self, cidb_id=None, buildbucket_id=None):
    """Instantiate a container class for all IDs.

    Args:
      cidb_id: ID of the build in CIDB.
      buildbucket_id: ID of the build in Buildbucket.
    """
    self.cidb_id = cidb_id
    self.buildbucket_id = buildbucket_id


class BuildStore(object):
  """BuildStore class to handle all DB calls."""

  NUM_RESULTS_NO_LIMIT = 1000

  def __init__(self, _read_from_bb=True, _write_to_bb=True,
               _write_to_cidb=True, cidb_creds=None, for_service=None):
    """Get an instance of the BuildStore.

    Args:
      _read_from_bb: Specify the read source.
      _write_to_bb: Determines whether information is written to Buildbucket.
      _write_to_cidb: Determines whether information is written to CIDB.
      cidb_creds: CIDB credentials for scripts running outside of cbuildbot.
      for_service: Argument for CIDBConnection.__init__().
    """
    self._read_from_bb = _read_from_bb
    self._write_to_bb = _write_to_bb
    self._write_to_cidb = _write_to_cidb
    self.cidb_creds = cidb_creds
    self.for_service = for_service
    self.cidb_conn = None
    self.bb_client = None
    self.process_id = os.getpid()

  def _IsCIDBClientMissing(self):
    """Checks to see if CIDB client is needed and is missing.

    Returns:
      Boolean indicating the state of CIDB client.
    """
    need_for_cidb = self._write_to_cidb or not self._read_from_bb
    cidb_is_running = self.cidb_conn is not None

    return need_for_cidb and not cidb_is_running

  def _IsBuildbucketClientMissing(self):
    """Checks to see if Buildbucket v2 client is needed and is missing.

    Returns:
      Boolean indicating the state of Buildbucket v2 client.
    """
    need_for_bb = self._write_to_bb or self._read_from_bb
    bb_is_running = self.bb_client is not None

    return need_for_bb and not bb_is_running

  def GetCIDBHandle(self):
    """Retrieve cidb_conn.

    Returns:
      self.cidb_conn if initialized.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')

    if self.cidb_conn:
      return self.cidb_conn
    else:
      raise BuildStoreException('CIDBConnection not found.')

  def InitializeClients(self):
    """Check if underlying clients are initialized.

    Returns:
      A boolean indicating the client statuses.
    """
    pid_mismatch = (self.process_id != os.getpid())
    if self._IsCIDBClientMissing() or pid_mismatch:
      self.process_id = os.getpid()
      if self.cidb_creds:
        for_service = self.for_service if self.for_service else False
        self.cidb_conn = cidb.CIDBConnection(self.cidb_creds,
                                             for_service=for_service)
      elif not cidb.CIDBConnectionFactory.IsCIDBSetup():
        self.cidb_conn = None
      else:
        self.cidb_conn = (
            cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder())

    if self._IsBuildbucketClientMissing() or pid_mismatch:
      self.bb_client = buildbucket_v2.BuildbucketV2()

    return not (self._IsCIDBClientMissing() or
                self._IsBuildbucketClientMissing())

  def AreClientsReady(self):
    """A front-end function for InitializeClients()."""
    return self.InitializeClients()

  def InsertBuild(self,
                  builder_name,
                  build_number,
                  build_config,
                  bot_hostname,
                  master_build_id=None,
                  timeout_seconds=None,
                  important=None,
                  buildbucket_id=None,
                  branch=None):
    """Insert a build row.

    Args:
      builder_name: buildbot builder name.
      build_number: buildbot build number.
      build_config: cbuildbot config of build
      bot_hostname: hostname of bot running the build
      master_build_id: (Optional) primary key of master build to this build.
      timeout_seconds: (Optional) If provided, total time allocated for this
                       build. A deadline is recorded in CIDB for the current
                       build to end.
      important: (Optional) If provided, the |important| value for this build.
      buildbucket_id: (Optional) If provided, the |buildbucket_id| value for
                       this build.
      branch: (Optional) Manifest branch name of this build.

    Returns:
      build_id: incremental primary ID of the build in CIDB.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    build_id = 0
    if self._write_to_cidb:
      build_id = self.cidb_conn.InsertBuild(
          builder_name, build_number, build_config, bot_hostname,
          master_build_id, timeout_seconds, important, buildbucket_id, branch)
    if self._write_to_bb:
      buildbucket_v2.UpdateSelfCommonBuildProperties(critical=important,
                                                     cidb_id=build_id)

    return build_id

  def GetSlaveStatuses(self, master_build_identifier):
    """Gets the statuses of slave builders to given build.

    Args:
      master_build_identifier: BuildIdentifier of the master build to fetch the
          slave statuses for.

    Returns:
      A list containing a dictionary with keys BUILD_STATUS_KEYS.
      The list contains all child builds of the given master.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if (self._read_from_bb and
        master_build_identifier.buildbucket_id is not None):
      return self.bb_client.GetChildStatuses(
          int(master_build_identifier.buildbucket_id))
    elif not self._read_from_bb and master_build_identifier.cidb_id is not None:
      return self.cidb_conn.GetSlaveStatuses(master_build_identifier.cidb_id,
                                             None)

  def GetKilledChildBuilds(self, build_identifier):
    """Get the child builds that were killed by the given master.

    Args:
      build_identifier: The master build to get children for.

    Returns:
      A list of child buildbucket_ids of the build that were killed.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._read_from_bb:
      if build_identifier.buildbucket_id is not None:
        return self.bb_client.GetKilledChildBuilds(
            int(build_identifier.buildbucket_id))
    else:
      if build_identifier.cidb_id is not None:
        return [int(message['message_value']) for message in
                self.cidb_conn.GetBuildMessages(
                    build_identifier.cidb_id,
                    message_type=constants.MESSAGE_TYPE_IGNORED_REASON,
                    message_subtype=constants.MESSAGE_SUBTYPE_SELF_DESTRUCTION)]

  def GetBuildHistory(
      self, build_config, num_results,
      ignore_build_id=None, start_date=None, end_date=None, branch=None,
      platform_version=None, starting_build_id=None):
    """Returns basic information about most recent builds for build config.

    By default this function returns the most recent builds. Some arguments can
    restrict the result to older builds.

    Args:
      build_config: config name of the build to get history.
      num_results: Number of builds to search back.
      ignore_build_id: (Optional) Ignore a specific build. This is most useful
          to ignore the current build when querying recent past builds from a
          build in flight.
      start_date: (Optional, type: datetime.date) Get builds that occured on or
          after this date.
      end_date: (Optional, type:datetime.date) Get builds that occured on or
          before this date.
      branch: (Optional) Return only results for this branch.
      platform_version: (Optional) Return only results for this
          platform_version.
      starting_build_id: (Optional) The oldest build_id till which builds should
          be retrieved.

    Returns:
      A sorted list of dicts containing up to |number| dictionaries for
      build statuses in descending order (if |reverse| is True, ascending
      order).
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if platform_version or not self._read_from_bb:
      return self.cidb_conn.GetBuildHistory(
          build_config, num_results, ignore_build_id=ignore_build_id,
          start_date=start_date, end_date=end_date, branch=branch,
          platform_version=platform_version,
          starting_build_id=starting_build_id)
    else:
      return self.bb_client.GetBuildHistory(
          build_config, num_results, ignore_build_id=ignore_build_id,
          start_date=start_date, end_date=end_date, branch=branch,
          start_build_id=starting_build_id)

  def InsertBuildStage(self,
                       build_id,
                       name,
                       board=None,
                       status=constants.BUILDER_STATUS_PLANNED):
    """Insert a build stage entry into database.

    Args:
      build_id: primary key of the build in buildTable.
      name: Full name of build stage.
      board: (Optional) board name, if this is a board-specific stage.
      status: (Optional) stage status, one of constants.BUILDER_ALL_STATUSES.
              Default constants.BUILDER_STATUS_PLANNED.

    Returns:
      Integer primary key of inserted stage, i.e. build_stage_id
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._write_to_cidb:
      return self.cidb_conn.InsertBuildStage(build_id, name, board, status)

  def InsertBuildMessage(
      self, build_id, message_type=constants.MESSAGE_TYPE_IGNORED_REASON,
      message_subtype=constants.MESSAGE_SUBTYPE_SELF_DESTRUCTION,
      message_value=None, board=None):
    """Insert a build message into database.

    Args:
      build_id: primary key of build recording this message.
      message_type: Optional str name of message type.
      message_subtype: Optional str name of message subtype.
      message_value: Optional value of message.
      board: Optional str name of the board.
    """
    assert isinstance(message_value, list)
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._write_to_cidb:
      for buildbucket_id in message_value:
        self.cidb_conn.InsertBuildMessage(
            build_id, message_type=message_type,
            message_subtype=message_subtype, message_value=str(buildbucket_id),
            board=board)
    if self._write_to_bb:
      buildbucket_v2.UpdateSelfCommonBuildProperties(
          killed_child_builds=message_value)

  def FinishBuild(self, build_id, status=None, summary=None, metadata_url=None,
                  strict=True):
    """Update the given build row, marking it as finished.

    This should be called once per build, as the last update to the build.
    This will also mark the row's final=True.

    Args:
      build_id: id of row to update.
      status: Final build status, one of
              constants.BUILDER_COMPLETED_STATUSES.
      summary: A summary of the build (failures) collected from all slaves.
      metadata_url: google storage url to metadata.json file for this build,
                    e.g. ('gs://chromeos-image-archive/master-paladin/'
                          'R39-6225.0.0-rc1/metadata.json')
      strict: If |strict| is True, can only update the build status when 'final'
        is False. |strict| can only be False when the caller wants to change the
        entry ignoring the 'final' value (For example, a build was marked as
        status='aborted' and final='true', a cron job to adjust the finish_time
        will call this method with strict=False).
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._write_to_cidb:
      self.cidb_conn.FinishBuild(
          build_id, status=status, summary=summary, metadata_url=metadata_url,
          strict=strict)
    if self._write_to_bb:
      buildbucket_v2.UpdateSelfCommonBuildProperties(metadata_url=metadata_url)

  def FinishChildConfig(self, build_id, child_config, status=None):
    """Marks the given child config as finished with |status|.

    This should be called before FinishBuild, on all child configs that
    were used in a build.

    Args:
      build_id: primary key of the build in the buildTable
      child_config: String child_config name.
      status: Final child_config status, one of
              constants.BUILDER_COMPLETED_STATUSES or None
              for default "inflight".
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._write_to_cidb:
      self.cidb_conn.FinishChildConfig(build_id, child_config, status=status)

  def StartBuildStage(self, build_stage_id):
    """Marks a build stage as inflight, in the database.

    Args:
      build_stage_id: primary key of the build stage in buildStageTable.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._write_to_cidb:
      return self.cidb_conn.StartBuildStage(build_stage_id)

  def WaitBuildStage(self, build_stage_id):
    """Marks a build stage as waiting, in the database.

    Args:
      build_stage_id: primary key of the build stage in buildStageTable.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._write_to_cidb:
      return self.cidb_conn.WaitBuildStage(build_stage_id)

  def FinishBuildStage(self, build_stage_id, status):
    """Marks a build stage as finished, in the database.

    Args:
      build_stage_id: primary key of the build stage in buildStageTable.
      status: one of constants.BUILDER_COMPLETED_STATUSES
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._write_to_cidb:
      return self.cidb_conn.FinishBuildStage(build_stage_id, status)

  def InsertBoardPerBuild(self, build_id, board, board_metadata=None):
    """Inserts board-per-build into the database.

    This function redirects both InsertBoardPerBuild and
    UpdateBoardPerBuildMetadata of CIDB.

    Args:
      build_id: CIDB id of the build.
      board: Board of the build.
      board_metadata: A dict with keys - 'main-firmware-version' and
      'ec-firmware-version'.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if self._write_to_cidb:
      if board_metadata is None:
        self.cidb_conn.InsertBoardPerBuild(build_id, board)
      else:
        self.cidb_conn.UpdateBoardPerBuildMetadata(build_id, board,
                                                   board_metadata)
    if self._write_to_bb:
      if board_metadata is None:
        buildbucket_v2.UpdateSelfCommonBuildProperties(board=board)
      else:
        buildbucket_v2.UpdateSelfCommonBuildProperties(
            board=board,
            main_firmware_version=board_metadata.get('main-firmware-version'),
            ec_firmware_version=board_metadata.get('ec-firmware-version'))

  def UpdateMetadata(self, build_id, metadata):
    """Update the given metadata row in database.

    Args:
      build_id: CIDB id of the build to update.
      metadata: CBuildbotMetadata instance to update with.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    update_status = 0
    if self._write_to_cidb:
      update_status = self.cidb_conn.UpdateMetadata(build_id, metadata)
    if self._write_to_bb:
      buildbucket_v2.UpdateBuildMetadata(metadata)
    return update_status

  def GetBuildsFailures(self, buildbucket_ids=None):
    """Gets the failure entries for all listed buildbucket_ids.

    Args:
      buildbucket_ids: list of build ids of the builds to fetch failures for.

    Returns:
      A list of failure_message_lib.StageFailure instances. This will change
      with Buildbucket implementation.
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if not buildbucket_ids:
      return []
    elif self._read_from_bb:
      failure_list = []
      for buildbucket_id in buildbucket_ids:
        bb_list = self.bb_client.GetStageFailures(int(buildbucket_id))
        for stage in bb_list:
          failure_list.append(failure_message_lib.StageFailure(
              id=None, build_stage_id=None, outer_failure_id=None,
              exception_type=None, exception_message=None,
              exception_category=None, extra_info=None, timestamp=None,
              stage_name=stage['stage_name'], board=None,
              stage_status=stage['stage_status'], build_id=None,
              master_build_id=None, builder_name=None, build_number=None,
              build_config=stage['build_config'],
              build_status=stage['build_status'],
              important=stage['important'], buildbucket_id=buildbucket_id))
      return failure_list
    else:
      return self.cidb_conn.GetBuildsFailures(buildbucket_ids)

  def GetBuildsStages(self, buildbucket_ids):
    """Gets all the stages for all listed build_ids.

    Args:
      buildbucket_ids: list of Buildbucket IDs to query for.

    Returns:
      A list containing, for each stage of the builds found, a dictionary with
      keys (id, build_id, name, board, status, last_updated, start_time,
      finish_time, final).
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if not buildbucket_ids:
      return []
    elif self._read_from_bb:
      stage_list = []
      for buildbucket_id in buildbucket_ids:
        stage_list += self.bb_client.GetBuildStages(int(buildbucket_id))
      return stage_list
    else:
      return self.cidb_conn.GetBuildsStagesWithBuildbucketIds(buildbucket_ids)

  def GetBuildStatuses(self, buildbucket_ids=None, build_ids=None):
    """Retrieve the build statuses of list of builds.

    The two arguments are to be mutually exclusive. If both are provided,
    an error will be raised. If both are absent, an empty list will be returned.

    Args:
      buildbucket_ids: list of buildbucket_id's to query.
      build_ids: list of CIDB id's to query.

    Returns:
      A list of Dictionaries with keys (id, build_config, start_time,
      finish_time, status, platform_version, full_version,
      milestone_version, important).
    """
    if not self.InitializeClients():
      raise BuildStoreException('BuildStore clients could not be initialized.')
    if buildbucket_ids and build_ids:
      raise BuildStoreException('GetBuildStatuses: Cannot process both '
                                'buildbucket_ids and build_ids.')
    # build_ids have to serviced from CIDB. This codepath will be defunct after
    # CQ is shut down.
    if build_ids:
      return self.cidb_conn.GetBuildStatuses(build_ids)
    elif not buildbucket_ids:
      return []
    elif self._read_from_bb:
      return [self.bb_client.GetBuildStatus(int(buildbucket_id))
              for buildbucket_id in buildbucket_ids]
    elif not self._read_from_bb:
      return self.cidb_conn.GetBuildStatusesWithBuildbucketIds(
          buildbucket_ids)


#pylint: disable=unused-argument
class FakeBuildStore(object):
  """Fake BuildStore class to be used only in unittests."""

  def __init__(self, fake_cidb_conn=None):
    super(FakeBuildStore, self).__init__()
    if fake_cidb_conn:
      self.fake_cidb = fake_cidb_conn
    else:
      self.fake_cidb = fake_cidb.FakeCIDBConnection()

  def InitializeClients(self):
    return True

  def AreClientsReady(self):
    return True

  def GetCIDBHandle(self):
    return self.fake_cidb

  def InsertBuild(self,
                  builder_name,
                  build_number,
                  build_config,
                  bot_hostname,
                  master_build_id=None,
                  timeout_seconds=None,
                  status=constants.BUILDER_STATUS_PASSED,
                  important=None,
                  buildbucket_id=None,
                  milestone_version=None,
                  platform_version=None,
                  start_time=None,
                  build_type=None,
                  branch=None):
    build_id = self.fake_cidb.InsertBuild(
        builder_name, build_number, build_config, bot_hostname, master_build_id,
        timeout_seconds, status, important, buildbucket_id, milestone_version,
        platform_version, start_time, build_type, branch)
    return build_id

  def GetBuildHistory(
      self, build_config, num_results, ignore_build_id=None, start_date=None,
      end_date=None, branch=None, platform_version=None, starting_build_id=None,
      ending_build_id=None):
    return self.fake_cidb.GetBuildHistory(
        build_config, num_results, ignore_build_id=ignore_build_id,
        start_date=start_date, end_date=end_date,
        platform_version=platform_version, starting_build_id=starting_build_id)

  def InsertBuildStage(self,
                       build_id,
                       name,
                       board=None,
                       status=constants.BUILDER_STATUS_PLANNED):
    build_stage_id = self.fake_cidb.InsertBuildStage(build_id, name, board,
                                                     status)
    return build_stage_id

  def GetSlaveStatuses(self, master_build_id, buildbucket_ids=None):
    return self.fake_cidb.GetSlaveStatuses(master_build_id, buildbucket_ids)

  def GetKilledChildBuilds(self, build_identifier):
    return [m['message_value'] for m in self.fake_cidb.GetBuildMessages(
        build_identifier.cidb_id,
        message_type=constants.MESSAGE_TYPE_IGNORED_REASON,
        message_subtype=constants.MESSAGE_SUBTYPE_SELF_DESTRUCTION)]

  def InsertBuildMessage(
      self, build_id, message_type=constants.MESSAGE_TYPE_IGNORED_REASON,
      message_subtype=constants.MESSAGE_SUBTYPE_SELF_DESTRUCTION,
      message_value=None, board=None):
    for buildbucket_id in message_value:
      self.fake_cidb.InsertBuildMessage(
          build_id, message_type=message_type,
          message_subtype=message_subtype, message_value=str(buildbucket_id),
          board=board)

  def FinishBuild(self, build_id, status=None, summary=None, metadata_url=None,
                  strict=True):
    return

  def StartBuildStage(self, build_stage_id):
    return build_stage_id

  def WaitBuildStage(self, build_stage_id):
    return build_stage_id

  def FinishBuildStage(self, build_stage_id, status):
    return build_stage_id

  def InsertBoardPerBuild(self, build_id, board, board_metadata=None):
    pass

  def UpdateMetadata(self, build_id, metadata):
    return

  def GetBuildsFailures(self, buildbucket_ids=None):
    if buildbucket_ids:
      return self.fake_cidb.GetBuildsFailures(buildbucket_ids)
    else:
      return []

  def GetBuildsStages(self, buildbucket_ids=None, build_ids=None):
    if buildbucket_ids:
      return self.fake_cidb.GetBuildsStagesWithBuildbucketIds(buildbucket_ids)
    elif build_ids:
      return self.fake_cidb.GetBuildsStages(build_ids)
    else:
      return []

  def GetBuildStatuses(self, buildbucket_ids=None, build_ids=None):
    if buildbucket_ids:
      return self.fake_cidb.GetBuildStatusesWithBuildbucketIds(buildbucket_ids)
    elif build_ids:
      return self.fake_cidb.GetBuildStatuses(build_ids)
    else:
      return []
