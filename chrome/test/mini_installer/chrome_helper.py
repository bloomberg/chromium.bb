# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common helper module for working with Chrome's processes and windows."""

import psutil
import re
import win32gui
import win32process

import path_resolver


def GetProcessIDs(process_path):
  """Returns a list of IDs of processes whose path is |process_path|.

  Args:
    process_path: The path to the process.

  Returns:
    A list of process IDs.
  """
  process_ids = []
  for process in psutil.process_iter():
    try:
      found_process_path = process.exe
      if found_process_path == process_path:
        process_ids.append(process.pid)
    except psutil.AccessDenied:
      # It's normal that some processes are not accessible.
      pass
  return process_ids


def GetWindowHandles(process_ids):
  """Returns a list of handles of windows owned by processes in |process_ids|.

  Args:
    process_ids: A list of process IDs.

  Returns:
    A list of handles of windows owned by processes in |process_ids|.
  """
  hwnds = []
  def EnumerateWindowCallback(hwnd, _):
    _, found_process_id = win32process.GetWindowThreadProcessId(hwnd)
    if found_process_id in process_ids and win32gui.IsWindowVisible(hwnd):
      hwnds.append(hwnd)
  # Enumerate all the top-level windows and call the callback with the hwnd as
  # the first parameter.
  win32gui.EnumWindows(EnumerateWindowCallback, None)
  return hwnds


def WindowExists(process_ids, class_pattern):
  """Returns whether there exists a window with the specified criteria.

  This method returns whether there exists a window that is owned by a process
  in |process_ids| and has a class name that matches |class_pattern|.

  Args:
    process_ids: A list of process IDs.
    class_pattern: The regular expression pattern of the window class name.

  Returns:
    A boolean indicating whether such window exists.
  """
  for hwnd in GetWindowHandles(process_ids):
    if re.match(class_pattern, win32gui.GetClassName(hwnd)):
      return True
  return False


def GetChromePath(system_level):
  """Returns the path to Chrome, at the |system_level| or user level."""
  chrome_path = None
  if system_level:
    chrome_path = '$PROGRAM_FILES\\$CHROME_DIR\\Application\\chrome.exe'
  else:
    chrome_path = '$LOCAL_APPDATA\\$CHROME_DIR\\Application\\chrome.exe'
  return path_resolver.ResolvePath(chrome_path)
