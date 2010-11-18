// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Definition of IE crash reporter.

#include "ceee/ie/common/crash_reporter.h"

#include "base/logging.h"
#include "base/stringize_macros.h"
#include "ceee/ie/common/ceee_module_util.h"

#include "version.h"  // NOLINT

const wchar_t kGoogleUpdatePipeName[] =
    L"\\\\.\\pipe\\GoogleCrashServices\\S-1-5-18";

CrashReporter::CrashReporter(const wchar_t* component_name)
    : exception_handler_(NULL) {
  // Initialize the custom data that will be used to identify the client
  // when reporting a crash.
  google_breakpad::CustomInfoEntry ver_entry(
      L"ver", TO_L_STRING(CHROME_VERSION_STRING));
  google_breakpad::CustomInfoEntry prod_entry(L"prod", L"CEEE_IE");
  google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  google_breakpad::CustomInfoEntry type_entry(L"ptype", component_name);
  google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };

  const int num_entries = arraysize(entries);
  client_info_entries_.reset(
      new google_breakpad::CustomInfoEntry[num_entries]);

  for (int i = 0; i < num_entries; ++i) {
    client_info_entries_[i] = entries[i];
  }

  client_info_.entries = client_info_entries_.get();
  client_info_.count = num_entries;
}

CrashReporter::~CrashReporter() {
  DCHECK(exception_handler_ == NULL);
}

void CrashReporter::InitializeCrashReporting(bool full_dump) {
  DCHECK(exception_handler_ == NULL);

  if (!ceee_module_util::GetCollectStatsConsent())
    return;

  wchar_t temp_path[MAX_PATH];
  DWORD len = ::GetTempPath(arraysize(temp_path), temp_path);
  if (len == 0) {
    LOG(ERROR) << "Failed to instantiate Breakpad exception handler. " <<
        "Could not get a temp path.";
    return;
  }

  // Install an exception handler instance here, which should be the lowest
  // level in the process. We give it the appropriate pipe name so it can
  // talk to the reporting service (e.g. Omaha) to do dump generation and
  // send information back to the server.
  MINIDUMP_TYPE dump_type = full_dump ? MiniDumpWithFullMemory : MiniDumpNormal;
  exception_handler_ = new google_breakpad::ExceptionHandler(
      temp_path, NULL, NULL, NULL,
      google_breakpad::ExceptionHandler::HANDLER_ALL, dump_type,
      kGoogleUpdatePipeName, &client_info_);

  LOG_IF(ERROR, exception_handler_ == NULL) <<
      "Failed to instantiate Breakpad exception handler.";
}

void CrashReporter::ShutdownCrashReporting() {
  delete exception_handler_;
  exception_handler_ = NULL;
}
