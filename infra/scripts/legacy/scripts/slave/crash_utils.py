#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility for crash reporting."""

import os
import time

from common import chromium_utils


def print_new_crash_files(new_crash_files):
  """Prints all the new crash files."""
  if new_crash_files:
    print '\nFound %d new crash file(s), dumping them:' % (
        len(new_crash_files))
    for crash_file in new_crash_files:
      print 'File: ' + crash_file
      print '=' * (6 + len(crash_file))
      for crash_line in open(crash_file):
        print '  ' + crash_line.rstrip()
        print ''


def list_crash_logs():
  """List all the crash files stored in the user directory."""
  reports_dir = os.path.expanduser('~/Library/Logs/DiagnosticReports')
  result = [x for x in chromium_utils.LocateFiles('*.crash', reports_dir)]
  return result


def wait_for_crash_logs():
  """Sleeps for a while to allow the crash logs to be written.

  The crash reporter runs asynchronously out of process, so when a unittest
  crashes nothing says the crash log is written immediately. This method is a
  hack to allow time for the crash logs to be written. Ninety seconds is picked
  from looking at data on the bots."""
  # TODO(lakshya): Optimize by polling every 10 seconds for a crash log to be
  # available instead of waiting for 90 seconds.
  print ('\nNote: Test finished with non zero status, sleeping for 90s to '
         'allow crash files to be written.')
  time.sleep(90)

