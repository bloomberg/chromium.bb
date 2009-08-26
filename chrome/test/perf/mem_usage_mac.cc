// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/mem_usage.h"

#include <mach/mach.h>

#include <vector>

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/test/chrome_process_util.h"

bool GetMemoryInfo(uint32 process_id,
                   size_t* peak_virtual_size,
                   size_t* current_virtual_size,
                   size_t* peak_working_set_size,
                   size_t* current_working_set_size) {
  DCHECK(current_virtual_size && current_working_set_size);

  // Mac doesn't support retrieving peak sizes.
  if (peak_virtual_size)
    *peak_virtual_size = 0;
  if (peak_working_set_size)
    *peak_working_set_size = 0;

  std::vector<base::ProcessId> processes;
  processes.push_back(process_id);

  MacChromeProcessInfoList process_info = GetRunningMacProcessInfo(processes);
  if (process_info.empty())
    return false;

  bool found_process = false;
  *current_virtual_size = 0;
  *current_working_set_size = 0;

  MacChromeProcessInfoList::iterator it = process_info.begin();
  for (; it != process_info.end(); ++it) {
    if (it->pid != static_cast<base::ProcessId>(process_id))
      continue;
    found_process = true;
    *current_virtual_size = it->vsz_in_kb * 1024;
    *current_working_set_size = it->rsz_in_kb * 1024;
    break;
  }

  return found_process;
}

// Bytes committed by the system.
size_t GetSystemCommitCharge() {
  host_name_port_t host = mach_host_self();
  mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
  vm_statistics_data_t data;
  kern_return_t kr = host_statistics(host, HOST_VM_INFO,
                                     reinterpret_cast<host_info_t>(&data),
                                     &count);
  if (kr)
    return 0;

  vm_size_t page_size;
  kr = host_page_size(host, &page_size);
  if (kr)
    return 0;

  return data.active_count * page_size;
}

void PrintChromeMemoryUsageInfo() {
  NOTIMPLEMENTED();
}

