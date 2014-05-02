#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Records or runs web-page-replay data for InstantExtendedManualTests.*.
Typical usage is:

 $ cd src/chrome/test/data/search/tools/
 $ sudo instant_extended_manual_tests.py record \
     ../../../../../third_party/webpagereplay/replay.py \
     ../../../../../out/Debug/interactive_ui_tests \
     ../replay/archive.wpr

This will create the archive.wpr file against the lastest google.com GWS.
The tests then can be isolated from the live site by replaying this captured
session on the bots.

This archive.wpr file should be re-recorded and checked into the repo whenever
new manual tests are created.
"""

import signal
import subprocess
import sys

def Usage():
  print 'Usage: sudo python instant_extended_manual_tests.py (record|run) \\'
  print '         <path/to/replay> <path/to/ui_tests> <path/to/data>'
  return 1

def ReplayTests(replay_path, test_path, data_path, arg=None):
  # Start up web-page-replay.
  if not arg:
    p = subprocess.Popen([replay_path, data_path])
  else:
    p = subprocess.Popen([replay_path, arg, data_path])

  # Run the tests within the mock server.
  return_value = subprocess.call(
    [test_path,
     '--gtest_filter=InstantExtendedManualTest.*',
     '--run-manual',
     '--enable-benchmarking',
     '--enable-stats-table',
     '--ignore-certificate-errors'])

  # Shut down web-page-replay and save the recorded session to |data_path|.
  p.send_signal(signal.SIGINT)
  p.wait()
  return return_value

def main():
  if len(sys.argv) != 5:
    return Usage()

  if sys.argv[1] == 'record':
    return ReplayTests(sys.argv[2], sys.argv[3], sys.argv[4], '--record')

  if sys.argv[1] == 'run':
    return ReplayTests(sys.argv[2], sys.argv[3], sys.argv[4])

  return Usage()

if __name__ == '__main__':
  sys.exit(main())
