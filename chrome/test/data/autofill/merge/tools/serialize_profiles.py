# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sqlite3
import sys

from autofill_merge_common import SerializeProfiles, ColumnNameToFieldType

def main():
  """Serializes the autofill_profiles table from the specified database."""

  if len(sys.argv) != 2:
    print "Usage: python serialize_profiles.py <path/to/database>"
    return

  database = sys.argv[1]
  if not os.path.isfile(database):
    print "Cannot read database at \"%s\"" % database
    return

  try:
    connection = sqlite3.connect(database, 0)
    cursor = connection.cursor()
    cursor.execute("SELECT * from autofill_profiles;")
  except sqlite3.OperationalError:
    print "Failed to read the autofill_profiles table from \"%s\"" % database
    raise

  # For backward-compatibility, the result of |cursor.description| is a list of
  # 7-tuples, in which the first item is the column name, and the remaining
  # items are 'None'.
  types = [ColumnNameToFieldType(item[0]) for item in cursor.description]
  profiles = [zip(types, profile) for profile in cursor]

  print SerializeProfiles(profiles)


if __name__ == '__main__':
  main()