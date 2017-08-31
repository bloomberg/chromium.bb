// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_CRASH_EXPORT_THUNKS_H_
#define COMPONENTS_CRASH_CONTENT_APP_CRASH_EXPORT_THUNKS_H_

#include <stddef.h>
#include <windows.h>

#include "build/build_config.h"

namespace crash_reporter {
struct Report;
}

extern "C" {

// TODO(siggi): Rename these functions to something descriptive and unique,
//   once all the thunks have been converted to import binding.

// This function may be invoked across module boundaries to request a single
// crash report upload. See CrashUploadListCrashpad.
void RequestSingleCrashUploadImpl(const char* local_id);

// This function may be invoked across module boundaries to retrieve the crash
// list. It copies up to |report_count| reports into |reports| and returns the
// number of reports available. If the return value is less than or equal to
// |reports_size|, then |reports| contains all the available reports.
size_t GetCrashReportsImpl(crash_reporter::Report* reports,
                           size_t reports_size);

// Crashes the process after generating a dump for the provided exception. Note
// that the crash reporter should be initialized before calling this function
// for it to do anything.
// NOTE: This function is used by SyzyASAN to invoke a crash. If you change the
// the name or signature of this function you will break SyzyASAN instrumented
// releases of Chrome. Please contact syzygy-team@chromium.org before doing so!
int CrashForException(EXCEPTION_POINTERS* info);

// This function is used in chrome_metrics_services_manager_client.cc to trigger
// changes to the upload-enabled state. This is done when the metrics services
// are initialized, and when the user changes their consent for uploads. See
// crash_reporter::SetUploadConsent for effects. The given consent value should
// be consistent with
// crash_reporter::GetCrashReporterClient()->GetCollectStatsConsent(), but it's
// not enforced to avoid blocking startup code on synchronizing them.
void SetUploadConsentImpl(bool consent);

// NOTE: This function is used by SyzyASAN to annotate crash reports. If you
// change the name or signature of this function you will break SyzyASAN
// instrumented releases of Chrome. Please contact syzygy-team@chromium.org
// before doing so! See also http://crbug.com/567781.
void SetCrashKeyValueImpl(const wchar_t* key, const wchar_t* value);

void ClearCrashKeyValueImpl(const wchar_t* key);

void SetCrashKeyValueImplEx(const char* key, const char* value);

void ClearCrashKeyValueImplEx(const char* key);

// Injects a thread into a remote process to dump state when there is no crash.
// |serialized_crash_keys| is a nul terminated string in the address space of
// |process| that represents serialized crash keys sent from the browser.
// Keys and values are separated by ':', and key/value pairs are separated by
// ','. All keys should be previously registered as crash keys.
// This method is used solely to classify hung input.
HANDLE InjectDumpForHungInput(HANDLE process, void* serialized_crash_keys);

// Injects a thread into a remote process to dump state when there is no crash.
// This method provides |reason| which will interpreted as an integer and logged
// as a crash key.
HANDLE InjectDumpForHungInputNoCrashKeys(HANDLE process, int reason);

#if defined(ARCH_CPU_X86_64)
// V8 support functions.
void RegisterNonABICompliantCodeRange(void* start, size_t size_in_bytes);
void UnregisterNonABICompliantCodeRange(void* start);
#endif  // defined(ARCH_CPU_X86_64)

}  // extern "C"

#endif  // COMPONENTS_CRASH_CONTENT_APP_CRASH_EXPORT_THUNKS_H_
