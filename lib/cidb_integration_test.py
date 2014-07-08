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

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cidb
from chromite.lib import cros_test_lib


class CIDBIntegrationTest(cros_test_lib.TestCase):
  """Base class for cidb tests that connect to a test MySQL instance."""

  def _PrepareFreshDatabase(self, max_schema_version=None):
    """Create an empty database with migrations applied.

    Args:
      max_schema_version: The highest schema version migration to apply,
      defaults to None in which case all migrations will be applied.

    Returns:
      A CIDBConnection instance, connected to a an empty database.
    """
    # Connect to database and drop its contents.
    db = cidb.CIDBConnection()
    db.DropDatabase()

    # Connect to now fresh database and apply migrations.
    db = cidb.CIDBConnection()
    db.ApplySchemaMigrations(max_schema_version)

    return db

class CIDBMigrationsTest(CIDBIntegrationTest):
  """Test that all migrations apply correctly."""

  def testMigrations(self):
    """Test that all migrations apply correctly."""
    self._PrepareFreshDatabase()


class CIDBAPITest(CIDBIntegrationTest):
  """Tests of the CIDB API."""
  def testSchemaVersionTooLow(self):
    """Tests that the minimum_schema decorator works as expected."""
    db = self._PrepareFreshDatabase(0)
    self.assertRaises2(cidb.UnsupportedMethodException,
                       db.TestMethodSchemaTooLow)

  def testSchemaVersionOK(self):
    """Tests that the minimum_schema decorator works as expected."""
    db = self._PrepareFreshDatabase(0)
    db.TestMethodSchemaOK()


# TODO(akeshet): Allow command line args to specify alternate CIDB instance
# for testing.
if __name__ == '__main__':
  cros_test_lib.main()
