# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Continuous Integration Database Library."""

import glob
import logging
import os
import re
import sqlalchemy
import sqlalchemy.exc
from sqlalchemy import MetaData

from chromite.cbuildbot import constants

TEST_DB_CREDENTIALS_DIR = os.path.join(constants.SOURCE_ROOT,
                                       'crostools', 'cidb',
                                       'test_credentials')

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
    def wrapper(self, *args):
      if self.schema_version < min_version:
        raise UnsupportedMethodException()
      return f(self, *args)
    return wrapper
  return decorator


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

    connect_string = 'mysql://%s:%s@%s' % (user, password, host)

    # Create a temporary engine to connect to the mysql instance, and check if
    # a database named |db_name| exists. If not, create one. We use a temporary
    # engine here because the real engine will be opened with a default
    # database name given by |db_name|.
    temp_engine = sqlalchemy.create_engine(connect_string,
                                           connect_args=self._ssl_args)
    databases = temp_engine.execute('SHOW DATABASES').fetchall()
    if (db_name,) not in databases:
      temp_engine.execute('CREATE DATABASE %s' % db_name)
      logging.info('Created database %s', db_name)

    temp_engine.dispose()

    # Now create the persistent connection to the database named |db_name|.
    # If there is a schema version table, read the current schema version
    # from it. Otherwise, assume schema_version 0.
    self._connect_string = '%s/%s' % (connect_string, db_name)

    self.schema_version = self.QuerySchemaVersion()

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

  def ApplySchemaMigrations(self, maxVersion=None):
    """Apply pending migration scripts to database, in order.

    Args:
      maxVersion: The highest version migration script to apply. If
                  unspecified, all migrations found will be applied.
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
    key_columns = t.primary_key.columns.values()
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
        logging.debug('Retrying a query on engine %s, due to dropped '
                      'connection.', self._GetEngine())
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
      e = sqlalchemy.create_engine(self._connect_string,
                                   connect_args=self._ssl_args)
      self._engine = e
      self._engine_pid = pid
      logging.debug('Created engine %s for pid %s', e, pid)
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
  def __init__(self):
    super(CIDBConnection, self).__init__('cidb', CIDB_MIGRATIONS_DIR,
                                         TEST_DB_CREDENTIALS_DIR)

  @minimum_schema(1)
  def TestMethodSchemaTooLow(self):
    """This method is a temporary one to test the minimum_schema decorator."""

  @minimum_schema(0)
  def TestMethodSchemaOK(self):
    """This method is a temporary one to test the minimum_schema decorator."""

