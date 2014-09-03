#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration tests for cidb.py module.

Running these tests requires and assumes:
  1) You are running from a machine with whitelisted access to the CIDB
database test instance.
  2) You have a checkout of the crostools repo, which provides credentials
to the above test instance.
"""

# pylint: disable-msg= W0212

import glob
import logging
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.cbuildbot import constants
from chromite.cbuildbot import metadata_lib
from chromite.lib import cidb
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel

SERIES_0_TEST_DATA_PATH = os.path.join(
    constants.CHROMITE_DIR, 'cidb', 'test_data', 'series_0')

SERIES_1_TEST_DATA_PATH = os.path.join(
    constants.CHROMITE_DIR, 'cidb', 'test_data', 'series_1')

TEST_DB_CRED_ROOT = os.path.join(constants.SOURCE_ROOT,
                                 'crostools', 'cidb',
                                 'cidb_test_root')

TEST_DB_CRED_READONLY = os.path.join(constants.SOURCE_ROOT,
                                     'crostools', 'cidb',
                                     'cidb_test_readonly')

TEST_DB_CRED_BOT = os.path.join(constants.SOURCE_ROOT,
                                'crostools', 'cidb',
                                'cidb_test_bot')


class CIDBIntegrationTest(cros_test_lib.TestCase):
  """Base class for cidb tests that connect to a test MySQL instance."""

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
    db = cidb.CIDBConnection(TEST_DB_CRED_ROOT)
    db.DropDatabase()

    # Connect to now fresh database and apply migrations.
    db = cidb.CIDBConnection(TEST_DB_CRED_ROOT)
    db.ApplySchemaMigrations(max_schema_version)

    return db

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

    for i in range(1, max_version+1):
      db.ApplySchemaMigrations(i)

  def testActions(self):
    """Test that InsertCLActions accepts 0-, 1-, and multi-item lists."""
    db = self._PrepareFreshDatabase()
    build_id = db.InsertBuild('my builder', 'chromiumos', 12, 'my config',
                              'my bot hostname')

    a1 = metadata_lib.GetCLActionTuple(
        metadata_lib.GerritPatchTuple(1, 1, True),
        constants.CL_ACTION_PICKED_UP)
    a2 = metadata_lib.GetCLActionTuple(
        metadata_lib.GerritPatchTuple(1, 1, True),
        constants.CL_ACTION_PICKED_UP)
    a3 = metadata_lib.GetCLActionTuple(
        metadata_lib.GerritPatchTuple(1, 1, True),
        constants.CL_ACTION_PICKED_UP)

    db.InsertCLActions(build_id, [])
    db.InsertCLActions(build_id, [a1])
    db.InsertCLActions(build_id, [a2, a3])

    action_count = db._GetEngine().execute('select count(*) from clActionTable'
                                           ).fetchall()[0][0]
    self.assertEqual(action_count, 3)

    # Test that all known CL action types can be inserted
    fakepatch = metadata_lib.GerritPatchTuple(1, 1, True)
    all_actions_list = [metadata_lib.GetCLActionTuple(fakepatch, action)
                        for action in constants.CL_ACTIONS]
    db.InsertCLActions(build_id, all_actions_list)

class CIDBAPITest(CIDBIntegrationTest):
  """Tests of the CIDB API."""

  def testSchemaVersionTooLow(self):
    """Tests that the minimum_schema decorator works as expected."""
    db = self._PrepareFreshDatabase(3)
    self.assertRaises2(cidb.UnsupportedMethodException,
                       db.InsertBuildStages, [])

  def testSchemaVersionOK(self):
    """Tests that the minimum_schema decorator works as expected."""
    db = self._PrepareFreshDatabase(4)
    db.InsertBuildStages([])


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

  def testCQWithSchema11(self):
    """Run the CQ test with schema version 13."""
    # Run the CQ test at schema version 13
    self._PrepareFreshDatabase(13)
    self._runCQTest()

  def _runCQTest(self):
    """Simulate a set of 630 master/slave CQ builds.

    Note: This test takes about 2.5 minutes to populate its 630 builds
    and their corresponding cl actions into the test database.
    """
    metadatas = GetTestDataSeries(SERIES_0_TEST_DATA_PATH)
    self.assertEqual(len(metadatas), 630, 'Did not load expected amount of '
                                          'test data')

    bot_db = cidb.CIDBConnection(TEST_DB_CRED_BOT)

    # Simulate the test builds, using a database connection as the
    # bot user.
    self.simulate_builds(bot_db, metadatas)

    # Perform some sanity check queries against the database, connected
    # as the readonly user.
    readonly_db = cidb.CIDBConnection(TEST_DB_CRED_READONLY)

    self._start_and_finish_time_checks(readonly_db)

    build_types = readonly_db._GetEngine().execute(
        'select build_type from buildTable').fetchall()
    self.assertTrue(all(x == ('paladin',) for x in build_types))

    self._cl_action_checks(readonly_db)

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

    self._start_and_finish_time_checks(readonly_db)
    self._cl_action_checks(readonly_db)
    self._last_updated_time_checks(readonly_db)

  def _last_updated_time_checks(self, db):
    """Sanity checks on the last_updated column."""
    # We should have a diversity of last_updated times. Since the timestamp
    # resolution is only 1 second, and we have lots of parallelism in the test,
    # we won't have a distring last_updated time per row. But we will have at
    # least 100 distinct last_updated times.
    distinct_last_updated = db._GetEngine().execute(
        'select count(distinct last_updated) from buildTable').fetchall()[0][0]
    self.assertTrue(distinct_last_updated > 80)

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

  def _cl_action_checks(self, db):
    """Sanity checks that correct cl actions were recorded."""
    submitted_cl_count = db._GetEngine().execute(
        'select count(*) from clActionTable where action="submitted"'
        ).fetchall()[0][0]
    rejected_cl_count = db._GetEngine().execute(
        'select count(*) from clActionTable where action="kicked_out"'
        ).fetchall()[0][0]
    total_actions = db._GetEngine().execute(
        'select count(*) from clActionTable').fetchall()[0][0]
    self.assertEqual(submitted_cl_count, 56)
    self.assertEqual(rejected_cl_count, 8)
    self.assertEqual(total_actions, 1877)

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


  def simulate_builds(self, db, metadatas):
    """Simulate a serires of Commit Queue master and slave builds.

    This method use the metadata objects in |metadatas| to simulate those
    builds insertions and updates to the cidb. All metadatas encountered
    after a particular master build will be assumed to be slaves of that build,
    until a new master build is encountered. Slave builds for a particular
    master will be simulated in parallel.

    The first element in |metadatas| must be a CQ master build.

    Args:
      db: A CIDBConnection instance.
      metadatas: A list of CBuildbotMetadata instances, sorted by start time.
    """
    m_iter = iter(metadatas)

    def is_master(m):
      return m.GetDict()['bot-config'] == 'master-paladin'

    next_master = m_iter.next()

    while next_master:
      master = next_master
      next_master = None
      assert is_master(master)
      master_build_id = _SimulateBuildStart(db, master)

      def simulate_slave(slave_metadata):
        build_id = _SimulateBuildStart(db, slave_metadata,
                                        master_build_id)
        _SimulateCQBuildFinish(db, slave_metadata, build_id)
        logging.debug('Simulated slave build %s on pid %s', build_id,
                      os.getpid())
        return build_id

      slave_metadatas = []
      for slave in m_iter:
        if is_master(slave):
          next_master = slave
          break
        slave_metadatas.append(slave)

      with parallel.BackgroundTaskRunner(simulate_slave, processes=15) as queue:
        for slave in slave_metadatas:
          queue.put([slave])

      _SimulateCQBuildFinish(db, master, master_build_id)
      logging.debug('Simulated master build %s', master_build_id)


class DataSeries1Test(CIDBIntegrationTest):
  """Simulate a single set of canary builds."""

  def runTest(self):
    """Simulate a single set of canary builds with database schema v7."""
    metadatas = GetTestDataSeries(SERIES_1_TEST_DATA_PATH)
    self.assertEqual(len(metadatas), 18, 'Did not load expected amount of '
                                         'test data')

    # Migrate db to specified version. As new schema versions are added,
    # migrations to later version can be applied after the test builds are
    # simulated, to test that db contents are correctly migrated.
    self._PrepareFreshDatabase(11)

    bot_db = cidb.CIDBConnection(TEST_DB_CRED_BOT)

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
    status = _TranslateStatus(status)
    db.FinishBuild(build_id, status)

    return build_id


def _TranslateStatus(status):
  # TODO(akeshet): The status strings used in BuildStatus are not the same as
  # those recorded in CBuildbotMetadata. Use a general purpose adapter.
  if status == 'passed':
    return 'pass'

  if status == 'failed':
    return 'fail'

  return status


def _SimulateBuildStart(db, metadata, master_build_id=None):
  """Returns (build_id, metadata_id) tuple."""
  metadata_dict = metadata.GetDict()
  # TODO(akeshet): We are pretending that all these builds were on the internal
  # waterfall at the moment, for testing purposes. This is because we don't
  # actually save in the metadata.json any way to know which waterfall the
  # build was on.
  waterfall = 'chromeos'

  build_id = db.InsertBuild(metadata_dict['builder-name'],
                            waterfall,
                            metadata_dict['build-number'],
                            metadata_dict['bot-config'],
                            metadata_dict['bot-hostname'],
                            master_build_id)

  return build_id


def _SimulateCQBuildFinish(db, metadata, build_id):

  metadata_dict = metadata.GetDict()

  # Insert the first build stage using InsertBuildStage, then batch-insert
  # the rest with InsertBuildStages. This allows us to test InsertBuildStage
  # without taking too much performance loss in the test.
  stage_results = metadata_dict['results']
  if len(stage_results) > 0:
    r = stage_results[0]
    db.InsertBuildStage(build_id, r['name'], r['board'],
                        _TranslateStatus(r['status']), r['log'],
                        cros_build_lib.ParseDurationToSeconds(r['duration']),
                        r['summary'])
  if len(stage_results) > 1:
    stages = [{'build_id': build_id,
               'name': r['name'],
               'board': r['board'],
               'status': _TranslateStatus(r['status']),
               'log_url': r['log'],
               'duration_seconds':
                 cros_build_lib.ParseDurationToSeconds(r['duration']),
               'summary': r['summary']}
              for r in stage_results[1:]]
    db.InsertBuildStages(stages)

  db.InsertCLActions(build_id, metadata_dict['cl_actions'])

  db.UpdateMetadata(build_id, metadata)

  status = metadata_dict['status']['status']

  status = _TranslateStatus(status)

  db.FinishBuild(build_id, status)


# TODO(akeshet): Allow command line args to specify alternate CIDB instance
# for testing.
if __name__ == '__main__':
  logging.root.setLevel(logging.DEBUG)
  logging.root.addHandler(logging.StreamHandler())
  cros_test_lib.main()
