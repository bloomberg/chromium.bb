// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/perf_test.h"

#include <stdio.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "chrome/test/base/chrome_process_util.h"

namespace {

std::string ResultsToString(const std::string& measurement,
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
  return base::StringPrintf("%sRESULT %s%s: %s= %s%s%s %s\n",
         important ? "*" : "", measurement.c_str(), modifier.c_str(),
         trace.c_str(), prefix.c_str(), values.c_str(), suffix.c_str(),
         units.c_str());
}

void PrintResultsImpl(const std::string& measurement,
                      const std::string& modifier,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& prefix,
                      const std::string& suffix,
                      const std::string& units,
                      bool important) {
  fflush(stdout);
  printf("%s", ResultsToString(measurement, modifier, trace, values,
                               prefix, suffix, units, important).c_str());
  fflush(stdout);
}

}  // namespace

namespace perf_test {

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 size_t value,
                 const std::string& units,
                 bool important) {
  PrintResultsImpl(measurement, modifier, trace, base::UintToString(value),
                   "", "", units, important);
}

void AppendResult(std::string& output,
                  const std::string& measurement,
                  const std::string& modifier,
                  const std::string& trace,
                  size_t value,
                  const std::string& units,
                  bool important) {
  output += ResultsToString(measurement, modifier, trace,
                            base::UintToString(value),
                            "", "", units, important);
}

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const std::string& value,
                 const std::string& units,
                 bool important) {
  PrintResultsImpl(measurement, modifier, trace, value, "", "", units,
                   important);
}

void AppendResult(std::string& output,
                  const std::string& measurement,
                  const std::string& modifier,
                  const std::string& trace,
                  const std::string& value,
                  const std::string& units,
                  bool important) {
  output += ResultsToString(measurement, modifier, trace, value, "", "", units,
                            important);
}

void PrintResultMeanAndError(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const std::string& mean_and_error,
                             const std::string& units,
                             bool important) {
  PrintResultsImpl(measurement, modifier, trace, mean_and_error,
                   "{", "}", units, important);
}

void AppendResultMeanAndError(std::string& output,
                              const std::string& measurement,
                              const std::string& modifier,
                              const std::string& trace,
                              const std::string& mean_and_error,
                              const std::string& units,
                              bool important) {
  output += ResultsToString(measurement, modifier, trace, mean_and_error,
                            "{", "}", units, important);
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const std::string& values,
                     const std::string& units,
                     bool important) {
  PrintResultsImpl(measurement, modifier, trace, values,
                   "[", "]", units, important);
}

void AppendResultList(std::string& output,
                      const std::string& measurement,
                      const std::string& modifier,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& units,
                      bool important) {
  output += ResultsToString(measurement, modifier, trace, values,
                            "[", "]", units, important);
}

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
    if (!base::OpenPrivilegedProcessHandle(*it, &process_handle)) {
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
  AppendResult(output, "read_op_b", "", "r_op_b" + t_name, read_op_b, "ops",
               false);
  AppendResult(output, "write_op_b", "", "w_op_b" + t_name, write_op_b, "ops",
               false);
  AppendResult(output, "other_op_b", "", "o_op_b" + t_name, other_op_b, "ops",
               false);
  AppendResult(output, "total_op_b", "", "IO_op_b" + t_name, total_op_b, "ops",
               false);

  AppendResult(output, "read_byte_b", "", "r_b" + t_name, read_byte_b, "kb",
               false);
  AppendResult(output, "write_byte_b", "", "w_b" + t_name, write_byte_b, "kb",
               false);
  AppendResult(output, "other_byte_b", "", "o_b" + t_name, other_byte_b, "kb",
               false);
  AppendResult(output, "total_byte_b", "", "IO_b" + t_name, total_byte_b, "kb",
               false);

  AppendResult(output, "read_op_r", "", "r_op_r" + t_name, read_op_r, "ops",
               false);
  AppendResult(output, "write_op_r", "", "w_op_r" + t_name, write_op_r, "ops",
               false);
  AppendResult(output, "other_op_r", "", "o_op_r" + t_name, other_op_r, "ops",
               false);
  AppendResult(output, "total_op_r", "", "IO_op_r" + t_name, total_op_r, "ops",
               false);

  AppendResult(output, "read_byte_r", "", "r_r" + t_name, read_byte_r, "kb",
               false);
  AppendResult(output, "write_byte_r", "", "w_r" + t_name, write_byte_r, "kb",
               false);
  AppendResult(output, "other_byte_r", "", "o_r" + t_name, other_byte_r, "kb",
               false);
  AppendResult(output, "total_byte_r", "", "IO_r" + t_name, total_byte_r, "kb",
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
    if (!base::OpenPrivilegedProcessHandle(*it, &process_handle)) {
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
  AppendResult(output, "vm_size_final_b", "", "vm_size_f_b" + trace_name,
               browser_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_rss_final_b", "", "vm_rss_f_b" + trace_name,
               browser_working_set_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_size_final_r", "", "vm_size_f_r" + trace_name,
               renderer_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_rss_final_r", "", "vm_rss_f_r" + trace_name,
               renderer_working_set_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_size_final_t", "", "vm_size_f_t" + trace_name,
               total_virtual_size, "bytes",
               false /* not important */);
  AppendResult(output, "vm_rss_final_t", "", "vm_rss_f_t" + trace_name,
               total_working_set_size, "bytes",
               false /* not important */);
#else
  NOTIMPLEMENTED();
#endif
  AppendResult(output, "processes", "", "proc_" + trace_name,
               chrome_processes.size(), "count",
               false /* not important */);

  return output;
}

void PrintSystemCommitCharge(const std::string& test_name,
                             size_t charge,
                             bool important) {
  PrintSystemCommitCharge(stdout, test_name, charge, important);
}

void PrintSystemCommitCharge(FILE* target,
                             const std::string& test_name,
                             size_t charge,
                             bool important) {
  fprintf(target, "%s", SystemCommitChargeToString(test_name, charge,
                                                   important).c_str());
}

std::string SystemCommitChargeToString(const std::string& test_name,
                                       size_t charge,
                                       bool important) {
  std::string trace_name(test_name);
  std::string output;
  AppendResult(output, "commit_charge", "", "cc" + trace_name, charge, "kb",
               important);
  return output;
}

}  // namespace perf_test
