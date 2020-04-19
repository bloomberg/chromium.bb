# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for builder_status_lib."""

from __future__ import print_function

import sys

from chromite.lib import builder_status_lib
from chromite.lib import cidb
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import failure_message_lib
from chromite.lib import failure_message_lib_unittest
from chromite.lib.buildstore import FakeBuildStore, BuildIdentifier


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


stage_failure_helper = failure_message_lib_unittest.StageFailureHelper


def ConstructFailureMessages(build_config):
  """Helper method to construct failure messages."""
  entry_1 = stage_failure_helper.GetStageFailure(
      build_config=build_config, failure_id=1)
  entry_2 = stage_failure_helper.GetStageFailure(
      build_config=build_config, failure_id=2, outer_failure_id=1)
  entry_3 = stage_failure_helper.GetStageFailure(
      build_config=build_config, failure_id=3, outer_failure_id=1)
  failure_entries = [entry_1, entry_2, entry_3]
  failure_messages = (
      failure_message_lib.FailureMessageManager.ConstructStageFailureMessages(
          failure_entries))

  return failure_messages


class BuilderStatusLibTests(cros_test_lib.MockTestCase):
  """Tests for builder_status_lib."""

  def testGetSlavesAbortedBySelfDestructedMaster(self):
    """Test GetSlavesAbortedBySelfDestructedMaster with aborted slaves."""
    db = fake_cidb.FakeCIDBConnection()
    buildstore = FakeBuildStore(db)
    cidb.CIDBConnectionFactory.SetupMockCidb(db)
    master_build_id = db.InsertBuild(
        'master', 1, 'master', 'bot_hostname',
        buildbucket_id=1234)
    master_build_identifier = BuildIdentifier(cidb_id=master_build_id,
                                              buildbucket_id=1234)

    self.assertEqual(
        set(),
        builder_status_lib.GetSlavesAbortedBySelfDestructedMaster(
            master_build_identifier, buildstore))

    db.InsertBuild(
        'slave_1', 1, 'slave_1', 'bot_hostname',
        master_build_id=master_build_id, buildbucket_id=12)
    db.InsertBuild(
        'slave_2', 2, 'slave_2', 'bot_hostname',
        master_build_id=master_build_id, buildbucket_id=23)
    db.InsertBuild(
        'slave_3', 3, 'slave_3', 'bot_hostname',
        master_build_id=master_build_id, buildbucket_id=34)
    for slave_build_id in (12, 23):
      db.InsertBuildMessage(
          master_build_id,
          message_type=constants.MESSAGE_TYPE_IGNORED_REASON,
          message_subtype=constants.MESSAGE_SUBTYPE_SELF_DESTRUCTION,
          message_value=str(slave_build_id))
    self.assertEqual(
        {'slave_1', 'slave_2'},
        builder_status_lib.GetSlavesAbortedBySelfDestructedMaster(
            BuildIdentifier(cidb_id=master_build_id,
                            buildbucket_id=1234), buildstore))


# pylint: disable=protected-access
class BuilderStatusManagerTest(cros_test_lib.MockTestCase):
  """Tests for BuilderStatusManager."""

  def setUp(self):
    self.db = fake_cidb.FakeCIDBConnection()

  def testCreateBuildFailureMessageWithMessages(self):
    """Test CreateBuildFailureMessage with stage failure messages."""
    overlays = constants.PRIVATE_OVERLAYS
    dashboard_url = 'http://fake_dashboard_url'
    slave = 'cyan-paladin'
    failure_messages = ConstructFailureMessages(slave)

    build_msg = (
        builder_status_lib.BuilderStatusManager.CreateBuildFailureMessage(
            slave, overlays, dashboard_url, failure_messages))

    self.assertTrue('the builder failed' in build_msg.message_summary)
    self.assertTrue(build_msg.internal)
    self.assertEqual(build_msg.builder, slave)

  def testCreateBuildFailureMessageWithoutMessages(self):
    """Test CreateBuildFailureMessage without stage failure messages."""
    overlays = constants.PUBLIC_OVERLAYS
    dashboard_url = 'http://fake_dashboard_url'
    slave = 'cyan-paladin'

    build_msg = (
        builder_status_lib.BuilderStatusManager.CreateBuildFailureMessage(
            slave, overlays, dashboard_url, None))

    self.assertTrue('cbuildbot failed' in build_msg.message_summary)
    self.assertFalse(build_msg.internal)
    self.assertEqual(build_msg.builder, slave)

  def testCreateBuildFailureMessageWhenCanceled(self):
    """Test CreateBuildFailureMessage with no stage failure and canceled"""
    overlays = constants.PRIVATE_OVERLAYS
    dashboard_url = 'http://fake_dashboard_url'
    slave = 'cyan-paladin'

    build_msg = (
        builder_status_lib.BuilderStatusManager.CreateBuildFailureMessage(
            slave, overlays, dashboard_url, None,
            aborted_by_self_destruction=True))

    self.assertTrue('aborted by self-destruction' in build_msg.message_summary)
    self.assertFalse('cbuildbot failed' in build_msg.message_summary)
    self.assertEqual(build_msg.builder, slave)

  def testCreateBuildFailureMessageSupersedesCancellation(self):
    """Test CreateBuildFailureMessage with a stage failure when canceled"""
    overlays = constants.PRIVATE_OVERLAYS
    dashboard_url = 'http://fake_dashboard_url'
    slave = 'cyan-paladin'
    failure_messages = ConstructFailureMessages(slave)

    build_msg = (
        builder_status_lib.BuilderStatusManager.CreateBuildFailureMessage(
            slave, overlays, dashboard_url, failure_messages,
            aborted_by_self_destruction=True))

    self.assertFalse('canceled by master' in build_msg.message_summary)
    self.assertFalse('cbuildbot failed' in build_msg.message_summary)
    self.assertEqual(build_msg.builder, slave)
