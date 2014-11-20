// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crash_reporter_client.h"

#include "base/time/time.h"
#include "components/crash/app/breakpad_linux.h"
#include "content/public/common/content_switches.h"

namespace chromecast {

namespace {

char* g_process_type = NULL;
uint64_t g_process_start_time = 0;

}  // namespace

// static
void CastCrashReporterClient::InitCrashReporter(
    const std::string& process_type) {
  g_process_start_time = (base::TimeTicks::Now() -
                          base::TimeTicks()).InMilliseconds();

  // Save the process type (leaked).
  // Note: "browser" process is identified by empty process type string.
  const std::string& named_process_type(
      process_type.empty() ? "browser" : process_type);
  const size_t process_type_len = named_process_type.size() + 1;
  g_process_type = new char[process_type_len];
  strncpy(g_process_type, named_process_type.c_str(), process_type_len);

  // Start up breakpad for this process, if applicable.
  breakpad::InitCrashReporter(process_type);
}

// static
char* CastCrashReporterClient::GetProcessType() {
  return g_process_type;
}

// static
uint64_t CastCrashReporterClient::GetProcessStartTime() {
  return g_process_start_time;
}

CastCrashReporterClient::CastCrashReporterClient() {}
CastCrashReporterClient::~CastCrashReporterClient() {}

bool CastCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}

}  // namespace chromecast
