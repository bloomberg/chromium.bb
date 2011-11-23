#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a simple helper script to shut down the Chrome Frame helper process.

Requires Python Win32 extensions.
"""

import pywintypes
import sys
import win32gui
import win32con


def main():
  exit_code = 0
  window = win32gui.FindWindow('ChromeFrameHelperWindowClass',
                               'ChromeFrameHelperWindowName')
  if not window:
    print 'Chrome Frame helper process not running.'
  else:
    try:
      win32gui.PostMessage(window, win32con.WM_CLOSE, 0, 0)
      print 'Chrome Frame helper process shut down.'
    except pywintypes.error as ex:
      print 'Failed to shutdown Chrome Frame helper process: '
      print ex
      exit_code = 1
  return exit_code


if __name__ == '__main__':
  sys.exit(main())
