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
from chromite.lib import buildbucket_v2

BuildStore = buildstore.BuildStore


class TestBuildStore(cros_test_lib.MockTestCase):
  """Test buildstore.BuildStore."""

  def testIsBuildbucketClientMissing(self):
    """Tests _IsBuildbucketClientMissing function."""
    # pylint: disable=protected-access
    # Test Buildbucket needed and client missing.
    bs = BuildStore(_read_from_bb=True, _write_to_bb=True)
    self.assertEqual(bs._IsBuildbucketClientMissing(), True)
    bs = BuildStore(_read_from_bb=True, _write_to_bb=False)
    self.assertEqual(bs._IsBuildbucketClientMissing(), True)
    bs = BuildStore(_read_from_bb=False, _write_to_bb=True)
    self.assertEqual(bs._IsBuildbucketClientMissing(), True)
    # Test Buildbucket is needed and client is up and running.
    bs = BuildStore(_read_from_bb=True, _write_to_bb=True)
    bs.bb_client = object()
    self.assertEqual(bs._IsBuildbucketClientMissing(), False)
    bs = BuildStore(_read_from_bb=False, _write_to_bb=True)
    bs.bb_client = object()
    self.assertEqual(bs._IsBuildbucketClientMissing(), False)
    bs = BuildStore(_read_from_bb=True, _write_to_bb=False)
    bs.bb_client = object()
    self.assertEqual(bs._IsBuildbucketClientMissing(), False)
    # Test Buildbucket is not needed.
    bs = BuildStore(_read_from_bb=False, _write_to_bb=False)
    self.assertEqual(bs._IsBuildbucketClientMissing(), False)

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
    result = bs.InitializeClients()
    self.assertEqual(bs.cidb_conn, mock_cidb)
    self.assertEqual(result, True)

  def testInitializeClientsWithoutCIDBSetup(self):
    """Tests InitializeClients with mock CIDB."""

    self.PatchObject(cidb.CIDBConnectionFactory, 'IsCIDBSetup',
                     return_value=False)
    bs = BuildStore()
    self.assertEqual(bs.InitializeClients(), False)

  def testInitializeClientsWhenCIDBIsNotNeeded(self):
    """Test InitializeClients without CIDB requirement."""
    bs = BuildStore(_read_from_bb=True, _write_to_cidb=False)
    bs.cidb_conn = None
    self.PatchObject(BuildStore, '_IsBuildbucketClientMissing',
                     return_value=False)
    # Does not raise exception.
    self.assertEqual(bs.InitializeClients(), True)

  def testInitializeClientsWithBuildbucketSetup(self):
    """Tests InitializeClients with mock Buildbucket."""
    bs = BuildStore()
    self.PatchObject(bs, '_IsCIDBClientMissing',
                     return_value=False)
    result = bs.InitializeClients()
    self.assertTrue(isinstance(bs.bb_client, buildbucket_v2.BuildbucketV2))
    self.assertEqual(result, True)

  def testInitializeClientsWhenBuildbucketIsNotNeeded(self):
    """Test InitializeClients without Buildbucket requirement."""
    bs = BuildStore(_read_from_bb=False, _write_to_bb=False)
    self.PatchObject(BuildStore, '_IsCIDBClientMissing',
                     return_value=False)
    # Does not raise exception.
    self.assertEqual(bs.InitializeClients(), True)

  def testInsertBuild(self):
    """Tests the redirect for InsertBuild function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    # Test CIDB redirect.
    bs = BuildStore(_write_to_cidb=True, _write_to_bb=False)
    bs.cidb_conn = mock.MagicMock()
    self.PatchObject(bs.cidb_conn, 'InsertBuild',
                     return_value=constants.MOCK_BUILD_ID)
    build_id = bs.InsertBuild(
        'builder_name', 12345,
        'something-paladin', 'bot_hostname', master_build_id='master_id',
        timeout_seconds='timeout')
    bs.cidb_conn.InsertBuild.assert_called_once_with(
        'builder_name', 12345, 'something-paladin', 'bot_hostname',
        'master_id', 'timeout', None, None, None)
    self.assertEqual(build_id, constants.MOCK_BUILD_ID)
    # Test Buildbucket redirect.
    bs = BuildStore(_write_to_cidb=False, _write_to_bb=True)
    bs.bb_client = mock.MagicMock()
    self.PatchObject(buildbucket_v2, 'UpdateSelfCommonBuildProperties')
    build_id = bs.InsertBuild(
        'builder_name', 12345,
        'something-paladin', 'bot_hostname', important=True,
        timeout_seconds='timeout')
    buildbucket_v2.UpdateSelfCommonBuildProperties.assert_called_once_with(
        critical=True)
    self.assertEqual(build_id, 0)

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

  def testFinishBuild(self):
    """Tests the redirect for FinishBuild function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    status = mock.Mock()
    summary = mock.Mock()
    metadata_url = mock.Mock()
    strict = mock.Mock()
    self.PatchObject(bs.cidb_conn, 'FinishBuild')
    bs.FinishBuild(constants.MOCK_BUILD_ID, status=status, summary=summary,
                   metadata_url=metadata_url, strict=strict)
    bs.cidb_conn.FinishBuild.assert_called_once_with(
        constants.MOCK_BUILD_ID, status=status, summary=summary,
        metadata_url=metadata_url, strict=strict)

  def testFinishChildConfig(self):
    """Tests the redirect for FinishChildConfig function."""
    self.PatchObject(BuildStore, 'InitializeClients',
                     return_value=True)
    bs = BuildStore()
    bs.cidb_conn = mock.MagicMock()
    child_config = mock.Mock()
    status = mock.Mock()
    self.PatchObject(bs.cidb_conn, 'FinishChildConfig')
    bs.FinishChildConfig(constants.MOCK_BUILD_ID, child_config, status=status)
    bs.cidb_conn.FinishChildConfig.assert_called_once_with(
        constants.MOCK_BUILD_ID, child_config, status=status)

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
