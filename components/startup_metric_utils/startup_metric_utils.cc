// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/startup_metric_utils.h"

#include "base/containers/hash_tables.h"
#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"

#if defined(OS_WIN)
#include <winternl.h>
#include "base/win/windows_version.h"
#endif

namespace startup_metric_utils {

namespace {

// Mark as volatile to defensively make sure usage is thread-safe.
// Note that at the time of this writing, access is only on the UI thread.
volatile bool g_non_browser_ui_displayed = false;

base::LazyInstance<base::Time>::Leaky g_process_creation_time =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::Time>::Leaky g_main_entry_point_time =
    LAZY_INSTANCE_INITIALIZER;

StartupTemperature g_startup_temperature = UNCERTAIN_STARTUP_TEMPERATURE;

#if defined(OS_WIN)

// These values are taken from the Startup.BrowserMessageLoopStartHardFaultCount
// histogram. If the cold start histogram starts looking strongly bimodal it may
// be because the binary/resource sizes have grown significantly larger than
// when these values were set. In this case the new values need to be chosen
// from the original histogram.
//
// Maximum number of hard faults tolerated for a startup to be classified as a
// warm start. Set at roughly the 40th percentile of the HardFaultCount
// histogram.
const uint32_t WARM_START_HARD_FAULT_COUNT_THRESHOLD = 5;
// Minimum number of hard faults expected for a startup to be classified as a
// cold start. Set at roughly the 60th percentile of the HardFaultCount
// histogram.
const uint32_t COLD_START_HARD_FAULT_COUNT_THRESHOLD = 1200;

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


// Helper macro for splitting out an UMA histogram based on cold or warm start.
// |type| is the histogram type, and corresponds to an UMA macro like
// UMA_HISTOGRAM_LONG_TIMES. It must be itself be a macro that only takes two
// parameters.
// |basename| is the basename of the histogram. A histogram of this name will
// always be recorded to. If the startup is either cold or warm then a value
// will also be recorded to the histogram with name |basename| and suffix
// ".ColdStart" or ".WarmStart", as appropriate.
// |value_expr| is an expression evaluating to the value to be recorded. This
// will be evaluated exactly once and cached, so side effects are not an issue.
#define UMA_HISTOGRAM_WITH_STARTUP_TEMPERATURE(type, basename, value_expr) \
  {                                                                        \
    const auto kValue = value_expr;                                        \
    /* Always record to the base histogram. */                             \
    type(basename, kValue);                                                \
    /* Record to the cold/warm suffixed histogram as appropriate. */       \
    if (g_startup_temperature == COLD_STARTUP_TEMPERATURE) {               \
      type(basename ".ColdStartup", kValue);                               \
    } else if (g_startup_temperature == WARM_STARTUP_TEMPERATURE) {        \
      type(basename ".WarmStartup", kValue);                               \
    }                                                                      \
  }

#define UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(type, basename,       \
                                                         begin_time, end_time) \
  {                                                                            \
    UMA_HISTOGRAM_WITH_STARTUP_TEMPERATURE(type, basename,                     \
                                           end_time - begin_time)              \
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(                                   \
        "startup", basename, 0,                                                \
        StartupTimeToTraceTicks(begin_time).ToInternalValue(), "Temperature",  \
        g_startup_temperature);                                                \
    TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP1(                                     \
        "startup", basename, 0,                                                \
        StartupTimeToTraceTicks(end_time).ToInternalValue(), "Temperature",    \
        g_startup_temperature);                                                \
  }

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

  // Determine the startup type based on the number of observed hard faults.
  DCHECK_EQ(UNCERTAIN_STARTUP_TEMPERATURE, g_startup_temperature);
  if (hard_fault_count < WARM_START_HARD_FAULT_COUNT_THRESHOLD) {
    g_startup_temperature = WARM_STARTUP_TEMPERATURE;
  } else if (hard_fault_count >= COLD_START_HARD_FAULT_COUNT_THRESHOLD) {
    g_startup_temperature = COLD_STARTUP_TEMPERATURE;
  }

  // Record the startup 'temperature'.
  UMA_HISTOGRAM_ENUMERATION(
      "Startup.Temperature", g_startup_temperature, STARTUP_TEMPERATURE_MAX);
#endif  // defined(OS_WIN)
}

// Converts a base::Time value to a base::TraceTicks value. The conversion isn't
// exact, but is within the time delta taken to synchronously resolve
// base::Time::Now() and base::TraceTicks::Now() which in practice is pretty
// much instant compared to multi-seconds startup timings.
// TODO(gab): Find a precise way to do this (http://crbug.com/544131).
base::TraceTicks StartupTimeToTraceTicks(const base::Time& time) {
  // First get a base which represents the same point in time in both units.
  // The wall clock time it takes to gather both of these is the precision of
  // this method.
  static const base::Time time_base = base::Time::Now();
  static const base::TraceTicks trace_ticks_base = base::TraceTicks::Now();

  // Then use the TimeDelta common ground between the two units to make the
  // conversion.
  const base::TimeDelta delta_since_base = time_base - time;
  return trace_ticks_base - delta_since_base;
}

// Record time of main entry so it can be read from Telemetry performance tests.
// TODO(jeremy): Remove once crbug.com/317481 is fixed.
void RecordMainEntryTimeHistogram() {
  const int kLowWordMask = 0xFFFFFFFF;
  const int kLower31BitsMask = 0x7FFFFFFF;
  DCHECK(!MainEntryPointTime().is_null());
  base::TimeDelta browser_main_entry_time_absolute =
      MainEntryPointTime() - base::Time::UnixEpoch();

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

// Environment variable that stores the timestamp when the executable's main()
// function was entered.
const char kChromeMainTimeEnvVar[] = "CHROME_MAIN_TIME";

// Returns the time of main entry recorded from RecordExeMainEntryTime.
base::Time ExeMainEntryPointTime() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string time_string;
  int64 time_int = 0;
  if (env->GetVar(kChromeMainTimeEnvVar, &time_string) &&
      base::StringToInt64(time_string, &time_int)) {
    return base::Time::FromInternalValue(time_int);
  }
  return base::Time();
}

}  // namespace

bool WasNonBrowserUIDisplayed() {
  return g_non_browser_ui_displayed;
}

void SetNonBrowserUIDisplayed() {
  g_non_browser_ui_displayed = true;
}

void RecordStartupProcessCreationTime(const base::Time& time) {
  DCHECK(g_process_creation_time.Get().is_null());
  g_process_creation_time.Get() = time;
  DCHECK(!g_process_creation_time.Get().is_null());
}

void RecordMainEntryPointTime(const base::Time& time) {
  DCHECK(MainEntryPointTime().is_null());
  g_main_entry_point_time.Get() = time;
  DCHECK(!MainEntryPointTime().is_null());
}

void RecordExeMainEntryPointTime(const base::Time& time) {
  std::string exe_load_time = base::Int64ToString(time.ToInternalValue());
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(kChromeMainTimeEnvVar, exe_load_time);
}

void RecordBrowserMainMessageLoopStart(const base::Time& time,
                                       bool is_first_run) {
  RecordHardFaultHistogram(is_first_run);
  RecordMainEntryTimeHistogram();

  const base::Time& process_creation_time = g_process_creation_time.Get();
  if (!is_first_run && !process_creation_time.is_null()) {
    UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES_100, "Startup.BrowserMessageLoopStartTime",
        process_creation_time, time);
  }

  // Bail if uptime < 7 minutes, to filter out cases where Chrome may have been
  // autostarted and the machine is under io pressure.
  const int64 kSevenMinutesInMilliseconds =
      base::TimeDelta::FromMinutes(7).InMilliseconds();
  if (base::SysInfo::Uptime() < kSevenMinutesInMilliseconds)
    return;

  // The Startup.BrowserMessageLoopStartTime histogram exhibits instability in
  // the field which limits its usefulness in all scenarios except when we have
  // a very large sample size. Attempt to mitigate this with a new metric:
  // * Measure time from main entry rather than the OS' notion of process start.
  // * Only measure launches that occur 7 minutes after boot to try to avoid
  //   cases where Chrome is auto-started and IO is heavily loaded.
  const base::Time dll_main_time = MainEntryPointTime();
  if (is_first_run) {
    UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES,
        "Startup.BrowserMessageLoopStartTimeFromMainEntry.FirstRun",
        dll_main_time, time);
  } else {
    UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES,
        "Startup.BrowserMessageLoopStartTimeFromMainEntry", dll_main_time,
        time);
  }

  // Record timings between process creation, the main() in the executable being
  // reached and the main() in the shared library being reached.
  if (!process_creation_time.is_null()) {
    const base::Time exe_main_time = ExeMainEntryPointTime();
    if (!exe_main_time.is_null()) {
      // Process create to chrome.exe:main().
      UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(
          UMA_HISTOGRAM_LONG_TIMES, "Startup.LoadTime.ProcessCreateToExeMain",
          process_creation_time, exe_main_time);

      // chrome.exe:main() to chrome.dll:main().
      UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(
          UMA_HISTOGRAM_LONG_TIMES, "Startup.LoadTime.ExeMainToDllMain",
          exe_main_time, dll_main_time);

      // Process create to chrome.dll:main(). Reported as a histogram only as
      // the other two events above are sufficient for tracing purposes.
      UMA_HISTOGRAM_WITH_STARTUP_TEMPERATURE(
          UMA_HISTOGRAM_LONG_TIMES, "Startup.LoadTime.ProcessCreateToDllMain",
          dll_main_time - process_creation_time);
    }
  }
}

void RecordBrowserWindowDisplay(const base::Time& time) {
  static bool is_first_call = true;
  if (!is_first_call || time.is_null())
    return;
  is_first_call = false;
  if (WasNonBrowserUIDisplayed() || g_process_creation_time.Get().is_null())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES, "Startup.BrowserWindowDisplay",
      g_process_creation_time.Get(), time);
}

void RecordBrowserOpenTabsDelta(const base::TimeDelta& delta) {
  static bool is_first_call = true;
  if (!is_first_call)
    return;
  is_first_call = false;

  UMA_HISTOGRAM_WITH_STARTUP_TEMPERATURE(UMA_HISTOGRAM_LONG_TIMES_100,
                                         "Startup.BrowserOpenTabs", delta);
}

void RecordFirstWebContentsMainFrameLoad(const base::Time& time) {
  static bool is_first_call = true;
  if (!is_first_call || time.is_null())
    return;
  is_first_call = false;
  if (WasNonBrowserUIDisplayed() || g_process_creation_time.Get().is_null())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES_100, "Startup.FirstWebContents.MainFrameLoad",
      g_process_creation_time.Get(), time);
}

void RecordFirstWebContentsNonEmptyPaint(const base::Time& time) {
  static bool is_first_call = true;
  if (!is_first_call || time.is_null())
    return;
  is_first_call = false;
  if (WasNonBrowserUIDisplayed() || g_process_creation_time.Get().is_null())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_STARTUP_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES_100, "Startup.FirstWebContents.NonEmptyPaint",
      g_process_creation_time.Get(), time);
}

base::Time MainEntryPointTime() {
  return g_main_entry_point_time.Get();
}

StartupTemperature GetStartupTemperature() {
  return g_startup_temperature;
}

}  // namespace startup_metric_utils
