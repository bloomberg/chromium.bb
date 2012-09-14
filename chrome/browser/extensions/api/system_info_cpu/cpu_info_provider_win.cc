// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

#include <windows.h>
#include <winternl.h>

#include "base/sys_info.h"

namespace extensions {

namespace {

const wchar_t kNtdll[] = L"ntdll.dll";
const char kNtQuerySystemInformationName[] = "NtQuerySystemInformation";

// See MSDN about NtQuerySystemInformation definition.
typedef DWORD (WINAPI *NtQuerySystemInformationPF)(DWORD system_info_class,
                                                   PVOID system_info,
                                                   ULONG system_info_length,
                                                   PULONG return_length);

}  // namespace

bool CpuInfoProvider::QueryCpuTimePerProcessor(std::vector<CpuTime>* times) {
  std::vector<CpuTime> results;

  HMODULE ntdll = GetModuleHandle(kNtdll);
  CHECK(ntdll != NULL);
  NtQuerySystemInformationPF NtQuerySystemInformation =
      reinterpret_cast<NtQuerySystemInformationPF>(
          ::GetProcAddress(ntdll, kNtQuerySystemInformationName));

  CHECK(NtQuerySystemInformation != NULL);

  int num_of_processors = base::SysInfo::NumberOfProcessors();
  scoped_array<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> processor_info(
      new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[num_of_processors]);

  ULONG returned_bytes = 0, bytes =
      sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * num_of_processors;
  if (!NT_SUCCESS(NtQuerySystemInformation(
          SystemProcessorPerformanceInformation,
          processor_info.get(), bytes, &returned_bytes)))
    return false;

  int returned_num_of_processors =
      returned_bytes / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);

  if (returned_num_of_processors != num_of_processors)
    return false;

  results.reserve(returned_num_of_processors);
  for (int i = 0; i < returned_num_of_processors; ++i) {
    CpuTime time;
    time.kernel = processor_info[i].KernelTime.QuadPart;
    time.user = processor_info[i].UserTime.QuadPart;
    time.idle = processor_info[i].IdleTime.QuadPart;
    results.push_back(time);
  }

  times->swap(results);
  return true;
}

}  // namespace extensions
