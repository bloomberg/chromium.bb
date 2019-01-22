# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for buildstore library."""

from __future__ import print_function

import mock

from chromite.lib import cidb
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import buildstore

BuildStore = buildstore.BuildStore


class TestBuildStore(cros_test_lib.MockTestCase):
  """Test buildstore.BuildStore."""

  def testInitializeClientsWithoutCIDBSetup(self):
    """Tests InitializeClients without CIDB."""
    bs = BuildStore()
    bs.cidb_conn = None
    self.assertEqual(bs.InitializeClients(), False)
    self.assertEqual(bs.cidb_conn, None)

  def testInitializeClientsWithCIDBSetup(self):
    """Tests InitializeClients with mock CIDB."""

    class DummyCIDBConnection(object):
      """Dummy class representing CIDBConnection."""

    # With CIDB setup, cidb_conn is populated.
    self.PatchObject(cidb.CIDBConnectionFactory, 'IsCIDBSetup',
                     return_value=True)
    mock_cidb = DummyCIDBConnection()
    self.PatchObject(cidb.CIDBConnectionFactory,
                     'GetCIDBConnectionForBuilder',
                     return_value=mock_cidb)
    bs = BuildStore()
    bs.InitializeClients()
    self.assertEqual(bs.cidb_conn, mock_cidb)

  def testInitializeClientsWhenCIDBIsNotNeeded(self):
    """Test InitializeClients without CIDB requirement."""
    bs = BuildStore()
    bs.cidb_conn = None
    self.PatchObject(BuildStore, '_IsCIDBClientMissing',
                     return_value=False)
    # Does not raise exception.
    bs.InitializeClients()

  def testInsertBuild(self):
    """Tests the redirect for InsertBuild function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'InsertBuild',
                     return_value=constants.MOCK_BUILD_ID)
    build_id = bs.InsertBuild(
        'builder_name', 12345,
        'something-paladin', 'bot_hostname')
    bs.cidb_conn.InsertBuild.assert_called_once_with(
        'builder_name', 12345, 'something-paladin', 'bot_hostname',
        None, None, None, None, None)
    self.assertEqual(build_id, constants.MOCK_BUILD_ID)

  def testInsertBuildStage(self):
    """Tests the redirect for InsertBuildStage function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'InsertBuildStage',
                     return_value=constants.MOCK_STAGE_ID)
    build_stage_id = bs.InsertBuildStage(
        constants.MOCK_BUILD_ID, 'stage_name')
    bs.cidb_conn.InsertBuildStage.assert_called_once_with(
        constants.MOCK_BUILD_ID, 'stage_name', None,
        constants.BUILDER_STATUS_PLANNED)
    self.assertEqual(build_stage_id, constants.MOCK_STAGE_ID)

  def testUpdateMetadata(self):
    """Tests the redirect for UpdateMetadata function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    fake_metadata = {}
    self.PatchObject(bs.cidb_conn, 'UpdateMetadata')
    bs.UpdateMetadata(constants.MOCK_BUILD_ID, fake_metadata)
    bs.cidb_conn.UpdateMetadata.assert_called_once_with(
        constants.MOCK_BUILD_ID, fake_metadata)

  def testExtendDeadline(self):
    """Tests the redirect for ExtendDeadline function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    mock_timeout = mock.Mock()
    self.PatchObject(bs.cidb_conn, 'ExtendDeadline')
    bs.ExtendDeadline(constants.MOCK_BUILD_ID, mock_timeout)
    bs.cidb_conn.ExtendDeadline.assert_called_once_with(
        constants.MOCK_BUILD_ID, mock_timeout)
