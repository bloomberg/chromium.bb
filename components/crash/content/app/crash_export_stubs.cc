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

void RequestSingleCrashUploadThunk(const char* local_id) {}

size_t GetCrashReportsImpl(crash_reporter::Report* reports,
                           size_t reports_size) {
  return 0;
}

int CrashForException(EXCEPTION_POINTERS* info) {
  // Make sure to properly crash the process by dispatching directly to the
  // Windows unhandled exception filter.
  return UnhandledExceptionFilter(info);
}

void SetUploadConsentImpl(bool consent) {}

void SetCrashKeyValueImpl(const wchar_t* key, const wchar_t* value) {}

void ClearCrashKeyValueImpl(const wchar_t* key) {}

void SetCrashKeyValueImplEx(const char* key, const char* value) {}

void ClearCrashKeyValueImplEx(const char* key) {}

HANDLE InjectDumpForHungInput(HANDLE process, void* serialized_crash_keys) {
  return nullptr;
}

HANDLE InjectDumpForHungInputNoCrashKeys(HANDLE process, int reason) {
  return nullptr;
}

#if defined(ARCH_CPU_X86_64)

void RegisterNonABICompliantCodeRange(void* start, size_t size_in_bytes) {}

void UnregisterNonABICompliantCodeRange(void* start) {}

#endif  // defined(ARCH_CPU_X86_64)
