// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

#include <mach/mach_host.h>

#include "base/mac/scoped_mach_port.h"

namespace extensions {

bool CpuInfoProvider::QueryCpuTimePerProcessor(std::vector<CpuTime>* times) {
  natural_t num_of_processors;
  base::mac::ScopedMachPort host(mach_host_self());
  mach_msg_type_number_t type;
  processor_cpu_load_info_data_t* cpu_infos;

  if (host_processor_info(host.get(),
                          PROCESSOR_CPU_LOAD_INFO,
                          &num_of_processors,
                          reinterpret_cast<processor_info_array_t*>(&cpu_infos),
                          &type) == KERN_SUCCESS) {
    std::vector<CpuTime> results;
    int64 user = 0, nice = 0, sys = 0, idle = 0;

    for (natural_t i = 0; i < num_of_processors; ++i) {
      CpuTime time;

      user = static_cast<int64>(cpu_infos[i].cpu_ticks[CPU_STATE_USER]);
      sys  = static_cast<int64>(cpu_infos[i].cpu_ticks[CPU_STATE_SYSTEM]);
      nice = static_cast<int64>(cpu_infos[i].cpu_ticks[CPU_STATE_NICE]);
      idle = static_cast<int64>(cpu_infos[i].cpu_ticks[CPU_STATE_IDLE]);

      time.kernel = sys;
      time.user = user + nice;
      time.idle = idle;
      results.push_back(time);
    }

    vm_deallocate(mach_task_self(),
                  reinterpret_cast<vm_address_t>(cpu_infos),
                  num_of_processors * sizeof(processor_cpu_load_info));

    times->swap(results);
    return true;
  }

  return false;
}

}  // namespace extensions
