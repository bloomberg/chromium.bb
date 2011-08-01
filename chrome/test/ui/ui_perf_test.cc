// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_perf_test.h"

#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/test_switches.h"

void UIPerfTest::SetLaunchSwitches() {
  UITestBase::SetLaunchSwitches();

  // Reduce performance test variance by disabling background networking.
  launch_arguments_.AppendSwitch(switches::kDisableBackgroundNetworking);
}

void UIPerfTest::PrintResult(const std::string& measurement,
                         const std::string& modifier,
                         const std::string& trace,
                         size_t value,
                         const std::string& units,
                         bool important) {
  PrintResultsImpl(measurement, modifier, trace, base::UintToString(value),
                   "", "", units, important);
}

void UIPerfTest::PrintResult(const std::string& measurement,
                         const std::string& modifier,
                         const std::string& trace,
                         const std::string& value,
                         const std::string& units,
                         bool important) {
  PrintResultsImpl(measurement, modifier, trace, value, "", "", units,
                   important);
}

void UIPerfTest::PrintResultMeanAndError(const std::string& measurement,
                                     const std::string& modifier,
                                     const std::string& trace,
                                     const std::string& mean_and_error,
                                     const std::string& units,
                                     bool important) {
  PrintResultsImpl(measurement, modifier, trace, mean_and_error,
                   "{", "}", units, important);
}

void UIPerfTest::PrintResultList(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const std::string& values,
                             const std::string& units,
                             bool important) {
  PrintResultsImpl(measurement, modifier, trace, values,
                   "[", "]", units, important);
}

void UIPerfTest::PrintResultsImpl(const std::string& measurement,
                              const std::string& modifier,
                              const std::string& trace,
                              const std::string& values,
                              const std::string& prefix,
                              const std::string& suffix,
                              const std::string& units,
                              bool important) {
  // <*>RESULT <graph_name>: <trace_name>= <value> <units>
  // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
  printf("%sRESULT %s%s: %s= %s%s%s %s\n",
         important ? "*" : "", measurement.c_str(), modifier.c_str(),
         trace.c_str(), prefix.c_str(), values.c_str(), suffix.c_str(),
         units.c_str());
}

void UIPerfTest::PrintIOPerfInfo(const char* test_name) {
  ChromeProcessList chrome_processes(
      GetRunningChromeProcesses(browser_process_id()));

  size_t read_op_b = 0;
  size_t read_op_r = 0;
  size_t write_op_b = 0;
  size_t write_op_r = 0;
  size_t other_op_b = 0;
  size_t other_op_r = 0;
  size_t total_op_b = 0;
  size_t total_op_r = 0;

  size_t read_byte_b = 0;
  size_t read_byte_r = 0;
  size_t write_byte_b = 0;
  size_t write_byte_r = 0;
  size_t other_byte_b = 0;
  size_t other_byte_r = 0;
  size_t total_byte_b = 0;
  size_t total_byte_r = 0;

  ChromeProcessList::const_iterator it;
  for (it = chrome_processes.begin(); it != chrome_processes.end(); ++it) {
    base::ProcessHandle process_handle;
    if (!base::OpenPrivilegedProcessHandle(*it, &process_handle)) {
      NOTREACHED();
      return;
    }

    // TODO(sgk):  if/when base::ProcessMetrics returns real stats on mac:
    // scoped_ptr<base::ProcessMetrics> process_metrics(
    //     base::ProcessMetrics::CreateProcessMetrics(process_handle));
    scoped_ptr<ChromeTestProcessMetrics> process_metrics(
        ChromeTestProcessMetrics::CreateProcessMetrics(process_handle));
    base::IoCounters io_counters;
    memset(&io_counters, 0, sizeof(io_counters));

    if (process_metrics.get()->GetIOCounters(&io_counters)) {
      // Print out IO performance.  We assume that the values can be
      // converted to size_t (they're reported as ULONGLONG, 64-bit numbers).
      std::string chrome_name = (*it == browser_process_id()) ? "_b" : "_r";

      size_t read_op = static_cast<size_t>(io_counters.ReadOperationCount);
      size_t write_op = static_cast<size_t>(io_counters.WriteOperationCount);
      size_t other_op = static_cast<size_t>(io_counters.OtherOperationCount);
      size_t total_op = static_cast<size_t>(io_counters.ReadOperationCount +
                                            io_counters.WriteOperationCount +
                                            io_counters.OtherOperationCount);

      size_t read_byte = static_cast<size_t>(io_counters.ReadTransferCount
                                             / 1024);
      size_t write_byte = static_cast<size_t>(io_counters.WriteTransferCount
                                              / 1024);
      size_t other_byte = static_cast<size_t>(io_counters.OtherTransferCount
                                              / 1024);
      size_t total_byte = static_cast<size_t>((io_counters.ReadTransferCount +
                                               io_counters.WriteTransferCount +
                                               io_counters.OtherTransferCount)
                                              / 1024);

      if (*it == browser_process_id()) {
        read_op_b = read_op;
        write_op_b = write_op;
        other_op_b = other_op;
        total_op_b = total_op;
        read_byte_b = read_byte;
        write_byte_b = write_byte;
        other_byte_b = other_byte;
        total_byte_b = total_byte;
      } else {
        read_op_r += read_op;
        write_op_r += write_op;
        other_op_r += other_op;
        total_op_r += total_op;
        read_byte_r += read_byte;
        write_byte_r += write_byte;
        other_byte_r += other_byte;
        total_byte_r += total_byte;
      }
    }

    base::CloseProcessHandle(process_handle);
  }

  std::string t_name(test_name);
  PrintResult("read_op_b", "", "r_op_b" + t_name, read_op_b, "", false);
  PrintResult("write_op_b", "", "w_op_b" + t_name, write_op_b, "", false);
  PrintResult("other_op_b", "", "o_op_b" + t_name, other_op_b, "", false);
  PrintResult("total_op_b", "", "IO_op_b" + t_name, total_op_b, "", true);

  PrintResult("read_byte_b", "", "r_b" + t_name, read_byte_b, "kb", false);
  PrintResult("write_byte_b", "", "w_b" + t_name, write_byte_b, "kb", false);
  PrintResult("other_byte_b", "", "o_b" + t_name, other_byte_b, "kb", false);
  PrintResult("total_byte_b", "", "IO_b" + t_name, total_byte_b, "kb", true);

  PrintResult("read_op_r", "", "r_op_r" + t_name, read_op_r, "", false);
  PrintResult("write_op_r", "", "w_op_r" + t_name, write_op_r, "", false);
  PrintResult("other_op_r", "", "o_op_r" + t_name, other_op_r, "", false);
  PrintResult("total_op_r", "", "IO_op_r" + t_name, total_op_r, "", true);

  PrintResult("read_byte_r", "", "r_r" + t_name, read_byte_r, "kb", false);
  PrintResult("write_byte_r", "", "w_r" + t_name, write_byte_r, "kb", false);
  PrintResult("other_byte_r", "", "o_r" + t_name, other_byte_r, "kb", false);
  PrintResult("total_byte_r", "", "IO_r" + t_name, total_byte_r, "kb", true);
}

void UIPerfTest::PrintMemoryUsageInfo(const char* test_name) {
  ChromeProcessList chrome_processes(
      GetRunningChromeProcesses(browser_process_id()));

  size_t browser_virtual_size = 0;
  size_t browser_working_set_size = 0;
  size_t renderer_virtual_size = 0;
  size_t renderer_working_set_size = 0;
  size_t total_virtual_size = 0;
  size_t total_working_set_size = 0;
#if defined(OS_WIN)
  size_t browser_peak_virtual_size = 0;
  size_t browser_peak_working_set_size = 0;
  size_t renderer_total_peak_virtual_size = 0;
  size_t renderer_total_peak_working_set_size = 0;
  size_t renderer_single_peak_virtual_size = 0;
  size_t renderer_single_peak_working_set_size = 0;
#endif

  ChromeProcessList::const_iterator it;
  for (it = chrome_processes.begin(); it != chrome_processes.end(); ++it) {
    base::ProcessHandle process_handle;
    if (!base::OpenPrivilegedProcessHandle(*it, &process_handle)) {
      NOTREACHED();
      return;
    }

    // TODO(sgk):  if/when base::ProcessMetrics returns real stats on mac:
    // scoped_ptr<base::ProcessMetrics> process_metrics(
    //     base::ProcessMetrics::CreateProcessMetrics(process_handle));
    scoped_ptr<ChromeTestProcessMetrics> process_metrics(
        ChromeTestProcessMetrics::CreateProcessMetrics(process_handle));

    size_t current_virtual_size = process_metrics->GetPagefileUsage();
    size_t current_working_set_size = process_metrics->GetWorkingSetSize();

    if (*it == browser_process_id()) {
      browser_virtual_size = current_virtual_size;
      browser_working_set_size = current_working_set_size;
    } else {
      renderer_virtual_size += current_virtual_size;
      renderer_working_set_size += current_working_set_size;
    }
    total_virtual_size += current_virtual_size;
    total_working_set_size += current_working_set_size;

#if defined(OS_WIN)
    size_t peak_virtual_size = process_metrics->GetPeakPagefileUsage();
    size_t peak_working_set_size = process_metrics->GetPeakWorkingSetSize();
    if (*it == browser_process_id()) {
      browser_peak_virtual_size = peak_virtual_size;
      browser_peak_working_set_size = peak_working_set_size;
    } else {
      if (peak_virtual_size > renderer_single_peak_virtual_size) {
        renderer_single_peak_virtual_size = peak_virtual_size;
      }
      if (peak_working_set_size > renderer_single_peak_working_set_size) {
        renderer_single_peak_working_set_size = peak_working_set_size;
      }
      renderer_total_peak_virtual_size += peak_virtual_size;
      renderer_total_peak_working_set_size += peak_working_set_size;
    }
#endif

    base::CloseProcessHandle(process_handle);
  }

  std::string trace_name(test_name);
#if defined(OS_WIN)
  PrintResult("vm_peak_b", "", "vm_pk_b" + trace_name,
              browser_peak_virtual_size, "bytes",
              true /* important */);
  PrintResult("ws_peak_b", "", "ws_pk_b" + trace_name,
              browser_peak_working_set_size, "bytes",
              true /* important */);
  PrintResult("vm_peak_r", "", "vm_pk_r" + trace_name,
              renderer_total_peak_virtual_size, "bytes",
              true /* important */);
  PrintResult("ws_peak_r", "", "ws_pk_r" + trace_name,
              renderer_total_peak_working_set_size, "bytes",
              true /* important */);
  PrintResult("vm_single_peak_r", "", "vm_spk_r" + trace_name,
              renderer_single_peak_virtual_size, "bytes",
              true /* important */);
  PrintResult("ws_single_peak_r", "", "ws_spk_r" + trace_name,
              renderer_single_peak_working_set_size, "bytes",
              true /* important */);

  PrintResult("vm_final_b", "", "vm_f_b" + trace_name,
              browser_virtual_size, "bytes",
              false /* not important */);
  PrintResult("ws_final_b", "", "ws_f_b" + trace_name,
              browser_working_set_size, "bytes",
              false /* not important */);
  PrintResult("vm_final_r", "", "vm_f_r" + trace_name,
              renderer_virtual_size, "bytes",
              false /* not important */);
  PrintResult("ws_final_r", "", "ws_f_r" + trace_name,
              renderer_working_set_size, "bytes",
              false /* not important */);
  PrintResult("vm_final_t", "", "vm_f_t" + trace_name,
              total_virtual_size, "bytes",
              false /* not important */);
  PrintResult("ws_final_t", "", "ws_f_t" + trace_name,
              total_working_set_size, "bytes",
              false /* not important */);
#elif defined(OS_LINUX) || defined(OS_MACOSX)
  PrintResult("vm_size_final_b", "", "vm_size_f_b" + trace_name,
              browser_virtual_size, "bytes",
              true /* important */);
  PrintResult("vm_rss_final_b", "", "vm_rss_f_b" + trace_name,
              browser_working_set_size, "bytes",
              true /* important */);
  PrintResult("vm_size_final_r", "", "vm_size_f_r" + trace_name,
              renderer_virtual_size, "bytes",
              true /* important */);
  PrintResult("vm_rss_final_r", "", "vm_rss_f_r" + trace_name,
              renderer_working_set_size, "bytes",
              true /* important */);
  PrintResult("vm_size_final_t", "", "vm_size_f_t" + trace_name,
              total_virtual_size, "bytes",
              true /* important */);
  PrintResult("vm_rss_final_t", "", "vm_rss_f_t" + trace_name,
              total_working_set_size, "bytes",
              true /* important */);
#else
  NOTIMPLEMENTED();
#endif
  PrintResult("processes", "", "proc_" + trace_name,
              chrome_processes.size(), "",
              false /* not important */);
}

void UIPerfTest::PrintSystemCommitCharge(const char* test_name,
                                     size_t charge,
                                     bool important) {
  std::string trace_name(test_name);
  PrintResult("commit_charge", "", "cc" + trace_name, charge, "kb", important);
}

void UIPerfTest::UseReferenceBuild() {
  FilePath dir;
  PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
  dir = dir.AppendASCII("reference_build");
#if defined(OS_WIN)
  dir = dir.AppendASCII("chrome_win");
#elif defined(OS_LINUX)
  dir = dir.AppendASCII("chrome_linux");
#elif defined(OS_MACOSX)
  dir = dir.AppendASCII("chrome_mac");
#endif
  launch_arguments_.AppendSwitch(switches::kEnableChromiumBranding);
  SetBrowserDirectory(dir);
}
