// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/perf_test.h"

#include <stdio.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/chrome_process_util.h"
#include "testing/perf/perf_test.h"

namespace perf_test {

void PrintIOPerfInfo(const std::string& test_name,
                     const ChromeProcessList& chrome_processes,
                     base::ProcessId browser_pid) {
  PrintIOPerfInfo(stdout, test_name, chrome_processes, browser_pid);
}

void PrintIOPerfInfo(FILE* target,
                     const std::string& test_name,
                     const ChromeProcessList& chrome_processes,
                     base::ProcessId browser_pid) {
  fprintf(target, "%s", IOPerfInfoToString(test_name, chrome_processes,
                                           browser_pid).c_str());
}

std::string IOPerfInfoToString(const std::string& test_name,
                               const ChromeProcessList& chrome_processes,
                               base::ProcessId browser_pid) {
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

  std::string output;
  ChromeProcessList::const_iterator it;
  for (it = chrome_processes.begin(); it != chrome_processes.end(); ++it) {
    base::ProcessHandle process_handle;
    if (!base::OpenProcessHandle(*it, &process_handle)) {
      NOTREACHED();
      return output;
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
      std::string chrome_name = (*it == browser_pid) ? "_b" : "_r";

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

      if (*it == browser_pid) {
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
  AppendResult(output,
               "read_op_b",
               std::string(),
               "r_op_b" + t_name,
               read_op_b,
               "ops",
               false);
  AppendResult(output,
               "write_op_b",
               std::string(),
               "w_op_b" + t_name,
               write_op_b,
               "ops",
               false);
  AppendResult(output,
               "other_op_b",
               std::string(),
               "o_op_b" + t_name,
               other_op_b,
               "ops",
               false);
  AppendResult(output,
               "total_op_b",
               std::string(),
               "IO_op_b" + t_name,
               total_op_b,
               "ops",
               false);

  AppendResult(output,
               "read_byte_b",
               std::string(),
               "r_b" + t_name,
               read_byte_b,
               "kb",
               false);
  AppendResult(output,
               "write_byte_b",
               std::string(),
               "w_b" + t_name,
               write_byte_b,
               "kb",
               false);
  AppendResult(output,
               "other_byte_b",
               std::string(),
               "o_b" + t_name,
               other_byte_b,
               "kb",
               false);
  AppendResult(output,
               "total_byte_b",
               std::string(),
               "IO_b" + t_name,
               total_byte_b,
               "kb",
               false);

  AppendResult(output,
               "read_op_r",
               std::string(),
               "r_op_r" + t_name,
               read_op_r,
               "ops",
               false);
  AppendResult(output,
               "write_op_r",
               std::string(),
               "w_op_r" + t_name,
               write_op_r,
               "ops",
               false);
  AppendResult(output,
               "other_op_r",
               std::string(),
               "o_op_r" + t_name,
               other_op_r,
               "ops",
               false);
  AppendResult(output,
               "total_op_r",
               std::string(),
               "IO_op_r" + t_name,
               total_op_r,
               "ops",
               false);

  AppendResult(output,
               "read_byte_r",
               std::string(),
               "r_r" + t_name,
               read_byte_r,
               "kb",
               false);
  AppendResult(output,
               "write_byte_r",
               std::string(),
               "w_r" + t_name,
               write_byte_r,
               "kb",
               false);
  AppendResult(output,
               "other_byte_r",
               std::string(),
               "o_r" + t_name,
               other_byte_r,
               "kb",
               false);
  AppendResult(output,
               "total_byte_r",
               std::string(),
               "IO_r" + t_name,
               total_byte_r,
               "kb",
               false);

  return output;
}

void PrintMemoryUsageInfo(const std::string& test_name,
                          const ChromeProcessList& chrome_processes,
                          base::ProcessId browser_pid) {
  PrintMemoryUsageInfo(stdout, test_name, chrome_processes, browser_pid);
}

void PrintMemoryUsageInfo(FILE* target,
                          const std::string& test_name,
                          const ChromeProcessList& chrome_processes,
                          base::ProcessId browser_pid) {
  fprintf(target, "%s", MemoryUsageInfoToString(test_name, chrome_processes,
                                                browser_pid).c_str());
}

std::string MemoryUsageInfoToString(const std::string& test_name,
                                    const ChromeProcessList& chrome_processes,
                                    base::ProcessId browser_pid) {
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

  std::string output;
  ChromeProcessList::const_iterator it;
  for (it = chrome_processes.begin(); it != chrome_processes.end(); ++it) {
    base::ProcessHandle process_handle;
    if (!base::OpenProcessHandle(*it, &process_handle)) {
      NOTREACHED();
      return output;
    }

    // TODO(sgk):  if/when base::ProcessMetrics returns real stats on mac:
    // scoped_ptr<base::ProcessMetrics> process_metrics(
    //     base::ProcessMetrics::CreateProcessMetrics(process_handle));
    scoped_ptr<ChromeTestProcessMetrics> process_metrics(
        ChromeTestProcessMetrics::CreateProcessMetrics(process_handle));

    size_t current_virtual_size = process_metrics->GetPagefileUsage();
    size_t current_working_set_size = process_metrics->GetWorkingSetSize();

    if (*it == browser_pid) {
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
    if (*it == browser_pid) {
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
  AppendResult(output, "vm_peak_b", "", "vm_pk_b" + trace_name,
               browser_peak_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "ws_peak_b", "", "ws_pk_b" + trace_name,
               browser_peak_working_set_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_peak_r", "", "vm_pk_r" + trace_name,
               renderer_total_peak_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "ws_peak_r", "", "ws_pk_r" + trace_name,
               renderer_total_peak_working_set_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_single_peak_r", "", "vm_spk_r" + trace_name,
               renderer_single_peak_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "ws_single_peak_r", "", "ws_spk_r" + trace_name,
               renderer_single_peak_working_set_size, "bytes",
               false /* important */);
  AppendResult(output, "vm_final_b", "", "vm_f_b" + trace_name,
               browser_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "ws_final_b", "", "ws_f_b" + trace_name,
               browser_working_set_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_final_r", "", "vm_f_r" + trace_name,
               renderer_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "ws_final_r", "", "ws_f_r" + trace_name,
               renderer_working_set_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_final_t", "", "vm_f_t" + trace_name,
               total_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "ws_final_t", "", "ws_f_t" + trace_name,
               total_working_set_size, "bytes",
               false /* not important */);
#elif defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_ANDROID)
  AppendResult(output,
               "vm_size_final_b",
               std::string(),
               "vm_size_f_b" + trace_name,
               browser_virtual_size,
               "bytes",
               false /* not important */);
  AppendResult(output,
               "vm_rss_final_b",
               std::string(),
               "vm_rss_f_b" + trace_name,
               browser_working_set_size,
               "bytes",
               false /* not important */);
  AppendResult(output,
               "vm_size_final_r",
               std::string(),
               "vm_size_f_r" + trace_name,
               renderer_virtual_size,
               "bytes",
               false /* not important */);
  AppendResult(output,
               "vm_rss_final_r",
               std::string(),
               "vm_rss_f_r" + trace_name,
               renderer_working_set_size,
               "bytes",
               false /* not important */);
  AppendResult(output,
               "vm_size_final_t",
               std::string(),
               "vm_size_f_t" + trace_name,
               total_virtual_size,
               "bytes",
               false /* not important */);
  AppendResult(output,
               "vm_rss_final_t",
               std::string(),
               "vm_rss_f_t" + trace_name,
               total_working_set_size,
               "bytes",
               false /* not important */);
#else
  NOTIMPLEMENTED();
#endif
  AppendResult(output,
               "processes",
               std::string(),
               "proc_" + trace_name,
               chrome_processes.size(),
               "count",
               false /* not important */);

  return output;
}

}  // namespace perf_test
