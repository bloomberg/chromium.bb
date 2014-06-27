# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Continuous Integration Database Library."""

import glob
import logging
import os
import re
import sqlalchemy

from chromite.cbuildbot import constants

TEST_DB_CREDENTIALS_DIR = os.path.join(constants.SOURCE_ROOT,
                                       'crostools', 'cidb',
                                       'test_credentials')

CIDB_MIGRATIONS_DIR = os.path.join(constants.CHROMITE_DIR, 'cidb',
                                   'migrations')

class DBException(Exception):
  """General exception class for this module."""


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
    ssl_args = {'ssl': {'cert': cert, 'key': key, 'ca': ca}}

    connect_string = 'mysql://%s:%s@%s' % (user, password, host)

    # Create a temporary engine to connect to the mysql instance, and check if
    # a database named |db_name| exists. If not, create one.
    temp_engine = sqlalchemy.create_engine(connect_string,
                                           connect_args=ssl_args)
    databases = temp_engine.execute('SHOW DATABASES').fetchall()
    if (db_name,) not in databases:
      temp_engine.execute('CREATE DATABASE %s' % db_name)
      logging.info('Created database %s', db_name)

    temp_engine.dispose()

    # Now create the persistent connection to the database named |db_name|.
    # If there is a schema version table, read the current schema version
    # from it. Otherwise, assume schema_version 0.
    connect_string = '%s/%s' % (connect_string, db_name)

    self.engine = sqlalchemy.create_engine(connect_string,
                                           connect_args=ssl_args)

    self.schema_version = self.QuerySchemaVersion()

  def DropDatabase(self):
    """Delete all data and tables from database, and drop database.

    Use with caution. All data in database will be deleted. Invalidates
    this database connection instance.
    """
    self.engine.execute('DROP DATABASE %s' % self.db_name)
    self.engine.dispose()

  def QuerySchemaVersion(self):
    """Query the database for its current schema version number.

    Returns:
      The current schema version from the database's schema version table,
      as an integer, or 0 if the table is empty or nonexistent.
    """
    tables = self.engine.execute('SHOW TABLES').fetchall()
    if (self.SCHEMA_VERSION_TABLE_NAME,) in tables:
      r = self.engine.execute('SELECT MAX(%s) from %s' %
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
      self.engine.execute(q)


class CIDBConnection(SchemaVersionedMySQLConnection):
  """Connection to a Continuous Integration database."""
  def __init__(self):
    super(CIDBConnection, self).__init__('cidb', CIDB_MIGRATIONS_DIR,
                                         TEST_DB_CREDENTIALS_DIR)

