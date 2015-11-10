// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crashpad_mac.h"

#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/auto_reset.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/client/simple_string_dictionary.h"
#include "third_party/crashpad/crashpad/client/simulate_crash.h"

namespace crash_reporter {

namespace {

crashpad::SimpleStringDictionary* g_simple_string_dictionary;
crashpad::CrashReportDatabase* g_database;

void SetCrashKeyValue(const base::StringPiece& key,
                      const base::StringPiece& value) {
  g_simple_string_dictionary->SetKeyValue(key.data(), value.data());
}

void ClearCrashKey(const base::StringPiece& key) {
  g_simple_string_dictionary->RemoveKey(key.data());
}

bool LogMessageHandler(int severity,
                       const char* file,
                       int line,
                       size_t message_start,
                       const std::string& string) {
  // Only handle FATAL.
  if (severity != logging::LOG_FATAL) {
    return false;
  }

  // In case of an out-of-memory condition, this code could be reentered when
  // constructing and storing the key. Using a static is not thread-safe, but if
  // multiple threads are in the process of a fatal crash at the same time, this
  // should work.
  static bool guarded = false;
  if (guarded) {
    return false;
  }
  base::AutoReset<bool> guard(&guarded, true);

  // Only log last path component.  This matches logging.cc.
  if (file) {
    const char* slash = strrchr(file, '/');
    if (slash) {
      file = slash + 1;
    }
  }

  std::string message = base::StringPrintf("%s:%d: %s", file, line,
                                           string.c_str() + message_start);
  SetCrashKeyValue("LOG_FATAL", message);

  // Rather than including the code to force the crash here, allow the caller to
  // do it.
  return false;
}

void DumpWithoutCrashing() {
  CRASHPAD_SIMULATE_CRASH();
}

}  // namespace

void InitializeCrashpad(bool initial_client, const std::string& process_type) {
  static bool initialized = false;
  DCHECK(!initialized);
  initialized = true;

  const bool browser_process = process_type.empty();
  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();

  if (initial_client) {
    // "relauncher" is hard-coded because it's a Chrome --type, but this
    // component can't see Chrome's switches. This is only used for argument
    // sanitization.
    DCHECK(browser_process || process_type == "relauncher");
  } else {
    DCHECK(!browser_process);
  }

  base::FilePath database_path;  // Only valid in the browser process.

  if (initial_client) {
    @autoreleasepool {
      base::FilePath framework_bundle_path = base::mac::FrameworkBundlePath();
      base::FilePath handler_path =
          framework_bundle_path.Append("Helpers").Append("crashpad_handler");

      // Is there a way to recover if this fails?
      crash_reporter_client->GetCrashDumpLocation(&database_path);

      // TODO(mark): Reading the Breakpad keys is temporary and transitional. At
      // the very least, they should be renamed to Crashpad. For the time being,
      // this isn't the worst thing: Crashpad is still uploading to a
      // Breakpad-type server, after all.
      NSBundle* framework_bundle = base::mac::FrameworkBundle();
      NSString* product = base::mac::ObjCCast<NSString>(
          [framework_bundle objectForInfoDictionaryKey:@"BreakpadProduct"]);
      NSString* version = base::mac::ObjCCast<NSString>(
          [framework_bundle objectForInfoDictionaryKey:@"BreakpadVersion"]);
      NSString* url_ns = base::mac::ObjCCast<NSString>(
          [framework_bundle objectForInfoDictionaryKey:@"BreakpadURL"]);

      std::string url = base::SysNSStringToUTF8(url_ns);

      std::map<std::string, std::string> process_annotations;
      process_annotations["prod"] = base::SysNSStringToUTF8(product);
      process_annotations["ver"] = base::SysNSStringToUTF8(version);
      process_annotations["plat"] = std::string("OS X");

      crashpad::CrashpadClient crashpad_client;

      std::vector<std::string> arguments;
      if (!browser_process) {
        // If this is an initial client that's not the browser process, it's
        // important that the new Crashpad handler also not be connected to any
        // existing handler. This argument tells the new Crashpad handler to
        // sever this connection.
        arguments.push_back(
            "--reset-own-crash-exception-port-to-system-default");
      }

      bool result = crashpad_client.StartHandler(handler_path,
                                                 database_path,
                                                 url,
                                                 process_annotations,
                                                 arguments,
                                                 false);
      if (result) {
        result = crashpad_client.UseHandler();
      }

      // If this is an initial client that's not the browser process, it's
      // important to sever the connection to any existing handler. If
      // StartHandler() or UseHandler() failed, call UseSystemDefaultHandler()
      // in that case to drop the link to the existing handler.
      if (!result && !browser_process) {
        crashpad::CrashpadClient::UseSystemDefaultHandler();
      }
    }  // @autoreleasepool
  }

  crashpad::CrashpadInfo* crashpad_info =
      crashpad::CrashpadInfo::GetCrashpadInfo();

#if defined(NDEBUG)
  const bool is_debug_build = false;
#else
  const bool is_debug_build = true;
#endif

  // Disable forwarding to the system's crash reporter in processes other than
  // the browser process. For the browser, the system's crash reporter presents
  // the crash UI to the user, so it's desirable there. Additionally, having
  // crash reports appear in ~/Library/Logs/DiagnosticReports provides a
  // fallback. Forwarding is turned off for debug-mode builds even for the
  // browser process, because the system's crash reporter can take a very long
  // time to chew on symbols.
  if (!browser_process || is_debug_build) {
    crashpad_info->set_system_crash_reporter_forwarding(
        crashpad::TriState::kDisabled);
  }

  g_simple_string_dictionary = new crashpad::SimpleStringDictionary();
  crashpad_info->set_simple_annotations(g_simple_string_dictionary);

  base::debug::SetCrashKeyReportingFunctions(SetCrashKeyValue, ClearCrashKey);
  crash_reporter_client->RegisterCrashKeys();

  SetCrashKeyValue("ptype", browser_process ? base::StringPiece("browser")
                                            : base::StringPiece(process_type));
  SetCrashKeyValue("pid", base::IntToString(getpid()));

  logging::SetLogMessageHandler(LogMessageHandler);

  // If clients called CRASHPAD_SIMULATE_CRASH() instead of
  // base::debug::DumpWithoutCrashing(), these dumps would appear as crashes in
  // the correct function, at the correct file and line. This would be
  // preferable to having all occurrences show up in DumpWithoutCrashing() at
  // the same file and line.
  base::debug::SetDumpWithoutCrashingFunction(DumpWithoutCrashing);

  if (browser_process) {
    g_database =
        crashpad::CrashReportDatabase::Initialize(database_path).release();

    bool enable_uploads = false;
    if (!crash_reporter_client->ReportingIsEnforcedByPolicy(&enable_uploads)) {
      // Breakpad provided a --disable-breakpad switch to disable crash dumping
      // (not just uploading) here. Crashpad doesn't need it: dumping is enabled
      // unconditionally and uploading is gated on consent, which tests/bots
      // shouldn't have. As a precaution, uploading is also disabled on bots
      // even if consent is present.
      enable_uploads = crash_reporter_client->GetCollectStatsConsent() &&
                       !crash_reporter_client->IsRunningUnattended();
    }

    SetUploadsEnabled(enable_uploads);
  }
}

void SetUploadsEnabled(bool enable_uploads) {
  if (g_database) {
    crashpad::Settings* settings = g_database->GetSettings();
    settings->SetUploadsEnabled(enable_uploads);
  }
}

bool GetUploadsEnabled() {
  if (g_database) {
    crashpad::Settings* settings = g_database->GetSettings();
    bool enable_uploads;
    if (settings->GetUploadsEnabled(&enable_uploads)) {
      return enable_uploads;
    }
  }

  return false;
}

void GetUploadedReports(std::vector<UploadedReport>* uploaded_reports) {
  uploaded_reports->clear();

  if (!g_database) {
    return;
  }

  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  crashpad::CrashReportDatabase::OperationStatus status =
      g_database->GetCompletedReports(&completed_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  for (const crashpad::CrashReportDatabase::Report& completed_report :
       completed_reports) {
    if (completed_report.uploaded) {
      UploadedReport uploaded_report;
      uploaded_report.local_id = completed_report.uuid.ToString();
      uploaded_report.remote_id = completed_report.id;
      uploaded_report.creation_time = completed_report.creation_time;

      uploaded_reports->push_back(uploaded_report);
    }
  }

  struct {
    bool operator()(const UploadedReport& a, const UploadedReport& b) {
      return a.creation_time >= b.creation_time;
    }
  } sort_by_time;
  std::sort(uploaded_reports->begin(), uploaded_reports->end(), sort_by_time);
}

}  // namespace crash_reporter
