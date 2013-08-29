# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Quits Chrome.

This script sends a WM_CLOSE message to each window of Chrome and waits until
the process terminates.
"""

import optparse
import sys
import time
import win32con
import win32gui

import chrome_helper


def CloseWindows(process_path):
  """Closes all windows owned by processes whose path is |process_path|.

  Args:
    process_path: The path to the process.

  Returns:
    A boolean indicating whether the processes successfully terminate within
    30 seconds.
  """
  start_time = time.time()
  while time.time() - start_time < 30:
    process_ids = chrome_helper.GetProcessIDs(process_path)
    if not process_ids:
      return True

    for hwnd in chrome_helper.GetWindowHandles(process_ids):
      win32gui.PostMessage(hwnd, win32con.WM_CLOSE, 0, 0)
    time.sleep(0)
  return False


def main():
  parser = optparse.OptionParser(description='Quit Chrome.')
  parser.add_option('--system-level', action='store_true', dest='system_level',
                    default=False, help='Quit Chrome at system level.')
  options, _ = parser.parse_args()
  chrome_path = chrome_helper.GetChromePath(options.system_level)
  if not CloseWindows(chrome_path):
    raise Exception('Could not quit Chrome.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
