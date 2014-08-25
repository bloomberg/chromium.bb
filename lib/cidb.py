# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Continuous Integration Database Library."""

import datetime
import glob
import logging
import os
import re
try:
  import sqlalchemy
  import sqlalchemy.exc
  import sqlalchemy.interfaces
  from sqlalchemy import MetaData
except ImportError:
  raise AssertionError(
      'Unable to import sqlalchemy. Please install this package by running '
      '`sudo apt-get install python-sqlalchemy` or similar.')

import time

from chromite.cbuildbot import constants

CIDB_MIGRATIONS_DIR = os.path.join(constants.CHROMITE_DIR, 'cidb',
                                   'migrations')

class DBException(Exception):
  """General exception class for this module."""


class UnsupportedMethodException(DBException):
  """Raised when a call is made that the database does not support."""


def minimum_schema(min_version):
  """Generate a decorator to specify a minimum schema version for a method.

  This decorator should be applied only to instance methods of
  SchemaVersionedMySQLConnection objects.
  """

  def decorator(f):
    def wrapper(self, *args, **kwargs):
      if self.schema_version < min_version:
        raise UnsupportedMethodException()
      return f(self, *args, **kwargs)
    return wrapper
  return decorator


class StrictModeListener(sqlalchemy.interfaces.PoolListener):
  """This listener ensures that STRICT_ALL_TABLES for all connections."""
  # pylint: disable-msg=W0613
  def connect(self, dbapi_con, *args, **kwargs):
    cur = dbapi_con.cursor()
    cur.execute("SET SESSION sql_mode='STRICT_ALL_TABLES'")
    cur.close()


class SchemaVersionedMySQLConnection(object):
  """Connection to a database that is aware of its schema version."""

  SCHEMA_VERSION_TABLE_NAME = 'schemaVersionTable'
  SCHEMA_VERSION_COL = 'schemaVersion'

  def __init__(self, db_name, db_migrations_dir, db_credentials_dir):
    """SchemaVersionedMySQLConnection constructor.

    Args:
      db_name: Name of the database to connect to.
      db_migrations_dir: Absolute path to directory of migration scripts
                         for this database.
      db_credentials_dir: Absolute path to directory containing connection
                          information to the database. Specifically, this
                          directory should contain files names user.txt,
                          password.txt, host.txt, client-cert.pem,
                          client-key.pem, and server-ca.pem
    """
    # None, or a sqlalchemy.MetaData instance
    self._meta = None

    # pid of process on which _engine was created
    self._engine_pid = None

    self._engine = None

    self.db_migrations_dir = db_migrations_dir
    self.db_credentials_dir = db_credentials_dir
    self.db_name = db_name

    with open(os.path.join(db_credentials_dir, 'password.txt')) as f:
      password = f.read().strip()
    with open(os.path.join(db_credentials_dir, 'host.txt')) as f:
      host = f.read().strip()
    with open(os.path.join(db_credentials_dir, 'user.txt')) as f:
      user = f.read().strip()

    cert = os.path.join(db_credentials_dir, 'client-cert.pem')
    key = os.path.join(db_credentials_dir, 'client-key.pem')
    ca = os.path.join(db_credentials_dir, 'server-ca.pem')
    self._ssl_args = {'ssl': {'cert': cert, 'key': key, 'ca': ca}}

    connect_url = sqlalchemy.engine.url.URL('mysql', username=user,
                                            password=password,
                                            host=host)

    # Create a temporary engine to connect to the mysql instance, and check if
    # a database named |db_name| exists. If not, create one. We use a temporary
    # engine here because the real engine will be opened with a default
    # database name given by |db_name|.
    temp_engine = sqlalchemy.create_engine(connect_url,
                                           connect_args=self._ssl_args,
                                           listeners=[StrictModeListener()])
    databases = temp_engine.execute('SHOW DATABASES').fetchall()
    if (db_name,) not in databases:
      temp_engine.execute('CREATE DATABASE %s' % db_name)
      logging.info('Created database %s', db_name)

    temp_engine.dispose()

    # Now create the persistent connection to the database named |db_name|.
    # If there is a schema version table, read the current schema version
    # from it. Otherwise, assume schema_version 0.
    self._connect_url = sqlalchemy.engine.url.URL('mysql', username=user,
                                                  password=password,
                                                  host=host, database=db_name)

    self.schema_version = self.QuerySchemaVersion()

    logging.debug('Created a SchemaVersionedMySQLConnection, '
                  'sqlalchemy version %s', sqlalchemy.__version__)

  def DropDatabase(self):
    """Delete all data and tables from database, and drop database.

    Use with caution. All data in database will be deleted. Invalidates
    this database connection instance.
    """
    self._meta = None
    self._GetEngine().execute('DROP DATABASE %s' % self.db_name)
    self._InvalidateEngine()

  def QuerySchemaVersion(self):
    """Query the database for its current schema version number.

    Returns:
      The current schema version from the database's schema version table,
      as an integer, or 0 if the table is empty or nonexistent.
    """
    tables = self._GetEngine().execute('SHOW TABLES').fetchall()
    if (self.SCHEMA_VERSION_TABLE_NAME,) in tables:
      r = self._GetEngine().execute('SELECT MAX(%s) from %s' %
          (self.SCHEMA_VERSION_COL, self.SCHEMA_VERSION_TABLE_NAME))
      return r.fetchone()[0] or 0
    else:
      return 0

  def _GetMigrationScripts(self):
    """Look for migration scripts and return their versions and paths."

    Returns:
      A list of (schema_version, script_path) tuples of the migration
      scripts for this database, sorted in ascending schema_version order.
    """
    # Look for migration script files in the migration script directory,
    # with names of the form [number]*.sql, and sort these by number.
    migration_scripts = glob.glob(os.path.join(self.db_migrations_dir, '*.sql'))
    migrations = []
    for script in migration_scripts:
      match = re.match(r'([0-9]*).*', os.path.basename(script))
      if match:
        migrations.append((int(match.group(1)), script))

    migrations.sort()
    return migrations

  def ApplySchemaMigrations(self, maxVersion=None):
    """Apply pending migration scripts to database, in order.

    Args:
      maxVersion: The highest version migration script to apply. If
                  unspecified, all migrations found will be applied.
    """
    migrations = self._GetMigrationScripts()

    # Execute the migration scripts in order, asserting that each one
    # updates the schema version to the expected number. If maxVersion
    # is specified stop early.
    for (number, script) in migrations:
      if maxVersion is not None and number > maxVersion:
        break

      if number > self.schema_version:
        # Invalidate self._meta, then run script and ensure that schema
        # version was increased.
        self._meta = None
        logging.info('Running migration script %s', script)
        self.RunQueryScript(script)
        self.schema_version = self.QuerySchemaVersion()
        if self.schema_version != number:
          raise DBException('Migration script %s did not update '
                            'schema version to %s as expected. ' % (number,
                                                                    script))

  def RunQueryScript(self, script_path):
    """Run a .sql script file located at |script_path| on the database."""
    with open(script_path, 'r') as f:
      script = f.read()
    queries = [q.strip() for q in script.split(';') if q.strip()]
    for q in queries:
      self._GetEngine().execute(q)

  def _ReflectToMetadata(self):
    """Use sqlalchemy reflection to construct MetaData model of database.

    If self._meta is already populated, this does nothing.
    """
    if self._meta is not None:
      return
    self._meta = MetaData()
    self._meta.reflect(bind=self._GetEngine())

  def _Insert(self, table, values):
    """Create and execute an INSERT query.

    Args:
      table: Table name to insert to.
      values: Dictionary of column values to insert. Or, list of
              value dictionaries to insert multiple rows.

    Returns:
      Integer primary key of the last inserted row.
    """
    self._ReflectToMetadata()
    ins = self._meta.tables[table].insert()
    r = self._Execute(ins, values)
    return r.inserted_primary_key[0]

  def _InsertMany(self, table, values):
    """Create and execute an multi-row INSERT query.

    Args:
      table: Table name to insert to.
      values: A list of value dictionaries to insert multiple rows.

    Returns:
      The number of inserted rows.
    """
    self._ReflectToMetadata()
    ins = self._meta.tables[table].insert()
    r = self._Execute(ins, values)
    return r.rowcount

  def _GetPrimaryKey(self, table):
    """Gets the primary key column of |table|.

    This function requires that the given table have a 1-column promary key.

    Args:
      table: Name of table to primary key for.

    Returns:
      A sqlalchemy.sql.schema.Column representing the primary key column.

    Raises:
      DBException if the table does not have a single column primary key.
   """
    self._ReflectToMetadata()
    t = self._meta.tables[table]

    # TODO(akeshet): between sqlalchemy 0.7 and 0.8, a breaking change was
    # made to how t.columns and t.primary_key are stored, and in sqlalchemy
    # 0.7 t.columns does not have a .values() method. Hence this clumsy way
    # of extracting the primary key column. Currently, our builders have 0.7
    # installed. Once we drop support for 0.7, this code can be simply replaced
    # by:
    # key_columns = t.primary_key.columns.values()
    col_names = t.columns.keys()
    cols = [t.columns[n] for n in col_names]
    key_columns = [c for c in cols if c.primary_key]

    if len(key_columns) != 1:
      raise DBException('Table %s does not have a 1-column primary '
                        'key.' % table)
    return key_columns[0]

  def _Update(self, table, row_id, values):
    """Create and execute an UPDATE query by primary key.

    Args:
      table: Table name to update.
      row_id: Primary key value of row to update.
      values: Dictionary of column values to update.

    Returns:
      The number of rows that were updated (0 or 1).
    """
    self._ReflectToMetadata()
    primary_key = self._GetPrimaryKey(table)
    upd = self._meta.tables[table].update().where(primary_key==row_id)
    r = self._Execute(upd, values)
    return r.rowcount

  def _UpdateWhere(self, table, where, values):
    """Create and execute an update query with a custom where clause.

    Args:
      table: Table name to update.
      where: Raw SQL for the where clause, in string form, e.g.
             'build_id = 1 and board = "tomato"'
      values: dictionary of column values to update.

    Returns:
      The number of rows that were updated.
    """
    self._ReflectToMetadata()
    upd = self._meta.tables[table].update().where(where)
    r = self._Execute(upd, values)
    return r.rowcount

  def _Execute(self, query, *args, **kwargs):
    """Execute a query using engine, with retires.

    This method wraps execution of a query in a single retry in case the
    engine's connection has been dropped.

    Args:
      query: Query to execute, of type string, or sqlalchemy.Executible,
             or other sqlalchemy-executible statement (see sqlalchemy
             docs).
      *args: Additional args passed along to .execute(...)
      **kwargs: Additional args passed along to .execute(...)

    Returns:
      The result of .execute(...)
    """
    try:
      return self._GetEngine().execute(query, *args, **kwargs)
    except sqlalchemy.exc.OperationalError as e:
      error_code = e.orig.args[0]
      # Error coded 2006 'MySQL server has gone away' indicates that the
      # connection used was closed or dropped.
      if error_code == 2006:
        e = self._GetEngine()
        logging.debug('Retrying a query on engine %s@%s for pid %s due to '
                      'dropped connection.', e.url.username, e.url.host,
                      self._engine_pid)
        return self._GetEngine().execute(query, *args, **kwargs)
      else:
        raise

  def _GetEngine(self):
    """Get the sqlalchemy engine for this process.

    This method creates a new sqlalchemy engine if necessary, and
    returns an engine that is unique to this process.

    Returns:
      An sqlalchemy.engine instance for this database.
    """
    pid = os.getpid()
    if pid == self._engine_pid and self._engine:
      return self._engine
    else:
      e = sqlalchemy.create_engine(self._connect_url,
                                   connect_args=self._ssl_args,
                                   listeners=[StrictModeListener()])
      self._engine = e
      self._engine_pid = pid
      logging.debug('Created cidb engine %s@%s for pid %s', e.url.username,
                    e.url.host, pid)
      return self._engine

  def _InvalidateEngine(self):
    """Dispose of an sqlalchemy engine."""
    try:
      pid = os.getpid()
      if pid == self._engine_pid and self._engine:
        self._engine.dispose()
    finally:
      self._engine = None
      self._meta = None


class CIDBConnection(SchemaVersionedMySQLConnection):
  """Connection to a Continuous Integration database."""
  def __init__(self, db_credentials_dir):
    super(CIDBConnection, self).__init__('cidb', CIDB_MIGRATIONS_DIR,
                                         db_credentials_dir)

  @minimum_schema(2)
  def InsertBuild(self, builder_name, waterfall, build_number,
                  build_config, bot_hostname, start_time=None,
                  master_build_id=None):
    """Insert a build row.

    Args:
      builder_name: buildbot builder name.
      waterfall: buildbot waterfall name.
      build_number: buildbot build number.
      build_config: cbuildbot config of build
      bot_hostname: hostname of bot running the build
      start_time: (Optional) Unix timestamp of build start time. If None,
                  current time will be used.
      master_build_id: (Optional) primary key of master build to this build.
    """
    start_time = start_time or time.time()
    dt = datetime.datetime.fromtimestamp(start_time)

    return self._Insert('buildTable', {'builder_name': builder_name,
                                       'buildbot_generation':
                                         constants.BUILDBOT_GENERATION,
                                       'waterfall': waterfall,
                                       'build_number': build_number,
                                       'build_config' : build_config,
                                       'bot_hostname': bot_hostname,
                                       'start_time' : dt,
                                       'master_build_id' : master_build_id}
                        )

  @minimum_schema(3)
  def InsertCLActions(self, build_id, cl_actions):
    """Insert a list of |cl_actions|.

    If |cl_actions| is empty, this function does nothing.

    Args:
      build_id: primary key of build that performed these actions.
      cl_actions: A list of cl_action tuples.

    Returns:
      Number of actions inserted.
    """
    if not cl_actions:
      return 0

    values = []
    # TODO(akeshet): Refactor to use either cl action tuples out of the
    # metadata dict (as now) OR CLActionTuple objects.
    for cl_action in cl_actions:
      change_source = 'internal' if cl_action[0]['internal'] else 'external'
      change_number = cl_action[0]['gerrit_number']
      patch_number = cl_action[0]['patch_number']
      action = cl_action[1]
      timestamp = cl_action[2]
      reason = cl_action[3]
      values.append({
          'build_id' : build_id,
          'change_source' : change_source,
          'change_number': change_number,
          'patch_number' : patch_number,
          'action' : action,
          'timestamp' : datetime.datetime.fromtimestamp(timestamp),
          'reason' : reason})

    return self._InsertMany('clActionTable', values)

  @minimum_schema(4)
  def InsertBuildStage(self, build_id, stage_name, board, status,
                       log_url, duration_seconds, summary):
    """Insert a build stage into buildStageTable.

    Args:
      build_id: id of responsible build
      stage_name: name of stage
      board: board that stage ran for
      status: 'pass' or 'fail'
      log_url: URL of stage log
      duration_seconds: run time of stage, in seconds
      summary: summary message of stage

    Returns:
      Primary key of inserted stage.
    """
    return self._Insert('buildStageTable',
                        {'build_id': build_id,
                         'name': stage_name,
                         'board': board,
                         'status': status,
                         'log_url': log_url,
                         'duration_seconds': duration_seconds,
                         'summary': summary})

  @minimum_schema(4)
  def InsertBuildStages(self, stages):
    """For testing only. Insert multiple build stages into buildStageTable.

    This method allows integration tests to more quickly populate build
    stages into the database, from test data. Normal builder operations are
    expected to insert build stage rows one at a time, using InsertBuildStage.

    Args:
      stages: A list of dictionaries, each dictionary containing keys
              build_id, name, board, status, log_url, duration_seconds, and
              summary.

    Returns:
      The number of build stage rows inserted.
    """
    if not stages:
      return 0
    return self._InsertMany('buildStageTable',
                            stages)

  @minimum_schema(6)
  def InsertBoardPerBuild(self, build_id, board):
    """Inserts a board-per-build entry into database.

    Args:
      build_id: primary key of the build in the buildTable
      board: String board name.
    """
    self._Insert('boardPerBuildTable', {'build_id': build_id,
                                        'board': board})

  @minimum_schema(7)
  def InsertChildConfigPerBuild(self, build_id, child_config):
    """Insert a child-config-per-build entry into database.

    Args:
      build_id: primary key of the build in the buildTable
      child_config: String child_config name.
    """
    self._Insert('childConfigPerBuildTable', {'build_id': build_id,
                                              'child_config': child_config})

  @minimum_schema(2)
  def UpdateMetadata(self, build_id, metadata):
    """Update the given metadata row in database.

    Args:
      build_id: id of row to update.
      metadata: CBuildbotMetadata instance to update with.

    Returns:
      The number of build rows that were updated (0 or 1).
    """
    d = metadata.GetDict()
    versions = d.get('version') or {}
    return self._Update('buildTable', build_id,
                        {'chrome_version': versions.get('chrome'),
                         'milestone_version': versions.get('milestone'),
                         'platform_version': versions.get('platform'),
                         'full_version': versions.get('full'),
                         'sdk_version': d.get('sdk-versions'),
                         'toolchain_url': d.get('toolchain-url'),
                         'build_type': d.get('build_type'),
                         'metadata_json': metadata.GetJSON()})

  @minimum_schema(6)
  def UpdateBoardPerBuildMetadata(self, build_id, board, board_metadata):
    """Update the given board-per-build metadata.

    Args:
      build_id: id of the build
      board: board to update
      board_metadata: per-board metadata dict for this board
    """
    update_dict = {
        'main_firmware_version': board_metadata.get('main-firmware-version'),
        'ec_firmware_version': board_metadata.get('ec-firmware-version')
        }
    return self._UpdateWhere('boardPerBuildTable',
        'build_id = %s and board = "%s"' % (build_id, board),
        update_dict)


  @minimum_schema(2)
  def FinishBuild(self, build_id, finish_time=None, status=None,
                  status_pickle=None):
    """Update the given build row, marking it as finished.

    This should be called once per build, as the last update to the build.
    This will also mark the row's final=True.

    Args:
      build_id: id of row to update.
      finish_time: Unix timestamp of build finish time. If None, current time
                   will be used.
      status: Final build status, one of
              manifest_version.BuilderStatus.COMPLETED_STATUSES.
      status_pickle: Pickled manifest_version.BuilderStatus.
    """
    self._ReflectToMetadata()
    finish_time = finish_time or time.time()
    dt = datetime.datetime.fromtimestamp(finish_time)

    # TODO(akeshet) atomically update the final field of metadata to
    # True
    self._Update('buildTable', build_id, {'finish_time' : dt,
                                          'status' : status,
                                          'status_pickle' : status_pickle,
                                          'final' : True})


class CIDBConnectionFactory(object):
  """Factory class used by builders to fetch the appropriate cidb connection"""

  # A call to one of the Setup methods below is necessary before using the
  # GetCIDBConnectionForBuilder Factory. This ensures that unit tests do not
  # accidentally use one of the live database instances.

  _ConnectionIsSetup = False
  _ConnectionType = None
  _ConnectionCredsPath = None
  _MockCIDB = None
  _CachedCIDB = None

  _CONNECTION_TYPE_PROD = 'prod'   # production database
  _CONNECTION_TYPE_DEBUG = 'debug' # debug database, used by --debug builds
  _CONNECTION_TYPE_MOCK = 'mock'   # mock connection, not backed by database
  _CONNECTION_TYPE_NONE = 'none'   # explicitly no connection


  @classmethod
  def IsCIDBSetup(cls):
    """Returns True iff GetCIDBConnectionForBuilder is ready to be called."""
    return cls._ConnectionIsSetup


  @classmethod
  def SetupProdCidb(cls):
    """Sets up CIDB to use the prod instance of the database.

    May be called only once, and may not be called after any other CIDB Setup
    method, otherwise it will raise an AssertionError.
    """
    assert not cls._ConnectionIsSetup, 'CIDB is already set up.'
    assert not cls._ConnectionType == cls._CONNECTION_TYPE_MOCK, (
        'CIDB set for mock use.')
    cls._ConnectionType = cls._CONNECTION_TYPE_PROD
    cls._ConnectionCredsPath = constants.CIDB_PROD_BOT_CREDS
    cls._ConnectionIsSetup = True


  @classmethod
  def SetupDebugCidb(cls):
    """Sets up CIDB to use the debug instance of the database.

    May be called only once, and may not be called after any other CIDB Setup
    method, otherwise it will raise an AssertionError.
    """
    assert not cls._ConnectionIsSetup, 'CIDB is already set up.'
    assert not cls._ConnectionType == cls._CONNECTION_TYPE_MOCK, (
        'CIDB set for mock use.')
    cls._ConnectionType = cls._CONNECTION_TYPE_DEBUG
    cls._ConnectionCredsPath = constants.CIDB_DEBUG_BOT_CREDS
    cls._ConnectionIsSetup = True


  @classmethod
  def SetupMockCidb(cls, mock_cidb=None):
    """Sets up CIDB to use a mock object. May be called more than once.

    Args:
      mock_cidb: (optional) The mock cidb object to be returned by
                 GetCIDBConnection. If not supplied, then CIDB will be
                 considered not set up, but future calls to set up a
                 non-(mock or None) connection will fail.
    """
    if cls._ConnectionIsSetup:
      assert cls._ConnectionType == cls._CONNECTION_TYPE_MOCK, (
          'A non-mock CIDB is already set up.')
    cls._ConnectionType = cls._CONNECTION_TYPE_MOCK
    if mock_cidb:
      cls._ConnectionIsSetup = True
      cls._MockCIDB = mock_cidb


  @classmethod
  def SetupNoCidb(cls):
    """Sets up CIDB to use an explicit None connection.

    May be called more than once, or after SetupMockCidb.
    """
    if cls._ConnectionIsSetup:
      assert (cls._ConnectionType == cls._CONNECTION_TYPE_MOCK or
              cls._ConnectionType == cls._CONNECTION_TYPE_NONE) , (
          'A non-mock CIDB is already set up.')
    cls._ConnectionType = cls._CONNECTION_TYPE_NONE
    cls._ConnectionIsSetup = True


  @classmethod
  def GetCIDBConnectionForBuilder(cls):
    """Get a CIDBConnection.

    A call to one of the CIDB Setup methods must have been made before calling
    this factory method.

    Returns:
      A CIDBConnection instance connected to either the prod or debug
      instance of the database, or a mock connection, depending on which
      Setup method was called. Returns None if CIDB has been explicitly
      set up for that using SetupNoCidb.
    """
    assert cls._ConnectionIsSetup, 'CIDB has not be set up with a Setup call.'
    if cls._ConnectionType == cls._CONNECTION_TYPE_MOCK:
      return cls._MockCIDB
    elif cls._ConnectionType == cls._CONNECTION_TYPE_NONE:
      return None
    else:
      if cls._CachedCIDB:
        return cls._CachedCIDB
      cls._CachedCIDB = CIDBConnection(cls._ConnectionCredsPath)
      return cls._CachedCIDB

  @classmethod
  def _ClearCIDBSetup(cls):
    """Clears the CIDB Setup state. For testing purposes only."""
    cls._ConnectionIsSetup = False
    cls._ConnectionType = None
    cls._ConnectionCredsPath = None
    cls._MockCIDB = None
    cls._CachedCIDB = None


