// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/mem_usage.h"

#include <mach/mach.h>

#include <vector>

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/test/chrome_process_util.h"

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

