# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Quits Chrome.

This script sends a WM_CLOSE message to each window of Chrome and waits until
the process terminates.
"""

import optparse
import pywintypes
import sys
import time
import win32con
import win32gui
import winerror

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
      try:
        win32gui.PostMessage(hwnd, win32con.WM_CLOSE, 0, 0)
      except pywintypes.error as error:
        # It's normal that some window handles have become invalid.
        if error.args[0] != winerror.ERROR_INVALID_WINDOW_HANDLE:
          raise
    time.sleep(0.1)
  return False


def main():
  usage = 'usage: %prog chrome_path'
  parser = optparse.OptionParser(usage, description='Quit Chrome.')
  _, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Incorrect number of arguments.')
  chrome_path = args[0]

  if not CloseWindows(chrome_path):
    raise Exception('Could not quit Chrome.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
