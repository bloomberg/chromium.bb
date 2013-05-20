// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/os_resource_win.h"

namespace task_manager {

void GetWinGDIHandles(base::ProcessHandle process,
                      size_t* current,
                      size_t* peak) {
  *current = 0;
  *peak = 0;
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, process, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    *current = GetGuiResources(process_with_query_rights, GR_GDIOBJECTS);
    *peak = GetGuiResources(process_with_query_rights, GR_GDIOBJECTS_PEAK);
    CloseHandle(process_with_query_rights);
  }
}

void GetWinUSERHandles(base::ProcessHandle process,
                       size_t* current,
                       size_t* peak) {
  *current = 0;
  *peak = 0;
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, process, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    *current = GetGuiResources(process_with_query_rights, GR_USEROBJECTS);
    *peak = GetGuiResources(process_with_query_rights, GR_USEROBJECTS_PEAK);
    CloseHandle(process_with_query_rights);
  }
}

}  // namespace task_manager
