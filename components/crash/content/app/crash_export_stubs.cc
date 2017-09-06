// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file has no-op stub implementation for the functions declared in
// crash_export_thunks.h. This is solely for linking into tests, where
// crash reporting is unwanted and never initialized.

#include <windows.h>

#include "build/build_config.h"
#include "components/crash/content/app/crash_export_thunks.h"
#include "components/crash/content/app/crashpad.h"

void RequestSingleCrashUpload_ExportThunk(const char* local_id) {}

size_t GetCrashReports_ExportThunk(crash_reporter::Report* reports,
                                   size_t reports_size) {
  return 0;
}

int CrashForException_ExportThunk(EXCEPTION_POINTERS* info) {
  // Make sure to properly crash the process by dispatching directly to the
  // Windows unhandled exception filter.
  return UnhandledExceptionFilter(info);
}

void SetUploadConsent_ExportThunk(bool consent) {}

void SetCrashKeyValue_ExportThunk(const wchar_t* key, const wchar_t* value) {}

void ClearCrashKeyValue_ExportThunk(const wchar_t* key) {}

void SetCrashKeyValueEx_ExportThunk(const char* key,
                                    size_t key_len,
                                    const char* value,
                                    size_t value_len) {}

void ClearCrashKeyValueEx_ExportThunk(const char* key, size_t key_len) {}

HANDLE InjectDumpForHungInput_ExportThunk(HANDLE process,
                                          void* serialized_crash_keys) {
  return nullptr;
}

HANDLE InjectDumpForHungInputNoCrashKeys_ExportThunk(HANDLE process,
                                                     int reason) {
  return nullptr;
}

#if defined(ARCH_CPU_X86_64)

void RegisterNonABICompliantCodeRange_ExportThunk(void* start,
                                                  size_t size_in_bytes) {}

void UnregisterNonABICompliantCodeRange_ExportThunk(void* start) {}

#endif  // defined(ARCH_CPU_X86_64)
