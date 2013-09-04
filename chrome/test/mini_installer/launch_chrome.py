# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Launches Chrome.

This script launches Chrome and waits until its window shows up.
"""

import optparse
import subprocess
import sys
import time

import chrome_helper


def WaitForWindow(process_id, class_pattern):
  """Waits until a window specified by |process_id| and class name shows up.

  Args:
    process_id: The ID of the process that owns the window.
    class_pattern: The regular expression pattern of the window class name.

  Returns:
    A boolean value indicating whether the specified window shows up within
    30 seconds.
  """
  start_time = time.time()
  while time.time() - start_time < 30:
    if chrome_helper.WindowExists([process_id], class_pattern):
      return True
    time.sleep(0)
  return False


def main():
  usage = 'usage: %prog chrome_path'
  parser = optparse.OptionParser(usage, description='Launch Chrome.')
  _, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Incorrect number of arguments.')
  chrome_path = args[0]

  process = subprocess.Popen(chrome_path)
  if not WaitForWindow(process.pid, 'Chrome_WidgetWin_'):
    raise Exception('Could not launch Chrome.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
