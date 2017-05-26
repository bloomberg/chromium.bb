# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Continuous Integration Database Library."""

from __future__ import print_function

import collections
import datetime
import glob
import os
import re


from chromite.lib import constants
from chromite.lib import clactions
from chromite.lib import cros_logging as logging
from chromite.lib import factory
from chromite.lib import failure_message_lib
from chromite.lib import graphite
from chromite.lib import hwtest_results
from chromite.lib import metrics
from chromite.lib import osutils
from chromite.lib import retry_stats


sqlalchemy_imported = False
try:
  import sqlalchemy
  import sqlalchemy.exc
  import sqlalchemy.interfaces
  from sqlalchemy import MetaData
  sqlalchemy_imported = True
except ImportError:
  pass

CIDB_MIGRATIONS_DIR = os.path.join(constants.CHROMITE_DIR, 'cidb',
                                   'migrations')

_RETRYABLE_OPERATIONAL_ERROR_CODES = frozenset([
    1053,   # 'Server shutdown in progress'
    1205,   # 'Lock wait timeout exceeded; try restarting transaction'
    2003,   # 'Can't connect to MySQL server'
    2006,   # Error code 2006 'MySQL server has gone away' indicates that
            # the connection used was closed or dropped
    2013,   # 'Lost connection to MySQL server during query'
            # TODO(akeshet): consider only retrying UPDATE queries against
            # this error code, not INSERT queries, since we don't know
            # whether the query completed before or after the connection
            # lost.
    2026,   # 'SSL connection error: unknown error number'
])


def _IsRetryableException(e):
  """Determine whether a query should be retried based on exception.

  Intended for use as a handler for retry_util.

  Args:
    e: The exception to be filtered.

  Returns:
    True if the query should be retried, False otherwise.
  """
  # Exceptions usually are raised as sqlalchemy.exc.OperationalError, but
  # occasionally also escape as MySQLdb.OperationalError. Neither of those
  # exception types inherit from one another, so we fall back to string matching
  # on the exception name. See crbug.com/483654
  if 'OperationalError' in str(type(e)):
    # Unwrap the error till we get to the error raised by the DB backend.
    # Record each error_code that we encounter along the way.
    e_orig = e
    encountered_error_codes = set()
    while e_orig:
      if len(e_orig.args) and isinstance(e_orig.args[0], int):
        encountered_error_codes.add(e_orig.args[0])
      e_orig = getattr(e_orig, 'orig', None)

    if encountered_error_codes & _RETRYABLE_OPERATIONAL_ERROR_CODES:
      # Suppress logging of error code 2006 retries. They are routine and
      # expected, and logging them confuses people.
      if not 2006 in encountered_error_codes:
        logging.info('RETRYING cidb query due to %s.', e)
      return True
    else:
      logging.info('None of error codes encountered %s are-retryable.',
                   encountered_error_codes)

  return False


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




# Tuple to keep arguments that modify SQL query retry behaviour of
# SchemaVersionedMySQLConnection.
SqlConnectionRetryArgs = collections.namedtuple(
    'SqlConnectionRetryArgs',
    ('max_retry', 'sleep', 'backoff_factor'))


class SchemaVersionedMySQLConnection(object):
  """Connection to a database that is aware of its schema version."""

  SCHEMA_VERSION_TABLE_NAME = 'schemaVersionTable'
  SCHEMA_VERSION_COL = 'schemaVersion'

  def _UpdateConnectUrlArgs(self, key, db_credentials_dir, filename):
    """Read an argument for the sql connection from the given file.

    side effect: store argument in self._connect_url_args

    Args:
      key: Name of the argument to read.
      db_credentials_dir: The directory containing the credentials.
      filename: Name of the file to read.
    """
    file_path = os.path.join(db_credentials_dir, filename)
    if os.path.exists(file_path):
      self._connect_url_args[key] = osutils.ReadFile(file_path).strip()

  def _UpdateSslArgs(self, key, db_credentials_dir, filename):
    """Read an ssl argument for the sql connection from the given file.

    side effect: store argument in self._ssl_args

    Args:
      key: Name of the ssl argument to read.
      db_credentials_dir: The directory containing the credentials.
      filename: Name of the file to read.
    """
    file_path = os.path.join(db_credentials_dir, filename)
    if os.path.exists(file_path):
      if 'ssl' not in self._ssl_args:
        self._ssl_args['ssl'] = {}
      self._ssl_args['ssl'][key] = file_path

  def _UpdateConnectArgs(self, db_credentials_dir):
    """Update all connection args from |db_credentials_dir|."""
    self._UpdateConnectUrlArgs('host', db_credentials_dir, 'host.txt')
    self._UpdateConnectUrlArgs('port', db_credentials_dir, 'port.txt')
    self._UpdateConnectUrlArgs('username', db_credentials_dir, 'user.txt')
    self._UpdateConnectUrlArgs('password', db_credentials_dir, 'password.txt')

    self._UpdateSslArgs('cert', db_credentials_dir, 'client-cert.pem')
    self._UpdateSslArgs('key', db_credentials_dir, 'client-key.pem')
    self._UpdateSslArgs('ca', db_credentials_dir, 'server-ca.pem')

  def __init__(self, db_name, db_migrations_dir, db_credentials_dir,
               query_retry_args=SqlConnectionRetryArgs(8, 4, 2)):
    """SchemaVersionedMySQLConnection constructor.

    Args:
      db_name: Name of the database to connect to.
      db_migrations_dir: Absolute path to directory of migration scripts
                         for this database.
      db_credentials_dir: Absolute path to directory containing connection
                          information to the database. Specifically, this
                          directory may contain files names user.txt,
                          password.txt, host.txt, port.txt, client-cert.pem,
                          client-key.pem, and server-ca.pem This object will
                          silently drop the relevant mysql commandline flags for
                          missing files in the directory.
      query_retry_args: An optional SqlConnectionRetryArgs tuple to tweak the
                        retry behaviour of SQL queries.
    """
    if not sqlalchemy_imported:
      raise AssertionError('Unable to open cidb connections, as sqlalchemy '
                           'module could not be imported. If you need cidb, '
                           'please install the missing package by running '
                           '`sudo apt-get install python-sqlalchemy` or '
                           'similar.')

    # Note: This is a inner class because it needs to inherit from sqlalchemy,
    # but we do not know for sure that sqlalchemy has been successfully imported
    # until we enter this method (i.e. this class cannot be module-level).
    class StrictModeListener(sqlalchemy.interfaces.PoolListener):
      """Listener to set up a connection with STRICT_ALL_TABLES."""
      def __init__(self):
        pass

      def connect(self, dbapi_con, *_args, **_kwargs):
        """This listener ensures that STRICT_ALL_TABLES for all connections."""
        cur = dbapi_con.cursor()
        cur.execute("SET SESSION sql_mode='STRICT_ALL_TABLES'")
        cur.close()

    self._listener_class = StrictModeListener

    # None, or a sqlalchemy.MetaData instance
    self._meta = None

    # pid of process on which _engine was created
    self._engine_pid = None

    self._engine = None

    self.db_migrations_dir = db_migrations_dir
    self.db_credentials_dir = db_credentials_dir
    self.db_name = db_name
    self.query_retry_args = query_retry_args

    # mysql args that are optionally provided by files in db_credentials_dir
    self._connect_url_args = {}
    self._ssl_args = {}

    self._UpdateConnectArgs(db_credentials_dir)

    connect_url = sqlalchemy.engine.url.URL('mysql', **self._connect_url_args)

    # Create a temporary engine to connect to the mysql instance, and check if
    # a database named |db_name| exists. If not, create one. We use a temporary
    # engine here because the real engine will be opened with a default
    # database name given by |db_name|.
    temp_engine = sqlalchemy.create_engine(connect_url,
                                           connect_args=self._ssl_args,
                                           listeners=[self._listener_class()])
    databases = self._ExecuteWithEngine('SHOW DATABASES',
                                        temp_engine).fetchall()
    if (db_name,) not in databases:
      self._ExecuteWithEngine('CREATE DATABASE %s' % db_name, temp_engine)
      logging.info('Created database %s', db_name)

    temp_engine.dispose()

    # Now create the persistent connection to the database named |db_name|.
    # If there is a schema version table, read the current schema version
    # from it. Otherwise, assume schema_version 0.
    self._connect_url_args['database'] = db_name
    self._connect_url = sqlalchemy.engine.url.URL('mysql',
                                                  **self._connect_url_args)

    self.schema_version = self.QuerySchemaVersion()

    logging.info('Created a SchemaVersionedMySQLConnection, '
                 'sqlalchemy version %s', sqlalchemy.__version__)

  def DropDatabase(self):
    """Delete all data and tables from database, and drop database.

    Use with caution. All data in database will be deleted. Invalidates
    this database connection instance.
    """
    self._meta = None
    self._Execute('DROP DATABASE %s' % self.db_name)
    self._InvalidateEngine()

  def QuerySchemaVersion(self):
    """Query the database for its current schema version number.

    Returns:
      The current schema version from the database's schema version table,
      as an integer, or 0 if the table is empty or nonexistent.
    """
    tables = self._Execute('SHOW TABLES').fetchall()
    if (self.SCHEMA_VERSION_TABLE_NAME,) in tables:
      r = self._Execute('SELECT MAX(%s) from %s' % (
          self.SCHEMA_VERSION_COL, self.SCHEMA_VERSION_TABLE_NAME))
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
      # This is intentionally not wrapped in retries.
      self._GetEngine().execute(q)

  def _ReflectToMetadata(self):
    """Use sqlalchemy reflection to construct MetaData model of database.

    If self._meta is already populated, this does nothing.
    """
    if self._meta is not None:
      return
    self._meta = MetaData()
    fn = lambda: self._meta.reflect(bind=self._GetEngine())
    self._RunFunctorWithRetries(fn)

  def _Insert(self, table, values):
    """Create and execute a one-row INSERT query.

    Args:
      table: Table name to insert to.
      values: Dictionary of column values to insert.

    Returns:
      Integer primary key of the inserted row.
    """
    self._ReflectToMetadata()
    ins = self._meta.tables[table].insert().values(values)
    r = self._Execute(ins)
    return r.inserted_primary_key[0]

  def _InsertMany(self, table, values):
    """Create and execute an multi-row INSERT query.

    Args:
      table: Table name to insert to.
      values: A list of value dictionaries to insert multiple rows.

    Returns:
      The number of inserted rows.
    """
    # sqlalchemy 0.7 and prior has a bug in which it does not always
    # correctly unpack a list of rows to multi-insert if the list contains
    # only one item.
    if len(values) == 1:
      self._Insert(table, values[0])
      return 1

    self._ReflectToMetadata()
    ins = self._meta.tables[table].insert()
    r = self._Execute(ins, *values)
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
    upd = self._meta.tables[table].update().where(
        primary_key == row_id).values(values)
    r = self._Execute(upd)
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
    upd = self._meta.tables[table].update().where(where).values(values)
    r = self._Execute(upd)
    return r.rowcount

  def _Select(self, table, row_id, columns):
    """Create and execute a one-row one-table SELECT query by primary key.

    Args:
      table: Table name to select from.
      row_id: Primary key value of row to select.
      columns: List of column names to select.

    Returns:
      A column name to column value dict for the row found, if a row was found.
      None if no row was.
    """
    self._ReflectToMetadata()
    primary_key = self._GetPrimaryKey(table)
    table_m = self._meta.tables[table]
    columns_m = [table_m.c[col_name] for col_name in columns]
    sel = sqlalchemy.sql.select(columns_m).where(primary_key == row_id)
    r = self._Execute(sel).fetchall()
    if r:
      assert len(r) == 1, 'Query by primary key returned more than 1 row.'
      return dict(zip(columns, r[0]))
    else:
      return None

  def _SelectWhere(self, table, where, columns):
    """Create and execute a one-table SELECT query with a custom where clause.

    Args:
      table: Table name to update.
      where: Raw SQL for the where clause, in string form, e.g.
             'build_id = 1 and board = "tomato"'
      columns: List of column names to select.

    Returns:
      A list of column name to column value dictionaries each representing
      a row that was selected.
    """
    self._ReflectToMetadata()
    table_m = self._meta.tables[table]
    columns_m = [table_m.c[col_name] for col_name in columns]
    sel = sqlalchemy.sql.select(columns_m).where(where)
    r = self._Execute(sel)
    return [dict(zip(columns, values)) for values in r.fetchall()]

  def _Execute(self, query, *args, **kwargs):
    """Execute a query with retries.

    This method executes a query using the engine credentials that
    were set up in the constructor for this object. If necessary,
    a new engine unique to this pid will be created.

    Args:
      query: Query to execute, of type string, or sqlalchemy.Executible,
             or other sqlalchemy-executible statement (see sqlalchemy
             docs).
      *args: Additional args passed along to .execute(...)
      **kwargs: Additional args passed along to .execute(...)

    Returns:
      The result of .execute(...)
    """
    return self._ExecuteWithEngine(query, self._GetEngine(),
                                   *args, **kwargs)

  def _ExecuteWithEngine(self, query, engine, *args, **kwargs):
    """Execute a query using |engine|, with retires.

    This method wraps execution of a query against an engine in retries.
    The engine will automatically create new connections if a prior connection
    was dropped.

    Args:
      query: Query to execute, of type string, or sqlalchemy.Executible,
             or other sqlalchemy-executible statement (see sqlalchemy
             docs).
      engine: sqlalchemy.engine to use.
      *args: Additional args passed along to .execute(...)
      **kwargs: Additional args passed along to .execute(...)

    Returns:
      The result of .execute(...)
    """
    f = lambda: engine.execute(query, *args, **kwargs)
    logging.info('Running cidb query on pid %s, repr(query) starts with %s',
                 os.getpid(), repr(query)[:100])
    return self._RunFunctorWithRetries(f)

  def _RunFunctorWithRetries(self, functor):
    """Run the given |functor| with correct retry parameters."""

    # TODO(hidehiko): Move this back to retry implementation, because this
    # should be useful for other retry run, too.
    def _StatusCallback(attempt, success):
      if success and attempt:
        logging.info('cidb query succeeded after %d retries', attempt)

    return retry_stats.RetryWithStats(
        retry_stats.CIDB,
        handler=_IsRetryableException,
        max_retry=self.query_retry_args.max_retry,
        sleep=self.query_retry_args.sleep,
        backoff_factor=self.query_retry_args.backoff_factor,
        status_callback=_StatusCallback,
        raise_first_exception_on_failure=False,
        functor=functor)

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
                                   listeners=[self._listener_class()])
      self._engine = e
      self._engine_pid = pid
      logging.info('Created cidb engine %s@%s for pid %s', e.url.username,
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

  # The buildbucket_id is the request id inserted by pre-cq-launcher when
  # it launches by a pre-cq, not the pre-cq-launcher builder buildbucket_id
  _SQL_FETCH_ACTIONS = (
      'SELECT c.id, b.id, action, c.reason, build_config, '
      'change_number, patch_number, change_source, timestamp, c.buildbucket_id,'
      ' status FROM clActionTable c JOIN buildTable b ON build_id = b.id ')
  _SQL_FETCH_MESSAGES = (
      'SELECT build_id, build_config, waterfall, builder_name, build_number, '
      'message_type, message_subtype, message_value, timestamp, board FROM '
      'buildMessageTable c JOIN buildTable b ON build_id = b.id ')
  _DATE_FORMAT = '%Y-%m-%d'

  NUM_RESULTS_NO_LIMIT = -1

  BUILD_STATUS_KEYS = (
      'id', 'build_config', 'start_time', 'finish_time', 'status', 'waterfall',
      'build_number', 'builder_name', 'platform_version', 'full_version',
      'milestone_version', 'important', 'buildbucket_id', 'summary',
      'buildbot_generation')

  def __init__(self, db_credentials_dir, *args, **kwargs):
    super(CIDBConnection, self).__init__('cidb', CIDB_MIGRATIONS_DIR,
                                         db_credentials_dir, *args, **kwargs)

  def GetTime(self):
    """Gets the current time, according to database.

    Returns:
      datetime.datetime instance.
    """
    return self._Execute('SELECT NOW()').fetchall()[0][0]

  @minimum_schema(47)
  def InsertBuild(self, builder_name, waterfall, build_number,
                  build_config, bot_hostname, master_build_id=None,
                  timeout_seconds=None, important=None, buildbucket_id=None):
    """Insert a build row.

    Args:
      builder_name: buildbot builder name.
      waterfall: buildbot waterfall name.
      build_number: buildbot build number.
      build_config: cbuildbot config of build
      bot_hostname: hostname of bot running the build
      master_build_id: (Optional) primary key of master build to this build.
      timeout_seconds: (Optional) If provided, total time allocated for this
                       build. A deadline is recorded in cidb for the current
                       build to end.
      important: (Optional) If provided, the |important| value for this build.
      buildbucket_id: (Optional) If provided, the |buildbucket_id| value for
                       this build.
    """
    values = {
        'builder_name': builder_name,
        'buildbot_generation': constants.BUILDBOT_GENERATION,
        'waterfall': waterfall,
        'build_number': build_number,
        'build_config': build_config,
        'bot_hostname': bot_hostname,
        'start_time': sqlalchemy.func.current_timestamp(),
        'master_build_id': master_build_id,
        'important': important,
        'buildbucket_id': buildbucket_id
    }
    if timeout_seconds is not None:
      now = self.GetTime()
      duration = datetime.timedelta(seconds=timeout_seconds)
      values.update({'deadline': now + duration})

    return self._Insert('buildTable', values)

  @minimum_schema(3)
  def InsertCLActions(self, build_id, cl_actions, timestamp=None):
    """Insert a list of |cl_actions|.

    If |cl_actions| is empty, this function does nothing.

    Args:
      build_id: primary key of build that performed these actions.
      cl_actions: A list of CLAction objects.
      timestamp: (Optional) timestamp of the cl_actions. If not provided, use
        the current timestamp of the database.

    Returns:
      Number of actions inserted.
    """
    if not cl_actions:
      return 0

    values = []
    for cl_action in cl_actions:
      change_number = cl_action.change_number
      patch_number = cl_action.patch_number
      change_source = cl_action.change_source
      action = cl_action.action
      reason = cl_action.reason
      buildbucket_id = cl_action.buildbucket_id
      value = {
          'build_id': build_id,
          'change_source': change_source,
          'change_number': change_number,
          'patch_number': patch_number,
          'action': action,
          'reason': reason,
          'buildbucket_id': buildbucket_id}
      if timestamp != None:
        value['timestamp'] = timestamp
      values.append(value)

    retval = self._InsertMany('clActionTable', values)

    stats = graphite.StatsFactory.GetInstance()
    for cl_action in cl_actions:
      r = cl_action.reason or 'no_reason'

      # TODO(akeshet) This is a slightly hacky workaround for the fact that our
      # strategy reasons contain a ':', but statsd considers that character to
      # be a name terminator.
      statsd_name = 'cl_actions.%s' % cl_action.action
      stats.Counter(statsd_name).increment(r.replace(':', '_'))

      counter = metrics.Counter(constants.MON_CL_ACTION)
      counter.increment(fields={'reason': r, 'action': cl_action.action})

    return retval

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

  @minimum_schema(28)
  def InsertBuildStage(self, build_id, name, board=None,
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
    return self._Insert('buildStageTable', {'build_id': build_id,
                                            'name': name,
                                            'board': board,
                                            'status': status})

  @minimum_schema(29)
  def InsertFailure(self, build_stage_id, exception_type, exception_message,
                    exception_category=constants.EXCEPTION_CATEGORY_UNKNOWN,
                    outer_failure_id=None,
                    extra_info=None):
    """Insert a failure description into database.

    Args:
      build_stage_id: primary key, in buildStageTable, of the stage where
                      failure occured.
      exception_type: str name of the exception class.
      exception_message: str description of the failure.
      exception_category: (Optional) one of
                          constants.EXCEPTION_CATEGORY_ALL_CATEGORIES,
                          Default: 'unknown'.
      outer_failure_id: (Optional) primary key of outer failure which contains
                        this failure. Used to store CompoundFailure
                        relationship.
      extra_info: (Optional) extra category-specific string description giving
                  failure details. Used for programmatic triage.
    """
    if exception_message:
      exception_message = exception_message[:240]
    values = {'build_stage_id': build_stage_id,
              'exception_type': exception_type,
              'exception_message': exception_message,
              'exception_category': exception_category,
              'outer_failure_id': outer_failure_id,
              'extra_info': extra_info}
    return self._Insert('failureTable', values)

  @minimum_schema(42)
  def InsertBuildMessage(self, build_id, message_type=None,
                         message_subtype=None, message_value=None, board=None):
    """Insert a build message into database.

    Args:
      build_id: primary key of build recording this message.
      message_type: Optional str name of message type.
      message_subtype: Optional str name of message subtype.
      message_value: Optional value of message.
      board: Optional str name of the board.
    """
    if message_type:
      message_type = message_type[:240]
    if message_subtype:
      message_subtype = message_subtype[:240]
    if message_value:
      message_value = message_value[:480]
    if board:
      board = board[:240]

    values = {'build_id': build_id,
              'message_type': message_type,
              'message_subtype': message_subtype,
              'message_value': message_value,
              'board': board}
    return self._Insert('buildMessageTable', values)

  @minimum_schema(57)
  def InsertHWTestResults(self, hwTestResults):
    """Insert HWTest results.

    Args:
      hwTestResults: A list of hwtest_results.HWTestResult instances.

    Returns:
      The number of inserted rows.
    """
    values = []
    for result in hwTestResults:
      values.append({'build_id': result.build_id,
                     'test_name': result.test_name,
                     'status': result.status})

    return self._InsertMany('hwTestResultTable', values)

  @minimum_schema(56)
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
                         'important': d.get('important'),
                         'unibuild': d.get('unibuild', False),
                         'suite_scheduling': d.get('suite_scheduling', False)})

  @minimum_schema(2)
  def GetMetadata(self, build_id):
    """Get the metadata for |build_id| in buildTable.

    Args:
      build_id: Id of the row to select.
      fields: List of fields (column names) to select.

    Returns:
      The metadata object.
    """
    fields = ['chrome_version', 'milestone_version', 'platform_version',
              'full_version', 'sdk_version', 'toolchain_url', 'build_type',
              'important']
    return self._Select('buildTable', build_id, fields)

  @minimum_schema(32)
  def ExtendDeadline(self, build_id, timeout_seconds):
    """Extend the deadline for this build.

    Args:
      build_id: primary key, in buildTable, of the build for which deadline
          should be extended.
      timeout_seconds: Time remaining for the deadline from the current time.

    Returns:
      Number of rows updated (1 for success, 0 for failure)
      Deadline extension can fail if
        (1) The deadline is already past, or
        (2) The new deadline requested is earlier than the original deadline.
    """
    return self._Execute(
        'UPDATE buildTable SET deadline = NOW() + INTERVAL %d SECOND WHERE '
        'id = %d AND '
        '(deadline = 0 OR deadline > NOW()) AND '
        'NOW() + INTERVAL %d SECOND > deadline'
        % (timeout_seconds, build_id, timeout_seconds)
        ).rowcount

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
        'ec_firmware_version': board_metadata.get('ec-firmware-version'),
    }
    return self._UpdateWhere(
        'boardPerBuildTable',
        'build_id = %d and board = "%s"' % (build_id, board),
        update_dict)

  @minimum_schema(28)
  def StartBuildStage(self, build_stage_id):
    """Marks a build stage as inflight, in the database.

    Args:
      build_stage_id: primary key of the build stage in buildStageTable.
    """
    current_timestamp = sqlalchemy.func.current_timestamp()
    return self._Update(
        'buildStageTable',
        build_stage_id,
        {'status': constants.BUILDER_STATUS_INFLIGHT,
         'start_time': current_timestamp})

  @minimum_schema(46)
  def WaitBuildStage(self, build_stage_id):
    """Marks a build stage as waiting, in the database.

    Args:
      build_stage_id: primary key of the build stage in buildStageTable.
    """
    return self._Update(
        'buildStageTable',
        build_stage_id,
        {'status': constants.BUILDER_STATUS_WAITING})

  @minimum_schema(28)
  def FinishBuildStage(self, build_stage_id, status):
    """Marks a build stage as finished, in the database.

    Args:
      build_stage_id: primary key of the build stage in buildStageTable.
      status: one of constants.BUILDER_COMPLETED_STATUSES
    """
    current_timestamp = sqlalchemy.func.current_timestamp()
    return self._Update(
        'buildStageTable',
        build_stage_id,
        {'status': status,
         'finish_time': current_timestamp,
         'final': True})

  @minimum_schema(25)
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

    Returns:
      The number of rows that were updated.
    """
    self._ReflectToMetadata()

    clause = 'id = %d' % build_id
    if strict:
      clause += ' AND final = False'

    # The current timestamp is evaluated on the database, not locally.
    current_timestamp = sqlalchemy.func.current_timestamp()
    values = {
        'finish_time': current_timestamp,
        'final': True
    }
    if status is not None:
      values.update(status=status)
    if summary is not None:
      summary = summary[:1024]
      values.update(summary=summary)
    if metadata_url is not None:
      values.update(metadata_url=metadata_url)

    return self._UpdateWhere('buildTable', clause, values)

  @minimum_schema(16)
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
    self._Execute(
        'UPDATE childConfigPerBuildTable '
        'SET status="%s", final=1 '
        'WHERE (build_id, child_config) = (%d, "%s")' %
        (status, build_id, child_config))

  @minimum_schema(50)
  def GetBuildStatusWithBuildbucketId(self, buildbucket_id):
    status = self._SelectWhere('buildTable',
                               'buildbucket_id = "%s"' % buildbucket_id,
                               self.BUILD_STATUS_KEYS)
    return status[0] if status else None

  @minimum_schema(47)
  def GetBuildStatus(self, build_id):
    """Gets the status of the build.

    Args:
      build_id: build id to fetch.

    Returns:
      Dictionary for a single build (see GetBuildStatuses for keys) or
      None if no build with this id was found.
    """
    statuses = self.GetBuildStatuses([build_id])
    return statuses[0] if statuses else None

  @minimum_schema(47)
  def GetBuildStatuses(self, build_ids):
    """Gets the statuses of the builds.

    Args:
      build_ids: A list of build id to fetch.

    Returns:
      A list of dictionary with keys (id, build_config, start_time,
      finish_time, status, waterfall, build_number, builder_name,
      platform_version, full_version, milestone_version, important)
      (see BUILD_STATUS_KEYS) or None if no build with this id was found.
    """
    return self._SelectWhere(
        'buildTable',
        'id IN (%s)' % ','.join(str(int(x)) for x in build_ids),
        self.BUILD_STATUS_KEYS)

  @minimum_schema(30)
  def GetBuildStage(self, build_stage_id):
    """Gets stage given the build_stage_id.

    Args:
      build_stage_id: build_stage_id to fetch the stage.

    Returns:
      If the build_stage_id exists, returns a dictionary presenting the stage
      with keys (id, build_id, name, board, status, last_updated, start_time,
      finish_time, final); else, returns None.
    """
    stage = self._SelectWhere(
        'buildStageTable',
        'id = %s' % build_stage_id,
        ['id', 'build_id', 'name', 'board', 'status',
         'last_updated', 'start_time', 'finish_time', 'final'])
    return stage[0] if stage else None

  @minimum_schema(30)
  def GetBuildStages(self, build_id):
    """Gets all the stages of a given build.

    Args:
      build_id: build id of the build to fetch the stages for.

    Returns:
      A list containing, for each stage of the build found, a dictionary with
      keys (id, build_id, name, board, status, last_updated, start_time,
      finish_time, final).
    """
    return self.GetBuildsStages([build_id])

  @minimum_schema(30)
  def GetBuildsStages(self, build_ids):
    """Gets all the stages for all listed build_ids.

    Args:
      build_ids: list of build ids of the builds to fetch the stages for.

    Returns:
      A list containing, for each stage of the builds found, a dictionary with
      keys (id, build_id, name, board, status, last_updated, start_time,
      finish_time, final).
    """
    if not build_ids:
      return []
    return self._SelectWhere(
        'buildStageTable',
        'build_id IN (%s)' % ','.join(str(int(x)) for x in build_ids),
        ['id', 'build_id', 'name', 'board', 'status',
         'last_updated', 'start_time', 'finish_time', 'final'])

  @minimum_schema(43)
  def GetSlaveStatuses(self, master_build_id, buildbucket_ids=None):
    """Gets the statuses of slave builders to given build.

    Args:
      master_build_id: build id of the master build to fetch the slave
                       statuses for.
      buildbucket_ids: A list of buildbucket_ids (string). If it's given,
        only fetch the builds with buildbucket_id in the buildbucket_ids.
        Default to None.

    Returns:
      A list containing a dictionary with keys BUILD_STATUS_KEYS.
      If buildbucket_ids is None, the list contains all slave builds found
      in the buildTable; else, the list only contains the slave builds
      with |buildbucket_id| in the buildbucket_ids list.
    """
    if buildbucket_ids is None:
      return self._SelectWhere(
          'buildTable',
          'master_build_id = %d' % master_build_id,
          self.BUILD_STATUS_KEYS)
    else:
      if buildbucket_ids:
        return self._SelectWhere(
            'buildTable',
            'master_build_id = %d AND buildbucket_id IN (%s)' %
            (master_build_id, ','.join(
                '"%s"' % x for x in buildbucket_ids)),
            self.BUILD_STATUS_KEYS)
      else:
        return []

  @minimum_schema(30)
  def GetSlaveStages(self, master_build_id, buildbucket_ids=None):
    """Gets the stages of slave builds to given master_build_id.

    Args:
      master_build_id: build id of the master build to fetch the slave
                       stages for.
      buildbucket_ids: A list of buildbucket_ids (strings) of slave builds
        to given master_build_id. If buildbucket_ids is given, only fetch
        the stages of builds with |buildbucket_id| in buildbucket_ids.
        Default to None.

    Returns:
      A list containing, for each stage of slave builds found,
      a dictionary with keys (id, build_id, name, board, status, last_updated,
      start_time, finish_time, final, build_config).
      If buildbucket_ids is None, the list contains all stages of all slaves;
      else, it only contains the stages of the slaves with |buildbucket_id|
      in buildbucket_ids.
    """
    bs_table_columns = ['id', 'build_id', 'name', 'board', 'status',
                        'last_updated', 'start_time', 'finish_time', 'final']
    bs_prepended_columns = ['bs.' + x for x in bs_table_columns]

    query = ('SELECT %s, b.build_config FROM buildStageTable bs JOIN '
             'buildTable b ON build_id = b.id WHERE b.master_build_id = %d' %
             (', '.join(bs_prepended_columns), master_build_id))

    results = []
    if buildbucket_ids is None:
      results = self._Execute(query).fetchall()
    else:
      if not buildbucket_ids:
        return []

      query += (' AND b.buildbucket_id IN (%s)' %
                (','.join('"%s"' % x for x in buildbucket_ids)))
      results = self._Execute(query).fetchall()

    columns = bs_table_columns + ['build_config']
    return [dict(zip(columns, values)) for values in results]

  @minimum_schema(44)
  def GetBuildsFailures(self, build_ids):
    """Gets the failure entries for all listed build_ids.

    Args:
      build_ids: list of build ids of the builds to fetch failures for.

    Returns:
      A list of failure_message_lib.StageFailure instances.
    """
    if not build_ids:
      return []

    columns_string = ', '.join(failure_message_lib.FAILURE_KEYS)

    query = ('SELECT %s FROM failureView WHERE build_id IN (%s)' %
             (columns_string, ','.join(str(int(x)) for x in build_ids)))

    results = self._Execute(query).fetchall()
    return [failure_message_lib.StageFailure(*values) for values in results]

  @minimum_schema(44)
  def GetSlaveFailures(self, master_build_id, buildbucket_ids=None):
    """Gets the failure entries for slave builds to given build.

    Args:
      master_build_id: build id of the master build to fetch failures
                       for.
      buildbucket_ids: A list of buildbucket_ids (strings) of slave builds
        to given master_build_id. If buildbucket_ids is given, only fetch
        the failures of builds with |buildbucket_id| in buildbucket_ids.
        Default to None.

    Returns:
      A list of failure_message_lib.StageFailure instances.
    """
    columns_string = ', '.join(failure_message_lib.FAILURE_KEYS)

    query = ('SELECT %s FROM failureView WHERE master_build_id = %s ' %
             (columns_string, master_build_id))

    results = []
    if buildbucket_ids is None:
      results = self._Execute(query).fetchall()
    else:
      if not buildbucket_ids:
        return []

      query += (' AND buildbucket_id IN (%s)' %
                (','.join('"%s"' % x for x in buildbucket_ids)))
      results = self._Execute(query).fetchall()

    return [failure_message_lib.StageFailure(*values) for values in results]

  @minimum_schema(32)
  def GetTimeToDeadline(self, build_id):
    """Gets the time remaining till the deadline for given build_id.

    Always use this function to find time remaining to a deadline. This function
    computes all times on the database. You run the risk of hitting timezone
    issues if you compute remaining time locally.

    Args:
      build_id: The build_id of the build to query.

    Returns:
      The time remaining to the deadline in seconds.
      0 if the deadline is already past.
      None if no deadline is found.
    """
    # Sign information is lost in the timediff coercion into python
    # datetime.timedelta type. So, we must find out if the deadline is past
    # separately.
    r = self._Execute(
        'SELECT deadline >= NOW(), TIMEDIFF(deadline, NOW()), deadline '
        'from buildTable where id = %d' % build_id).fetchall()
    if not r:
      return None

    time_remaining = r[0][1]
    deadline = r[0][2]
    if deadline is None:
      return None

    deadline_past = (r[0][0] == 0)
    return 0 if deadline_past else abs(time_remaining.total_seconds())

  @minimum_schema(47)
  def GetBuildHistory(self, build_config, num_results,
                      ignore_build_id=None, start_date=None, end_date=None,
                      starting_build_number=None, ending_build_number=None,
                      milestone_version=None, platform_version=None,
                      starting_build_id=None, waterfall=None,
                      buildbot_generation=None, final=False):
    """Returns basic information about most recent builds.

    By default this function returns the most recent builds. Some arguments can
    restrict the result to older builds.

    Args:
      build_config: config name of the build.
      num_results: Number of builds to search back. Set this to
          CIDBConnection.NUM_RESULTS_NO_LIMIT to request no limit on the number
          of results.
      ignore_build_id: (Optional) Ignore a specific build. This is most useful
          to ignore the current build when querying recent past builds from a
          build in flight.
      start_date: (Optional, type: datetime.date) Get builds that occured on or
          after this date.
      end_date: (Optional, type:datetime.date) Get builds that occured on or
          before this date.
      starting_build_number: (Optional) The minimum build_number for which
          data should be retrieved.
      ending_build_number: (Optional) The maximum build_number for which
          data should be retrieved.
      milestone_version: (Optional) Return only results for this
          milestone_version.
      platform_version: (Optional) Return only results for this
          platform_version.
      starting_build_id: (Optional) The minimum build_id for which data should
          be retrieved.
      waterfall: (Optional) The waterfall for which data should be retrieved.
      buildbot_generation: (Optional) The buildbot_generation for which data
          should be retrieved.
      final: (Optional) If True, only retrieve final (ie finished) builds.

    Returns:
      A sorted list of dicts containing up to |number| dictionaries for
      build statuses in descending order. Valid keys in the dictionary are
      [id, build_config, buildbot_generation, waterfall, build_number,
      start_time, finish_time, platform_version, full_version, status,
      important, buildbucket_id].
    """
    where_clauses = ['build_config = "%s"' % build_config]
    if start_date is not None:
      where_clauses.append('date(start_time) >= date("%s")' %
                           start_date.strftime(self._DATE_FORMAT))
    if end_date is not None:
      where_clauses.append('date(start_time) <= date("%s")' %
                           end_date.strftime(self._DATE_FORMAT))
    if starting_build_number is not None:
      where_clauses.append('build_number >= %d' % starting_build_number)
    if starting_build_id is not None:
      where_clauses.append('id >= %d' % starting_build_id)
    if ending_build_number is not None:
      where_clauses.append('build_number <= %d' % ending_build_number)
    if ignore_build_id is not None:
      where_clauses.append('id != %d' % ignore_build_id)
    if milestone_version is not None:
      where_clauses.append('milestone_version = "%s"' % milestone_version)
    if platform_version is not None:
      where_clauses.append('platform_version = "%s"' % platform_version)
    if waterfall is not None:
      where_clauses.append('waterfall = "%s"' % waterfall)
    if buildbot_generation is not None:
      where_clauses.append('buildbot_generation = "%s"' % buildbot_generation)
    if final:
      where_clauses.append('final = 1')
    query = (
        'SELECT %s'
        ' FROM buildTable'
        ' WHERE %s'
        ' ORDER BY id DESC' %
        (', '.join(CIDBConnection.BUILD_STATUS_KEYS),
         ' AND '.join(where_clauses)))
    if num_results != self.NUM_RESULTS_NO_LIMIT:
      query += ' LIMIT %d' % num_results

    results = self._Execute(query).fetchall()
    return [dict(zip(CIDBConnection.BUILD_STATUS_KEYS, values))
            for values in results]

  @minimum_schema(47)
  def GetMostRecentBuild(self, waterfall, build_config, milestone_version=None):
    """Returns basic information about most recent completed build.

    Args:
      waterfall: waterfall of the build.
      build_config: config name of the build.
      milestone_version: optional milestone_version to filter upon.

    Returns:
      A dictionary with keys BUILD_STATUS_KEYS, or None if no results.
    """
    where_clauses = ['waterfall = "%s"' % waterfall,
                     'build_config = "%s"' % build_config,
                     'final = 1']
    if milestone_version:
      where_clauses.append('milestone_version = "%d"' % milestone_version)

    query = (
        'SELECT %s'
        ' FROM buildTable'
        ' WHERE %s'
        ' ORDER BY id DESC'
        ' LIMIT 1' %
        (', '.join(self.BUILD_STATUS_KEYS), ' AND '.join(where_clauses)))

    results = self._Execute(query).fetchall()
    return (dict(zip(self.BUILD_STATUS_KEYS, results[0]))
            if len(results) else None)

  @minimum_schema(26)
  def GetAnnotationsForBuilds(self, build_ids):
    """Returns the annotations for given build_ids.

    Args:
      build_ids: list of build_ids for which annotations are requested.

    Returns:
      {id: annotations} where annotations is itself a list of dicts containing
      annotations. Valid keys in annotations are [failure_category,
      failure_message, blame_url, notes].
    """
    columns_to_report = ['failure_category', 'failure_message',
                         'blame_url', 'notes']
    where_or_clauses = []
    for build_id in build_ids:
      where_or_clauses.append('build_id = %d' % build_id)
    annotations = self._SelectWhere('annotationsTable',
                                    ' OR '.join(where_or_clauses),
                                    ['build_id'] + columns_to_report)

    results = {}
    for annotation in annotations:
      build_id = annotation['build_id']
      if build_id not in results:
        results[build_id] = []
      results[build_id].append(annotation)
    return results

  @minimum_schema(11)
  def GetActionsForChanges(self, changes, ignore_patch_number=True,
                           status=None, action=None, start_time=None):
    """Gets all the actions for the given changes.

    Note, this includes all patches of the given changes.

    Args:
      changes: A list of GerritChangeTuple, GerritPatchTuple or GerritPatch
        specifying the changes to whose actions should be fetched.
      ignore_patch_number: Boolean indicating whether to ignore patch_number of
        the changes. If ignore_patch_number is False, only get the actions with
        matched patch_number. Default to True.
      status: If provided, only return the actions with build is |status| (a
        member of constants.BUILDER_ALL_STATUSES). Default to None.
      action: If provided, only return the actions is |action| (a member of
        constants.CL_ACTIONS). Default to None.
      start_time: If provided, only return the actions with timestamp >=
        start_time. Default to None.

    Returns:
      A list of CLAction instances, in action id order.
    """
    if not changes:
      return []

    clauses = []
    basic_conds = []
    if status is not None:
      basic_conds.append('status = "%s"' % status)
    if action is not None:
      basic_conds.append('action = "%s"' % action)
    if start_time is not None:
      basic_conds.append('timestamp > TIMESTAMP("%s")' % start_time)

    # Note: We are using a string of OR statements rather than a 'WHERE IN'
    # style clause, because 'WHERE IN' does not make use of multi-column
    # indexes, and therefore has poor performance with a large table.
    for change in changes:
      change_number = int(change.gerrit_number)
      change_source = 'internal' if change.internal else 'external'
      conds = ['change_number = %d' % change_number,
               'change_source = "%s"' % change_source]

      if not ignore_patch_number:
        patch_number = int(change.patch_number)
        conds.append('patch_number = %d' % patch_number)

      conds.extend(basic_conds)
      conds_str = ' AND '.join(conds)
      clauses.append('(' + conds_str + ')')

    clause = ' OR '.join(clauses)
    results = self._Execute(
        '%s WHERE %s' % (self._SQL_FETCH_ACTIONS, clause)).fetchall()
    return [clactions.CLAction(*values) for values in results]

  @minimum_schema(11)
  def GetActionsForBuild(self, build_id):
    """Gets all the actions associated with build |build_id|.

    Returns:
      A list of CLAction instance, in action id order.
    """
    q = '%s WHERE build_id = %s' % (self._SQL_FETCH_ACTIONS, build_id)
    results = self._Execute(q).fetchall()
    return [clactions.CLAction(*values) for values in results]

  @minimum_schema(11)
  def GetActionHistory(self, start_date, end_date=None):
    """Get the action history of CLs in the specified range.

    This will get the full action history of any patches that were touched
    by the CQ or Pre-CQ during the specified time range. Note: Since this
    includes the full action history of these patches, it may include actions
    outside the time range.

    Args:
      start_date: (Type: datetime.date) The first date on which you want action
          history.
      end_date: (Type: datetime.date) The last date on which you want action
          history (inclusive).
    """
    values = {'start_date': start_date.strftime(self._DATE_FORMAT),
              'end_date': end_date.strftime(self._DATE_FORMAT)}

    # Enforce start and end date.
    conds = 'timestamp >= TIMESTAMP(%(start_date)s)'
    if end_date:
      conds += (' AND timestamp < '
                'TIMESTAMP(DATE_ADD(%(end_date)s, INTERVAL 1 DAY))')

    changes = ('SELECT DISTINCT change_number, patch_number, change_source '
               'FROM clActionTable WHERE %s' % conds)
    query = '%s NATURAL JOIN (%s) as w' % (self._SQL_FETCH_ACTIONS, changes)
    results = self._Execute(query, values).fetchall()
    return clactions.CLActionHistory(clactions.CLAction(*values)
                                     for values in results)

  @minimum_schema(11)
  def GetActionsSince(self, start_date):
    """Get all CL actions after a specific |start_date|.

    Unlike GetActionHistory, this method makes use of only a single simple
    SELECT query, and does not perform any of the precalculations that
    CLActionHistory makes use of for statistics. Hence, this method is most
    suitable for queries expected to return a large result set.

    Args:
      start_date: (Type: datetime.date) The first date on which you want action
                  history.
    """
    values = {'start_date': start_date.strftime(self._DATE_FORMAT)}

    # Enforce start date
    conds = 'timestamp >= TIMESTAMP(%(start_date)s)'

    query = '%s WHERE %s' % (self._SQL_FETCH_ACTIONS, conds)
    results = self._Execute(query, values).fetchall()
    return [clactions.CLAction(*values) for values in results]

  @minimum_schema(29)
  def HasFailureMsgForStage(self, build_stage_id):
    """Determine whether a build stage has failure messages in failureTable.

    Args:
      build_stage_id: The id of the build_stage to query for.

    Returns:
      True if there're failures reported to failureTable for this build stage
      to cidb; else, False.
    """
    failures = self._SelectWhere('failureTable',
                                 'build_stage_id = %d' % build_stage_id,
                                 ['id'])
    return bool(failures)

  @minimum_schema(40)
  def GetKeyVals(self):
    """Get key-vals from keyvalTable.

    Returns:
      A dictionary of {key: value} strings (values may also be None).
    """
    results = self._Execute('SELECT k, v FROM keyvalTable').fetchall()
    return dict(results)

  @minimum_schema(42)
  def GetBuildMessages(self, build_id):
    """Gets build messages from buildMessageTable.

    Args:
      build_id: The build to get messages for.

    Returns:
      A list of build message dictionaries, where each dictionary contains
      keys build_id, build_config, builder_name, build_number, message_type,
      message_subtype, message_value, timestamp, board.
    """
    return self._GetBuildMessagesWithClause('build_id = %s' % build_id)

  @minimum_schema(42)
  def GetSlaveBuildMessages(self, master_build_id):
    """Gets build messages from buildMessageTable.

    Args:
      master_build_id: The build to get all slave messages for.

    Returns:
      A list of build message dictionaries, where each dictionary contains
      keys build_id, build_config, waterfall, builder_name, build_number,
      message_type, message_subtype, message_value, timestamp, board.
    """
    # Currently we only retry slave builds in Buildbucket which fail to pass
    # SyncStage. It means if a build fails to pass HWTestStage, where it posts
    # messages to buildMessageTable, we won't retry it. So there won't be
    # duplicated messages for one slave build.
    # TODO(nxia): it'll be good to have the buildbucket_ids filter.
    return self._GetBuildMessagesWithClause(
        'master_build_id = %s' % master_build_id)

  def _GetBuildMessagesWithClause(self, clause):
    """Private helper method for fetching build messages."""
    columns = ['build_id', 'build_config', 'waterfall', 'builder_name',
               'build_number', 'message_type', 'message_subtype',
               'message_value', 'timestamp', 'board']
    results = self._Execute('%s WHERE %s' % (self._SQL_FETCH_MESSAGES,
                                             clause)).fetchall()
    return [dict(zip(columns, values)) for values in results]

  @minimum_schema(57)
  def GetHWTestResultsForBuilds(self, build_ids):
    """Get HWTest results for builds.

    Args:
      build_ids: A list of build_ids (strings) to get the HWTest results.

    Returns:
      A list of HWTest result dictionaries, where each dictionary contains keys
        id, build_id, test_name and status.
    """
    q = ('SELECT * from hwTestResultTable WHERE build_id IN (%s)' %
         ','.join(str(int(x)) for x in build_ids))
    results = self._Execute(q).fetchall()

    return [hwtest_results.HWTestResult(*values) for values in results]


def _INV():
  raise AssertionError('CIDB connection factory has been invalidated.')

CONNECTION_TYPE_PROD = 'prod'   # production database
CONNECTION_TYPE_DEBUG = 'debug' # debug database, used by --debug builds
CONNECTION_TYPE_MOCK = 'mock'   # mock connection, not backed by database
CONNECTION_TYPE_NONE = 'none'   # explicitly no connection
CONNECTION_TYPE_INV = 'invalid' # invalidated connection

class CIDBConnectionFactoryClass(factory.ObjectFactory):
  """Factory class used by builders to fetch the appropriate cidb connection"""

  _CIDB_CONNECTION_TYPES = {
      CONNECTION_TYPE_PROD: factory.CachedFunctionCall(
          lambda: CIDBConnection(constants.CIDB_PROD_BOT_CREDS)),
      CONNECTION_TYPE_DEBUG: factory.CachedFunctionCall(
          lambda: CIDBConnection(constants.CIDB_DEBUG_BOT_CREDS)),
      CONNECTION_TYPE_MOCK: None,
      CONNECTION_TYPE_NONE: lambda: None,
      CONNECTION_TYPE_INV: _INV,
      }

  def _allowed_cidb_transition(self, from_setup, to_setup):
    # Always allow factory to be invalidated.
    if to_setup == CONNECTION_TYPE_INV:
      return True

    # Allow transition invalid - > mock
    if from_setup == CONNECTION_TYPE_INV and to_setup == CONNECTION_TYPE_MOCK:
      return True

    # Otherwise, only allow transitions between none -> none, mock -> mock, and
    # mock -> none.
    if from_setup == CONNECTION_TYPE_MOCK:
      if to_setup in (CONNECTION_TYPE_NONE, CONNECTION_TYPE_MOCK):
        return True
    if from_setup == CONNECTION_TYPE_NONE and to_setup == from_setup:
      return True
    return False

  def __init__(self):
    super(CIDBConnectionFactoryClass, self).__init__(
        'cidb connection', self._CIDB_CONNECTION_TYPES,
        self._allowed_cidb_transition)

  def IsCIDBSetup(self):
    """Returns True iff GetCIDBConnectionForBuilder is ready to be called."""
    return self.is_setup

  def GetCIDBConnectionType(self):
    """Returns the type of db connection that is set up.

    Returns:
      One of ('prod', 'debug', 'mock', 'none', 'invalid', None)
    """
    return self.setup_type

  def InvalidateCIDBSetup(self):
    """Invalidate the CIDB connection factory.

    This method may be called at any time, even after a setup method. Once
    this is called, future calls to GetCIDBConnectionForBuilder will raise
    an assertion error.
    """
    self.Setup(CONNECTION_TYPE_INV)

  def SetupProdCidb(self):
    """Sets up CIDB to use the prod instance of the database.

    May be called only once, and may not be called after any other CIDB Setup
    method, otherwise it will raise an AssertionError.
    """
    self.Setup(CONNECTION_TYPE_PROD)


  def SetupDebugCidb(self):
    """Sets up CIDB to use the debug instance of the database.

    May be called only once, and may not be called after any other CIDB Setup
    method, otherwise it will raise an AssertionError.
    """
    self.Setup(CONNECTION_TYPE_DEBUG)

  def SetupMockCidb(self, mock_cidb=None):
    """Sets up CIDB to use a mock object. May be called more than once.

    Args:
      mock_cidb: (optional) The mock cidb object to be returned by
                 GetCIDBConnection. If not supplied, then CIDB will be
                 considered not set up, but future calls to set up a
                 non-(mock or None) connection will fail.
    """
    self.Setup(CONNECTION_TYPE_MOCK, mock_cidb)

  def SetupNoCidb(self):
    """Sets up CIDB to use an explicit None connection.

    May be called more than once, or after SetupMockCidb.
    """
    self.Setup(CONNECTION_TYPE_NONE)

  def ClearMock(self):
    """Clear a mock CIDB object.

    This method clears a cidb mock object, but leaves the connection factory
    in _CONNECTION_TYPE_MOCK, so that future calls to set up a non-mock
    cidb will fail.
    """
    self.SetupMockCidb()

  def GetCIDBConnectionForBuilder(self):
    """Get a CIDBConnection.

    A call to one of the CIDB Setup methods must have been made before calling
    this factory method.

    Returns:
      A CIDBConnection instance connected to either the prod or debug
      instance of the database, or a mock connection, depending on which
      Setup method was called. Returns None if CIDB has been explicitly
      set up for that using SetupNoCidb.
    """
    return self.GetInstance()

  def _ClearCIDBSetup(self):
    """Clears the CIDB Setup state. For testing purposes only."""
    self._clear_setup()

CIDBConnectionFactory = CIDBConnectionFactoryClass()
