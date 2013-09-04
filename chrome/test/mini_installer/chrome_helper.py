# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common helper module for working with Chrome's processes and windows."""

import ctypes
import pywintypes
import re
import win32con
import win32gui
import win32process


def GetProcessIDAndPathPairs():
  """Returns a list of 2-tuples of (process id, process path).

  This is needed because psutil is not available on Windows slave machines (see:
  http://crbug.com/257696).
  TODO(sukolsak): Use psutil.process_iter() once it becomes available.
  """
  process_id_and_path_pairs = []
  for process_id in win32process.EnumProcesses():
    process_handle = ctypes.windll.kernel32.OpenProcess(
        win32con.PROCESS_QUERY_INFORMATION | win32con.PROCESS_VM_READ, False,
        process_id)
    if not process_handle:
      continue
    try:
      process_path = win32process.GetModuleFileNameEx(process_handle, 0)
      process_id_and_path_pairs.append((process_id, process_path))
    except pywintypes.error:
      # It's normal that some processes are not accessible.
      pass
  return process_id_and_path_pairs


def GetProcessIDs(process_path):
  """Returns a list of IDs of processes whose path is |process_path|.

  Args:
    process_path: The path to the process.

  Returns:
    A list of process IDs.
  """
  return [pid for (pid, path) in GetProcessIDAndPathPairs() if
          path == process_path]


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
