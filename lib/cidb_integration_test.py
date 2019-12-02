# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration tests for cidb.py module."""

from __future__ import print_function

import datetime
import glob
import os
import random
import shutil

from chromite.lib import constants
from chromite.lib import metadata_lib
from chromite.lib import cidb
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


# Hopefully cidb will be deleted soon (before Python 3 finalizes), so we don't
# have to fully port this module.
# pylint: disable=long-suffix

# pylint: disable=protected-access

# Used to ensure that all build_number values we use are unique.
def _random():
  return random.randint(1, 1000000000)


SERIES_0_TEST_DATA_PATH = os.path.join(
    constants.CHROMITE_DIR, 'cidb', 'test_data', 'series_0')

SERIES_1_TEST_DATA_PATH = os.path.join(
    constants.CHROMITE_DIR, 'cidb', 'test_data', 'series_1')


class CIDBIntegrationTest(cros_test_lib.LocalSqlServerTestCase):
  """Base class for cidb tests that connect to a test MySQL instance."""

  CIDB_USER_ROOT = 'root'
  CIDB_USER_BOT = 'bot'
  CIDB_USER_READONLY = 'readonly'

  CIDB_CREDS_DIR = {
      CIDB_USER_BOT: os.path.join(constants.SOURCE_ROOT, 'crostools', 'cidb',
                                  'cidb_test_bot'),
      CIDB_USER_READONLY: os.path.join(constants.SOURCE_ROOT, 'crostools',
                                       'cidb', 'cidb_test_readonly'),
  }

  def LocalCIDBConnection(self, cidb_user):
    """Create a CIDBConnection with the local mysqld instance.

    Args:
      cidb_user: The mysql user to connect as.

    Returns:
      The created CIDBConnection object.
    """
    creds_dir_path = os.path.join(self.tempdir, 'local_cidb_creds')
    osutils.RmDir(creds_dir_path, ignore_missing=True)
    osutils.SafeMakedirs(creds_dir_path)

    osutils.WriteFile(os.path.join(creds_dir_path, 'host.txt'),
                      self.mysqld_host)
    osutils.WriteFile(os.path.join(creds_dir_path, 'port.txt'),
                      str(self.mysqld_port))
    osutils.WriteFile(os.path.join(creds_dir_path, 'user.txt'), cidb_user)

    if cidb_user in self.CIDB_CREDS_DIR:
      shutil.copy(os.path.join(self.CIDB_CREDS_DIR[cidb_user], 'password.txt'),
                  creds_dir_path)

    return cidb.CIDBConnection(
        creds_dir_path,
        query_retry_args=cidb.SqlConnectionRetryArgs(4, 1, 1.1))

  def _PrepareFreshDatabase(self, max_schema_version=None):
    """Create an empty database with migrations applied.

    Args:
      max_schema_version: The highest schema version migration to apply,
      defaults to None in which case all migrations will be applied.

    Returns:
      A CIDBConnection instance, connected to a an empty database as the
      root user.
    """
    # Note: We do not use the cidb.CIDBConnectionFactory
    # in this module. That factory method is used only to construct
    # connections as the bot user, which is how the builders will always
    # connect to the database. In this module, however, we need to test
    # database connections as other mysql users.

    # Connect to database and drop its contents.
    db = self.LocalCIDBConnection(self.CIDB_USER_ROOT)
    db.DropDatabase()

    # Connect to now fresh database and apply migrations.
    db = self.LocalCIDBConnection(self.CIDB_USER_ROOT)
    db.ApplySchemaMigrations(max_schema_version)

    return db

  def _PrepareDatabase(self):
    """Prepares a database at the latest known schema version.

    If database already exists, do not delete existing database. This
    optimization can save a lot of time, when used by tests that do not
    require an empty database.
    """
    # Connect to now fresh database and apply migrations.
    db = self.LocalCIDBConnection(self.CIDB_USER_ROOT)
    db.ApplySchemaMigrations()

    return db

class SchemaDumpTest(CIDBIntegrationTest):
  """Ensure that schema.dump remains current."""

  def mysqlDump(self):
    """Helper to dump the schema.

    Returns:
      CIDB database schema as a single string.
    """
    cmd = [
        'mysqldump',
        '-S', os.path.join(self._mysqld_dir, 'mysqld.socket'),
        '-u', 'root',
        '--no-data',
        '--single-transaction',
        # '--skip-comments',  # Required to avoid dumping a timestamp.
        'cidb',
    ]
    result = cros_build_lib.run(cmd, capture_output=True, quiet=True)

    # Strip out comment lines, to avoid dumping a problematic timestamp.
    lines = [l for l in result.output.splitlines() if not l.startswith('--')]
    return '\n'.join(lines)

  def testDump(self):
    """Ensure generated file is up to date."""
    DUMP_FILE = os.path.join(constants.CHROMITE_DIR, 'cidb', 'schema.dump')

    self._PrepareFreshDatabase()

    # schema.dump
    new_dump = self.mysqlDump()
    old_dump = osutils.ReadFile(DUMP_FILE)

    if new_dump != old_dump:
      if cros_test_lib.GlobalTestConfig.UPDATE_GENERATED_FILES:
        osutils.WriteFile(DUMP_FILE, new_dump)
      else:
        self.fail('schema.dump does not match the '
                  'migrations generated schema. Run '
                  'lib/cidb_integration_test --update')


class CIDBMigrationsTest(CIDBIntegrationTest):
  """Test that all migrations apply correctly."""

  def testMigrations(self):
    """Test that all migrations apply in bulk correctly."""
    self._PrepareFreshDatabase()

  def testIncrementalMigrations(self):
    """Test that all migrations apply incrementally correctly."""
    db = self._PrepareFreshDatabase(0)
    migrations = db._GetMigrationScripts()
    max_version = migrations[-1][0]

    for i in range(1, max_version + 1):
      db.ApplySchemaMigrations(i)

  def testWaterfallMigration(self):
    """Test that migrating waterfall from enum to varchar preserves value."""
    self.skipTest('Skipped obsolete waterfall migration test.')
    # This test no longer runs. It was used only to confirm the correctness of
    # migration #41. In #43, the InsertBuild API changes in a way that is not
    # compatible with this test.
    # The test code remains in place for demonstration purposes only.
    db = self._PrepareFreshDatabase(40)
    build_id = db.InsertBuild('my builder', _random(),
                              'my config', 'my bot hostname')
    db.ApplySchemaMigrations(41)
    self.assertEqual('chromiumos', db.GetBuildStatus(build_id)['waterfall'])


class CIDBAPITest(CIDBIntegrationTest):
  """Tests of the CIDB API."""

  def testGetTime(self):
    db = self._PrepareFreshDatabase(1)
    current_db_time = db.GetTime()
    self.assertEqual(type(current_db_time), datetime.datetime)

  def testGetBuildStatusKeys(self):
    db = self._PrepareFreshDatabase()
    build_id = db.InsertBuild('builder_name', 1, 'build_config',
                              'bot_hostname')
    build_status = db.GetBuildStatus(build_id)
    for k in db.BUILD_STATUS_KEYS:
      self.assertIn(k, build_status)

  def testBuildMessages(self):
    db = self._PrepareFreshDatabase(65)
    self.assertEqual([], db.GetBuildMessages(1))
    master_build_id = db.InsertBuild('builder name',
                                     1,
                                     'master',
                                     'hostname')
    slave_build_id = db.InsertBuild('slave builder name',
                                    2,
                                    'slave',
                                    'slave hostname',
                                    master_build_id=master_build_id)
    db.InsertBuildMessage(master_build_id)
    db.InsertBuildMessage(master_build_id, 'message_type', 'message_subtype',
                          'message_value', 'board')
    for i in range(10):
      db.InsertBuildMessage(slave_build_id,
                            'message_type', 'message_subtype', str(i), 'board')

    master_messages = db.GetBuildMessages(master_build_id)
    master_messages_right_type = db.GetBuildMessages(
        master_build_id, message_type='message_type',
        message_subtype='message_subtype')
    master_messages_wrong_type = db.GetBuildMessages(
        master_build_id, message_type='wrong_message_type',
        message_subtype='wrong_message_subtype')

    self.assertEqual(2, len(master_messages))
    self.assertEqual(1, len(master_messages_right_type))
    self.assertEqual(0, len(master_messages_wrong_type))

    mm2 = master_messages[1]
    mm2.pop('timestamp')
    self.assertEqual({'build_id': master_build_id,
                      'build_config': 'master',
                      'waterfall': '',
                      'builder_name': 'builder name',
                      'build_number': 1L,
                      'message_type': 'message_type',
                      'message_subtype': 'message_subtype',
                      'message_value': 'message_value',
                      'board': 'board'},
                     mm2)
    message_right_type = master_messages_right_type[0]
    message_right_type.pop('timestamp')
    self.assertEqual(message_right_type, mm2)


def GetTestDataSeries(test_data_path):
  """Get metadata from json files at |test_data_path|.

  Returns:
    A list of CBuildbotMetadata objects, sorted by their start time.
  """
  filenames = glob.glob(os.path.join(test_data_path, '*.json'))
  metadatas = []
  for fname in filenames:
    metadatas.append(
        metadata_lib.CBuildbotMetadata.FromJSONString(osutils.ReadFile(fname)))

  # Convert start time values, which are stored in RFC 2822 string format,
  # to seconds since epoch.
  timestamp_from_dict = lambda x: cros_build_lib.ParseUserDateTimeFormat(
      x.GetDict()['time']['start'])

  metadatas.sort(key=timestamp_from_dict)
  return metadatas


class DataSeries0Test(CIDBIntegrationTest):
  """Simulate a set of 630 master/slave CQ builds."""

  def testCQWithSchema56(self):
    """Run the CQ test with schema version 65."""
    db = self._PrepareFreshDatabase(65)
    self._runCQTest(db)

  def _runCQTest(self, db):
    """Simulate a set of 630 master/slave CQ builds.

    Note: This test takes about 2.5 minutes to populate its 630 builds
    and their corresponding cl actions into the test database.
    """
    metadatas = GetTestDataSeries(SERIES_0_TEST_DATA_PATH)
    self.assertEqual(len(metadatas), 630, 'Did not load expected amount of '
                                          'test data')

    # Perform some sanity check queries against the database, connected
    # as the readonly user. Apply schema migrations first to ensure that we can
    # use the latest version or the readonly password.
    db.ApplySchemaMigrations()
    readonly_db = self.LocalCIDBConnection(self.CIDB_USER_READONLY)

    self._start_and_finish_time_checks(readonly_db)

    build_types = readonly_db._GetEngine().execute(
        'select build_type from buildTable').fetchall()
    self.assertTrue(all(x == ('paladin',) for x in build_types))

    build_config_count = readonly_db._GetEngine().execute(
        'select COUNT(distinct build_config) from buildTable').fetchall()[0][0]
    self.assertEqual(build_config_count, 30)

    # Test the _Select method, and verify that the first inserted
    # build is a master-paladin build.
    first_row = readonly_db._Select('buildTable', 1, ['id', 'build_config'])
    self.assertEqual(first_row['build_config'], 'master-paladin')

    # First master build has 29 slaves. Build with id 2 is a slave
    # build with no slaves of its own.
    self.assertEqual(len(readonly_db.GetSlaveStatuses(1)), 29)
    self.assertEqual(len(readonly_db.GetSlaveStatuses(2)), 0)

    # Make sure we can get build status by build id.
    self.assertEqual(readonly_db.GetBuildStatus(2).get('id'), 2)

    # Make sure we can get build statuses by build ids.
    build_dicts = readonly_db.GetBuildStatuses([1, 2])
    self.assertEqual([x.get('id') for x in build_dicts], [1, 2])

    self._start_and_finish_time_checks(readonly_db)
    self._last_updated_time_checks(readonly_db)

    # | Test get build_status from -- here's the relevant data from
    # master-paladin
    # |          id | status |
    # |         601 | pass   |
    # |         571 | pass   |
    # |         541 | fail   |
    # |         511 | pass   |
    # |         481 | pass   |
    # From 1929 because we always go back one build first.
    last_status = readonly_db.GetBuildHistory('master-paladin', 1)
    self.assertEqual(len(last_status), 1)
    last_status = readonly_db.GetBuildHistory('master-paladin', 5)
    self.assertEqual(len(last_status), 5)
    last_status = readonly_db.GetBuildHistory('master-paladin', 5,
                                              milestone_version=52)
    self.assertEqual(len(last_status), 0)
    last_status = readonly_db.GetBuildHistory('master-paladin', 1,
                                              platform_version='6029.0.0-rc1')
    self.assertEqual(len(last_status), 1)
    self.assertEqual(last_status[0]['platform_version'], '6029.0.0-rc1')
    last_status = readonly_db.GetBuildHistory('master-paladin', 1,
                                              platform_version='6029.0.0-rc2')
    self.assertEqual(len(last_status), 1)
    self.assertEqual(last_status[0]['platform_version'], '6029.0.0-rc2')
    last_status = readonly_db.GetBuildHistory('master-paladin', 1,
                                              platform_version='6029.0.0-rc3')
    self.assertEqual(len(last_status), 0)
    # Make sure keys are sorted correctly.
    build_ids = []
    for index, status in enumerate(last_status):
      # Add these to list to confirm they are sorted afterwards correctly.
      # Should be descending.
      build_ids.append(status['id'])
      if index == 2:
        self.assertEqual(status['status'], 'fail')
      else:
        self.assertEqual(status['status'], 'pass')

    # Check the sort order.
    self.assertEqual(sorted(build_ids, reverse=True), build_ids)

  def _last_updated_time_checks(self, db):
    """Sanity checks on the last_updated column."""
    # We should have a diversity of last_updated times. Since the timestamp
    # resolution is only 1 second, and we have lots of parallelism in the test,
    # we won't have a distinct last_updated time per row.
    # As the test is now local, almost everything happens together, so we check
    # for a tiny number of distinct timestamps.
    distinct_last_updated = db._GetEngine().execute(
        'select count(distinct last_updated) from buildTable').fetchall()[0][0]
    self.assertTrue(distinct_last_updated > 3)

    ids_by_last_updated = db._GetEngine().execute(
        'select id from buildTable order by last_updated').fetchall()

    ids_by_last_updated = [id_tuple[0] for id_tuple in ids_by_last_updated]

    # Build #1 should have been last updated before build # 200.
    self.assertLess(ids_by_last_updated.index(1),
                    ids_by_last_updated.index(200))

    # However, build #1 (which was a master build) should have been last updated
    # AFTER build #2 which was its slave.
    self.assertGreater(ids_by_last_updated.index(1),
                       ids_by_last_updated.index(2))

  def _start_and_finish_time_checks(self, db):
    """Sanity checks that correct data was recorded, and can be retrieved."""
    max_start_time = db._GetEngine().execute(
        'select max(start_time) from buildTable').fetchall()[0][0]
    min_start_time = db._GetEngine().execute(
        'select min(start_time) from buildTable').fetchall()[0][0]
    max_fin_time = db._GetEngine().execute(
        'select max(finish_time) from buildTable').fetchall()[0][0]
    min_fin_time = db._GetEngine().execute(
        'select min(finish_time) from buildTable').fetchall()[0][0]
    self.assertGreater(max_start_time, min_start_time)
    self.assertGreater(max_fin_time, min_fin_time)

    # For all builds, finish_time should equal last_updated.
    mismatching_times = db._GetEngine().execute(
        'select count(*) from buildTable where finish_time != last_updated'
        ).fetchall()[0][0]
    self.assertEqual(mismatching_times, 0)


class BuildStagesAndFailureTest(CIDBIntegrationTest):
  """Test buildStageTable functionality."""

  def runTest(self):
    """Test basic buildStageTable and failureTable functionality."""
    self._PrepareDatabase()

    bot_db = self.LocalCIDBConnection(self.CIDB_USER_BOT)

    master_build_id = bot_db.InsertBuild('master build',
                                         _random(),
                                         'master_config',
                                         'master.hostname')

    build_id = bot_db.InsertBuild('builder name',
                                  _random(),
                                  'build_config',
                                  'bot_hostname',
                                  master_build_id=master_build_id,
                                  buildbucket_id=123)

    build_stage_id = bot_db.InsertBuildStage(build_id,
                                             'My Stage',
                                             board='bunny')
    bot_db.WaitBuildStage(build_stage_id)

    bot_db.StartBuildStage(build_stage_id)

    bot_db.FinishBuildStage(build_stage_id, constants.BUILDER_STATUS_PASSED)

    child_ids = [c['buildbucket_id']
                 for c in bot_db.GetSlaveStatuses(master_build_id)]

    slave_stages = bot_db.GetBuildsStagesWithBuildbucketIds(child_ids)
    self.assertEqual(len(slave_stages), 1)
    self.assertEqual(slave_stages[0]['status'], 'pass')
    self.assertEqual(slave_stages[0]['build_config'], 'build_config')
    self.assertEqual(slave_stages[0]['name'], 'My Stage')


class BuildTableTest(CIDBIntegrationTest):
  """Test buildTable functionality not tested by the DataSeries tests."""

  def testBuildbucketId(self):
    """Test InsertBuild with buildbucket_id."""
    self._PrepareDatabase()
    bot_db = self.LocalCIDBConnection(self.CIDB_USER_BOT)

    tmp_buildbucket_id = '120'
    build_id = bot_db.InsertBuild('build_name',
                                  _random(),
                                  'build_config',
                                  'bot_hostname',
                                  buildbucket_id=tmp_buildbucket_id)
    self.assertEqual(tmp_buildbucket_id,
                     bot_db.GetBuildStatus(build_id)['buildbucket_id'])

    build_status = bot_db.GetBuildStatusesWithBuildbucketIds(
        [tmp_buildbucket_id])
    if build_status:
      build_status = build_status[0]
    self.assertEqual(build_status['id'], build_id)

  def testFinishBuild(self):
    """Test FinishBuild."""
    self._PrepareDatabase()
    bot_db = self.LocalCIDBConnection(self.CIDB_USER_BOT)

    build_id = bot_db.InsertBuild('build_name',
                                  _random(),
                                  'build_config',
                                  'bot_hostname')

    r = bot_db.FinishBuild(build_id, status=constants.BUILDER_STATUS_ABORTED,
                           summary='summary')
    self.assertEqual(r, 1)
    self.assertEqual(bot_db.GetBuildStatus(build_id)['status'],
                     constants.BUILDER_STATUS_ABORTED)
    self.assertEqual(bot_db.GetBuildStatus(build_id)['build_config'],
                     'build_config')
    self.assertEqual(bot_db.GetBuildStatus(build_id)['summary'],
                     'summary')

    # Final status cannot be changed with strict=True
    r = bot_db.FinishBuild(build_id, status=constants.BUILDER_STATUS_PASSED,
                           summary='updated summary')
    self.assertEqual(r, 0)
    self.assertEqual(bot_db.GetBuildStatus(build_id)['status'],
                     constants.BUILDER_STATUS_ABORTED)
    self.assertEqual(bot_db.GetBuildStatus(build_id)['build_config'],
                     'build_config')
    self.assertEqual(bot_db.GetBuildStatus(build_id)['summary'],
                     'summary')

    # Final status can be changed with strict=False
    r = bot_db.FinishBuild(build_id, status=constants.BUILDER_STATUS_PASSED,
                           summary='updated summary', strict=False)
    self.assertEqual(r, 1)
    self.assertEqual(bot_db.GetBuildStatus(build_id)['status'],
                     constants.BUILDER_STATUS_PASSED)
    self.assertEqual(bot_db.GetBuildStatus(build_id)['build_config'],
                     'build_config')
    self.assertEqual(bot_db.GetBuildStatus(build_id)['summary'],
                     'updated summary')

  def _GetBuildToBuildbucketIdList(self, slave_statuses):
    """Convert slave_statuses to a list of (build_config, buildbucket_id)."""
    return [(x['build_config'], x['buildbucket_id']) for x in slave_statuses]

  def testGetSlaveStatus(self):
    """Test GetSlaveStatus."""
    self._PrepareDatabase()
    bot_db = self.LocalCIDBConnection(self.CIDB_USER_BOT)

    build_id = bot_db.InsertBuild('build_name',
                                  _random(),
                                  'build_config',
                                  'bot_hostname')

    bot_db.InsertBuild('build_1',
                       _random(),
                       'build_1',
                       'bot_hostname',
                       master_build_id=build_id,
                       buildbucket_id='id_1')
    bot_db.InsertBuild('build_2',
                       _random(),
                       'build_2',
                       'bot_hostname',
                       master_build_id=build_id,
                       buildbucket_id='id_2')
    bot_db.InsertBuild('build_1',
                       _random(),
                       'build_1',
                       'bot_hostname',
                       master_build_id=build_id,
                       buildbucket_id='id_3')

    build_bb_id_list = self._GetBuildToBuildbucketIdList(
        bot_db.GetSlaveStatuses(build_id))
    expected_list = ([
        ('build_1', 'id_1'),
        ('build_2', 'id_2'),
        ('build_1', 'id_3')
    ])
    self.assertListEqual(build_bb_id_list, expected_list)

    build_bb_id_list = self._GetBuildToBuildbucketIdList(
        bot_db.GetSlaveStatuses(build_id, buildbucket_ids=['id_2', 'id_3']))
    expected_list = ([
        ('build_2', 'id_2'),
        ('build_1', 'id_3')
    ])
    self.assertListEqual(build_bb_id_list, expected_list)

    build_bb_id_list = self._GetBuildToBuildbucketIdList(
        bot_db.GetSlaveStatuses(build_id, buildbucket_ids=[]))
    self.assertListEqual(build_bb_id_list, [])

  def _InsertBuildAndUpdateMetadata(self, bot_db, build_config, milestone,
                                    platform):
    build_id = bot_db.InsertBuild(build_config,
                                  _random(),
                                  build_config,
                                  'bot_hostname')
    metadata = metadata_lib.CBuildbotMetadata(metadata_dict={
        'version': {
            'milestone': milestone,
            'platform': platform
        }
    })
    return bot_db.UpdateMetadata(build_id, metadata)

  def testBuildHistory(self):
    """Test Get operations on build history."""
    self._PrepareDatabase()
    bot_db = self.LocalCIDBConnection(self.CIDB_USER_BOT)
    for _ in range(0, 3):
      bot_db.InsertBuild('build_1',
                         _random(),
                         'build_1',
                         'bot_hostname')
      bot_db.InsertBuild('build_2',
                         _random(),
                         'build_2',
                         'bot_hostname')
    result = bot_db.GetBuildsHistory(['build_1', 'build_2'], -1)
    self.assertEqual(len(result), 6)

    result = bot_db.GetBuildsHistory(['build_3'], -1)
    self.assertEqual(len(result), 0)

    result = bot_db.GetBuildHistory('build_1', -1)
    self.assertEqual(len(result), 3)

    result = bot_db.GetBuildHistory('build_3', -1)
    self.assertEqual(len(result), 0)


class DataSeries1Test(CIDBIntegrationTest):
  """Simulate a single set of canary builds."""

  def runTest(self):
    """Simulate a single set of canary builds with database schema v56."""
    metadatas = GetTestDataSeries(SERIES_1_TEST_DATA_PATH)
    self.assertEqual(len(metadatas), 18, 'Did not load expected amount of '
                                         'test data')

    # Migrate db to specified version. As new schema versions are added,
    # migrations to later version can be applied after the test builds are
    # simulated, to test that db contents are correctly migrated.
    self._PrepareFreshDatabase(65)

    bot_db = self.LocalCIDBConnection(self.CIDB_USER_BOT)

    def is_master(m):
      return m.GetValue('bot-config') == 'master-release'

    master_index = metadatas.index(next(m for m in metadatas if is_master(m)))
    master_metadata = metadatas.pop(master_index)
    self.assertEqual(master_metadata.GetValue('bot-config'), 'master-release')

    master_id = self._simulate_canary(bot_db, master_metadata)

    for m in metadatas:
      self._simulate_canary(bot_db, m, master_id)

    # Verify that expected data was inserted
    num_boards = bot_db._GetEngine().execute(
        'select count(*) from boardPerBuildTable'
        ).fetchall()[0][0]
    self.assertEqual(num_boards, 40)

    main_firmware_versions = bot_db._GetEngine().execute(
        'select count(distinct main_firmware_version) from boardPerBuildTable'
        ).fetchall()[0][0]
    self.assertEqual(main_firmware_versions, 29)

    # For all builds, finish_time should equal last_updated.
    mismatching_times = bot_db._GetEngine().execute(
        'select count(*) from buildTable where finish_time != last_updated'
        ).fetchall()[0][0]
    self.assertEqual(mismatching_times, 0)

  def _simulate_canary(self, db, metadata, master_build_id=None):
    """Helper method to simulate an individual canary build.

    Args:
      db: cidb instance to use for simulation
      metadata: CBuildbotMetadata instance of build to simulate.
      master_build_id: Optional id of master build.

    Returns:
      build_id of build that was simulated.
    """
    build_id = _SimulateBuildStart(db, metadata, master_build_id)
    metadata_dict = metadata.GetDict()

    # Insert child configs and boards
    for child_config_dict in metadata_dict['child-configs']:
      db.InsertChildConfigPerBuild(build_id, child_config_dict['name'])

    for board in metadata_dict['board-metadata'].keys():
      db.InsertBoardPerBuild(build_id, board)

    for board, bm in metadata_dict['board-metadata'].items():
      db.UpdateBoardPerBuildMetadata(build_id, board, bm)

    db.UpdateMetadata(build_id, metadata)

    status = metadata_dict['status']['status']

    for child_config_dict in metadata_dict['child-configs']:
      # Note, we are not using test data here, because the test data
      # we have predates the existence of child-config status being
      # stored in metadata.json. Instead, we just pretend all child
      # configs had the same status as the main config.
      db.FinishChildConfig(build_id, child_config_dict['name'],
                           status)

    db.FinishBuild(build_id, status)

    return build_id


def _SimulateBuildStart(db, metadata, master_build_id=None, important=None):
  """Returns build_id for the inserted buildTable entry."""
  metadata_dict = metadata.GetDict()
  build_id = db.InsertBuild(metadata_dict['builder-name'],
                            metadata_dict['build-number'],
                            metadata_dict['bot-config'],
                            metadata_dict['bot-hostname'],
                            master_build_id,
                            important=important)

  return build_id


def main(_argv):
  # TODO(akeshet): Allow command line args to specify alternate CIDB instance
  # for testing.
  cros_test_lib.main(module=__name__)
