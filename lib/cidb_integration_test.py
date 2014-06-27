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


class CIDBMigrationsTest(cros_test_lib.TestCase):
  """Tests that migration scripts run without error."""
  def testMigrations(self):
    # Connect to database and drop its contents.
    db = cidb.CIDBConnection()
    db.DropDatabase()

    # Connect to now fresh database and apply migrations.
    db = cidb.CIDBConnection()
    db.ApplySchemaMigrations()


# TODO(akeshet): Allow command line args to specify alternate CIDB instance
# for testing.
if __name__ == '__main__':
  cros_test_lib.main()
