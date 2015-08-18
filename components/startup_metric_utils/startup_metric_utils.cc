// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/startup_metric_utils.h"

#include "base/containers/hash_tables.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"

#if defined(OS_WIN)
#include <winternl.h>
#include "base/win/windows_version.h"
#endif

namespace {

// Mark as volatile to defensively make sure usage is thread-safe.
// Note that at the time of this writing, access is only on the UI thread.
volatile bool g_non_browser_ui_displayed = false;

base::Time* MainEntryPointTimeInternal() {
  static base::Time main_start_time = base::Time::Now();
  return &main_start_time;
}

#if defined(OS_WIN)

// The struct used to return system process information via the NT internal
// QuerySystemInformation call. This is partially documented at
// http://goo.gl/Ja9MrH and fully documented at http://goo.gl/QJ70rn
// This structure is laid out in the same format on both 32-bit and 64-bit
// systems, but has a different size due to the various pointer-sized fields.
struct SYSTEM_PROCESS_INFORMATION_EX {
  ULONG NextEntryOffset;
  ULONG NumberOfThreads;
  LARGE_INTEGER WorkingSetPrivateSize;
  ULONG HardFaultCount;
  BYTE Reserved1[36];
  PVOID Reserved2[3];
  // This is labeled a handle so that it expands to the correct size for 32-bit
  // and 64-bit operating systems. However, under the hood it's a 32-bit DWORD
  // containing the process ID.
  HANDLE UniqueProcessId;
  PVOID Reserved3;
  ULONG HandleCount;
  BYTE Reserved4[4];
  PVOID Reserved5[11];
  SIZE_T PeakPagefileUsage;
  SIZE_T PrivatePageCount;
  LARGE_INTEGER Reserved6[6];
  // Array of SYSTEM_THREAD_INFORMATION structs follows.
};

// The signature of the NtQuerySystemInformation function.
typedef NTSTATUS (WINAPI *NtQuerySystemInformationPtr)(
    SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

// Gets the hard fault count of the current process, returning it via
// |hard_fault_count|. Returns true on success, false otherwise. Also returns
// whether or not the system call was even possible for the current OS version
// via |has_os_support|.
bool GetHardFaultCountForCurrentProcess(uint32_t* hard_fault_count,
                                        bool* has_os_support) {
  DCHECK(hard_fault_count);
  DCHECK(has_os_support);

  if (base::win::GetVersion() < base::win::VERSION_WIN7) {
    *has_os_support = false;
    return false;
  }
  // At this point the OS supports the required system call.
  *has_os_support = true;

  // Get the function pointer.
  NtQuerySystemInformationPtr query_sys_info =
      reinterpret_cast<NtQuerySystemInformationPtr>(
          ::GetProcAddress(GetModuleHandle(L"ntdll.dll"),
                           "NtQuerySystemInformation"));
  if (query_sys_info == nullptr)
    return false;

  // The output of this system call depends on the number of threads and
  // processes on the entire system, and this can change between calls. Retry
  // a small handful of times growing the buffer along the way.
  // NOTE: The actual required size depends entirely on the number of processes
  //       and threads running on the system. The initial guess suffices for
  //       ~100s of processes and ~1000s of threads.
  std::vector<uint8_t> buffer(32 * 1024);
  for (size_t tries = 0; tries < 3; ++tries) {
    ULONG return_length = 0;
    NTSTATUS status = query_sys_info(
        SystemProcessInformation,
        buffer.data(),
        static_cast<ULONG>(buffer.size()),
        &return_length);
    // Insufficient space in the buffer.
    if (return_length > buffer.size()) {
      buffer.resize(return_length);
      continue;
    }
    if (NT_SUCCESS(status) && return_length <= buffer.size())
      break;
    return false;
  }

  // Look for the struct housing information for the current process.
  DWORD proc_id = ::GetCurrentProcessId();
  size_t index = 0;
  while (index < buffer.size()) {
    DCHECK_LE(index + sizeof(SYSTEM_PROCESS_INFORMATION_EX), buffer.size());
    SYSTEM_PROCESS_INFORMATION_EX* proc_info =
        reinterpret_cast<SYSTEM_PROCESS_INFORMATION_EX*>(buffer.data() + index);
    if (reinterpret_cast<DWORD>(proc_info->UniqueProcessId) == proc_id) {
      *hard_fault_count = proc_info->HardFaultCount;
      return true;
    }
    // The list ends when NextEntryOffset is zero. This also prevents busy
    // looping if the data is in fact invalid.
    if (proc_info->NextEntryOffset <= 0)
      return false;
    index += proc_info->NextEntryOffset;
  }

  return false;
}

#endif  // defined(OS_WIN)

// On Windows, records the number of hard-faults that have occurred in the
// current chrome.exe process since it was started. This is a nop on other
// platforms.
// crbug.com/476923
// TODO(chrisha): If this proves useful, use it to split startup stats in two.
void RecordHardFaultHistogram(bool is_first_run) {
#if defined(OS_WIN)
  uint32_t hard_fault_count = 0;
  bool has_os_support = false;
  bool success = GetHardFaultCountForCurrentProcess(
      &hard_fault_count, &has_os_support);

  // Log whether or not the system call was successful, assuming the OS was
  // detected to support it.
  if (has_os_support) {
    UMA_HISTOGRAM_BOOLEAN(
        "Startup.BrowserMessageLoopStartHardFaultCount.Success",
        success);
  }

  // Don't log a histogram value if unable to get the hard fault count.
  if (!success)
    return;

  // Hard fault counts are expected to be in the thousands range,
  // corresponding to faulting in ~10s of MBs of code ~10s of KBs at a time.
  // (Observed to vary from 1000 to 10000 on various test machines and
  // platforms.)
  if (is_first_run) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Startup.BrowserMessageLoopStartHardFaultCount.FirstRun",
        hard_fault_count,
        0, 40000, 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Startup.BrowserMessageLoopStartHardFaultCount",
        hard_fault_count,
        0, 40000, 50);
  }
#endif  // defined(OS_WIN)
}

// Record time of main entry so it can be read from Telemetry performance
// tests.
// TODO(jeremy): Remove once crbug.com/317481 is fixed.
void RecordMainEntryTimeHistogram() {
  const int kLowWordMask = 0xFFFFFFFF;
  const int kLower31BitsMask = 0x7FFFFFFF;
  base::TimeDelta browser_main_entry_time_absolute =
      *MainEntryPointTimeInternal() - base::Time::UnixEpoch();

  uint64 browser_main_entry_time_raw_ms =
      browser_main_entry_time_absolute.InMilliseconds();

  base::TimeDelta browser_main_entry_time_raw_ms_high_word =
      base::TimeDelta::FromMilliseconds(
          (browser_main_entry_time_raw_ms >> 32) & kLowWordMask);
  // Shift by one because histograms only support non-negative values.
  base::TimeDelta browser_main_entry_time_raw_ms_low_word =
      base::TimeDelta::FromMilliseconds(
          (browser_main_entry_time_raw_ms >> 1) & kLower31BitsMask);

  // A timestamp is a 64 bit value, yet histograms can only store 32 bits.
  LOCAL_HISTOGRAM_TIMES("Startup.BrowserMainEntryTimeAbsoluteHighWord",
      browser_main_entry_time_raw_ms_high_word);
  LOCAL_HISTOGRAM_TIMES("Startup.BrowserMainEntryTimeAbsoluteLowWord",
      browser_main_entry_time_raw_ms_low_word);
}

bool g_main_entry_time_was_recorded = false;

// Environment variable that stores the timestamp when the executable's main()
// function was entered.
const char kChromeMainTimeEnvVar[] = "CHROME_MAIN_TIME";

}  // namespace

namespace startup_metric_utils {

bool WasNonBrowserUIDisplayed() {
  return g_non_browser_ui_displayed;
}

void SetNonBrowserUIDisplayed() {
  g_non_browser_ui_displayed = true;
}

void RecordMainEntryPointTime() {
  DCHECK(!g_main_entry_time_was_recorded);
  g_main_entry_time_was_recorded = true;
  MainEntryPointTimeInternal();
}

void RecordExeMainEntryTime() {
  std::string exe_load_time =
      base::Int64ToString(base::Time::Now().ToInternalValue());
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(kChromeMainTimeEnvVar, exe_load_time);
}

void RecordSavedMainEntryPointTime(const base::Time& entry_point_time) {
  DCHECK(!g_main_entry_time_was_recorded);
  g_main_entry_time_was_recorded = true;
  *MainEntryPointTimeInternal() = entry_point_time;
}

// Return the time recorded by RecordMainEntryPointTime().
const base::Time MainEntryStartTime() {
  DCHECK(g_main_entry_time_was_recorded);
  return *MainEntryPointTimeInternal();
}

void OnBrowserStartupComplete(bool is_first_run) {
  RecordHardFaultHistogram(is_first_run);
  RecordMainEntryTimeHistogram();

  // Bail if uptime < 7 minutes, to filter out cases where Chrome may have been
  // autostarted and the machine is under io pressure.
  const int64 kSevenMinutesInMilliseconds =
      base::TimeDelta::FromMinutes(7).InMilliseconds();
  if (base::SysInfo::Uptime() < kSevenMinutesInMilliseconds)
    return;

  // The Startup.BrowserMessageLoopStartTime histogram recorded in
  // chrome_browser_main.cc exhibits instability in the field which limits its
  // usefulness in all scenarios except when we have a very large sample size.
  // Attempt to mitigate this with a new metric:
  // * Measure time from main entry rather than the OS' notion of process start
  //   time.
  // * Only measure launches that occur 7 minutes after boot to try to avoid
  //   cases where Chrome is auto-started and IO is heavily loaded.
  base::TimeDelta startup_time_from_main_entry =
      base::Time::Now() - MainEntryStartTime();
  if (is_first_run) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Startup.BrowserMessageLoopStartTimeFromMainEntry.FirstRun",
        startup_time_from_main_entry);
  } else {
    UMA_HISTOGRAM_LONG_TIMES(
        "Startup.BrowserMessageLoopStartTimeFromMainEntry",
        startup_time_from_main_entry);
  }

// CurrentProcessInfo::CreationTime() is currently only implemented on some
// platforms.
#if (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN) || \
    defined(OS_LINUX)
  // Record timings between process creation, the main() in the executable being
  // reached and the main() in the shared library being reached.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string chrome_main_entry_time_string;
  if (env->GetVar(kChromeMainTimeEnvVar, &chrome_main_entry_time_string)) {
    // The time that the Chrome executable's main() function was entered.
    int64 chrome_main_entry_time_int = 0;
    if (base::StringToInt64(chrome_main_entry_time_string,
                            &chrome_main_entry_time_int)) {
      base::Time process_create_time = base::CurrentProcessInfo::CreationTime();
      base::Time exe_main_time =
          base::Time::FromInternalValue(chrome_main_entry_time_int);
      base::Time dll_main_time = MainEntryStartTime();

      // Process create to chrome.exe:main().
      UMA_HISTOGRAM_LONG_TIMES("Startup.LoadTime.ProcessCreateToExeMain",
                               exe_main_time - process_create_time);

      // chrome.exe:main() to chrome.dll:main().
      UMA_HISTOGRAM_LONG_TIMES("Startup.LoadTime.ExeMainToDllMain",
                               dll_main_time - exe_main_time);

      // Process create to chrome.dll:main().
      UMA_HISTOGRAM_LONG_TIMES("Startup.LoadTime.ProcessCreateToDllMain",
                               dll_main_time - process_create_time);
    }
  }
#endif
}

const base::Time* MainEntryPointTime() {
  if (!g_main_entry_time_was_recorded)
    return NULL;
  return MainEntryPointTimeInternal();
}

}  // namespace startup_metric_utils
