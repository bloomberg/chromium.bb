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

from chromite.lib import cidb
from chromite.lib import constants
from chromite.lib import fake_cidb


class BuildStoreException(Exception):
  """General exception class for this module."""


class BuildStore(object):
  """BuildStore class to handle all DB calls."""

  def __init__(self, _read_from_bb=False, _write_to_bb=False,
               _write_to_cidb=True):
    """Get an instance of the BuildStore.

    Args:
      _read_from_bb: Specify the read source.
      _write_to_bb: Determines whether information is written to Buildbucket.
      _write_to_cidb: Determines whether information is written to CIDB.
    """
    self._read_from_bb = _read_from_bb
    self._write_to_bb = _write_to_bb
    self._write_to_cidb = _write_to_cidb
    self.cidb_conn = None
    self.process_id = os.getpid()
    self.process_id = os.getpid()

  def _IsCIDBClientMissing(self):
    """Checks to see CIDB client if needed and is missing.

    Returns:
      Boolean indicating the state of CIDB client.
    """
    need_for_cidb = self._write_to_cidb or not self._read_from_bb
    cidb_is_running = self.cidb_conn is not None

    return need_for_cidb and not cidb_is_running

  def GetCIDBHandle(self):
    """Retrieve cidb_conn.

    Returns:
      self.cidb_conn if initialized.
    """
    self.InitializeClients()
    if self.cidb_conn:
      return self.cidb_conn
    else:
      raise BuildStoreException('CIDBConnection not found.')

  def InitializeClients(self):
    """Check if underlying clients are initialized."""
    pid_mismatch = (self.process_id != os.getpid())
    if self._IsCIDBClientMissing() or pid_mismatch:
      self.process_id = os.getpid()
      if not cidb.CIDBConnectionFactory.IsCIDBSetup():
        self.cidb_conn = None
      else:
        self.cidb_conn = cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder(
        )

    if self._IsCIDBClientMissing():
      raise BuildStoreException('CIDB Connection could not be initialized.')

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
    self.InitializeClients()
    if self._write_to_cidb:
      return self.cidb_conn.InsertBuild(
          builder_name, build_number, build_config, bot_hostname,
          master_build_id, timeout_seconds, important, buildbucket_id, branch)

  def GetBuildMessages(self, build_id, message_type=None, message_subtype=None):
    """Gets build messages for particular build_id.

    Args:
      build_id: The build to get messages for.
      message_type: Get messages with the specific message_type (string) if
        message_type is not None.
      message_subtype: Get messages with the specific message_subtype (string)
        if message_subtype is not None.

    Returns:
      A list of build message dictionaries, where each dictionary contains
      keys build_id, build_config, builder_name, build_number, message_type,
      message_subtype, message_value, timestamp, board.
    """
    self.InitializeClients()
    if not self._read_from_bb:
      return self.cidb_conn.GetBuildMessages(build_id, message_type,
                                             message_subtype)

  def InsertBuildStage(self,
                       build_id,
                       name,
                       board=None,
                       status=constants.BUILDER_STATUS_PLANNED):
    """Insert a build stage entry into database.

    Args:
      build_id: primary key of the build in buildTable
      name: Full name of build stage.
      board: (Optional) board name, if this is a board-specific stage.
      status: (Optional) stage status, one of constants.BUILDER_ALL_STATUSES.
              Default constants.BUILDER_STATUS_PLANNED.

    Returns:
      Integer primary key of inserted stage, i.e. build_stage_id
    """
    self.InitializeClients()
    if self._write_to_cidb:
      return self.cidb_conn.InsertBuildStage(build_id, name, board, status)


class FakeBuildStore(object):
  """Fake BuildStore class to be used only in unittests."""

  def __init__(self, fake_cidb_conn=None):
    super(FakeBuildStore, self).__init__()
    if fake_cidb_conn:
      self.fake_cidb = fake_cidb_conn
    else:
      self.fake_cidb = fake_cidb.FakeCIDBConnection()

  def InitializeClients(self):
    return

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

  def InsertBuildStage(self,
                       build_id,
                       name,
                       board=None,
                       status=constants.BUILDER_STATUS_PLANNED):
    build_stage_id = self.fake_cidb.InsertBuildStage(build_id, name, board,
                                                     status)
    return build_stage_id
