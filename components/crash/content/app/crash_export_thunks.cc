// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crash_export_thunks.h"

#include <algorithm>
#include <type_traits>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/crash/content/app/crashpad.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

void RequestSingleCrashUpload_ExportThunk(const char* local_id) {
  crash_reporter::RequestSingleCrashUploadImpl(local_id);
}

size_t GetCrashReports_ExportThunk(crash_reporter::Report* reports,
                                   size_t reports_size) {
  static_assert(std::is_pod<crash_reporter::Report>::value,
                "crash_reporter::Report must be POD");
  // Since this could be called across module boundaries, retrieve the full
  // list of reports into this vector, and then manually copy however much fits
  // into the caller's copy.
  std::vector<crash_reporter::Report> crash_reports;

  // The crash_reporter::GetReports function thunks here, here is delegation to
  // the actual implementation.
  crash_reporter::GetReportsImpl(&crash_reports);

  size_t to_copy = std::min(reports_size, crash_reports.size());
  for (size_t i = 0; i < to_copy; ++i)
    reports[i] = crash_reports[i];

  return crash_reports.size();
}

int CrashForException_ExportThunk(EXCEPTION_POINTERS* info) {
  crash_reporter::GetCrashpadClient().DumpAndCrash(info);
  return EXCEPTION_CONTINUE_SEARCH;
}

// This function is used in chrome_metrics_services_manager_client.cc to trigger
// changes to the upload-enabled state. This is done when the metrics services
// are initialized, and when the user changes their consent for uploads. See
// crash_reporter::SetUploadConsent for effects. The given consent value should
// be consistent with
// crash_reporter::GetCrashReporterClient()->GetCollectStatsConsent(), but it's
// not enforced to avoid blocking startup code on synchronizing them.
void SetUploadConsent_ExportThunk(bool consent) {
  crash_reporter::SetUploadConsent(consent);
}

// NOTE: This function is used by SyzyASAN to annotate crash reports. If you
// change the name or signature of this function you will break SyzyASAN
// instrumented releases of Chrome. Please contact syzygy-team@chromium.org
// before doing so! See also http://crbug.com/567781.
void SetCrashKeyValue_ExportThunk(const wchar_t* key, const wchar_t* value) {
  crash_reporter::SetCrashKeyValue(base::UTF16ToUTF8(key),
                                   base::UTF16ToUTF8(value));
}

void ClearCrashKeyValue_ExportThunk(const wchar_t* key) {
  crash_reporter::ClearCrashKey(base::UTF16ToUTF8(key));
}

void SetCrashKeyValueEx_ExportThunk(const char* key,
                                    size_t key_len,
                                    const char* value,
                                    size_t value_len) {
  crash_reporter::SetCrashKeyValue(base::StringPiece(key, key_len),
                                   base::StringPiece(value, value_len));
}

void ClearCrashKeyValueEx_ExportThunk(const char* key, size_t key_len) {
  crash_reporter::ClearCrashKey(base::StringPiece(key, key_len));
}

HANDLE InjectDumpForHungInput_ExportThunk(HANDLE process,
                                          void* serialized_crash_keys) {
  return CreateRemoteThread(
      process, nullptr, 0,
      crash_reporter::internal::DumpProcessForHungInputThread,
      serialized_crash_keys, 0, nullptr);
}

HANDLE InjectDumpForHungInputNoCrashKeys_ExportThunk(HANDLE process,
                                                     int reason) {
  return CreateRemoteThread(
      process, nullptr, 0,
      crash_reporter::internal::DumpProcessForHungInputNoCrashKeysThread,
      reinterpret_cast<void*>(reason), 0, nullptr);
}

#if defined(ARCH_CPU_X86_64)

void RegisterNonABICompliantCodeRange_ExportThunk(void* start,
                                                  size_t size_in_bytes) {
  crash_reporter::internal::RegisterNonABICompliantCodeRangeImpl(start,
                                                                 size_in_bytes);
}

void UnregisterNonABICompliantCodeRange_ExportThunk(void* start) {
  crash_reporter::internal::UnregisterNonABICompliantCodeRangeImpl(start);
}

#endif  // ARCH_CPU_X86_64
