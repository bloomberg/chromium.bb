// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/trace_startup_config.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"

#if defined(OS_ANDROID)
#include "base/android/early_trace_event_binding.h"
#endif

namespace tracing {

namespace {

// Maximum trace config file size that will be loaded, in bytes.
const size_t kTraceConfigFileSizeLimit = 64 * 1024;
const int kDefaultStartupDuration = 5;

// Trace config file path:
// - Android: /data/local/chrome-trace-config.json
// - Others: specified by --trace-config-file flag.
#if defined(OS_ANDROID)
const base::FilePath::CharType kAndroidTraceConfigFile[] =
    FILE_PATH_LITERAL("/data/local/chrome-trace-config.json");

const char kDefaultStartupCategories[] =
    "startup,browser,toplevel,EarlyJava,cc,Java,navigation,loading,gpu,"
    "disabled-by-default-cpu_profiler,download_service,-*";
#else
const char kDefaultStartupCategories[] =
    "benchmark,toplevel,startup,disabled-by-default-file,disabled-by-default-"
    "toplevel.flow,disabled-by-default-ipc.flow,download_service,-*";
#endif

// String parameters that can be used to parse the trace config file content.
const char kTraceConfigParam[] = "trace_config";
const char kStartupDurationParam[] = "startup_duration";
const char kResultFileParam[] = "result_file";

}  // namespace

// static
TraceStartupConfig* TraceStartupConfig::GetInstance() {
  return base::Singleton<TraceStartupConfig, base::DefaultSingletonTraits<
                                                 TraceStartupConfig>>::get();
}

// static
base::trace_event::TraceConfig
TraceStartupConfig::GetDefaultBrowserStartupConfig() {
  base::trace_event::TraceConfig trace_config(
      kDefaultStartupCategories, base::trace_event::RECORD_UNTIL_FULL);
  // Filter only browser process events.
  base::trace_event::TraceConfig::ProcessFilterConfig process_config(
      {base::GetCurrentProcId()});
  // First 10k events at start are sufficient to debug startup traces.
  trace_config.SetTraceBufferSizeInEvents(10000);
  trace_config.SetProcessFilterConfig(process_config);
  // Enable argument filter since we could be background tracing.
  trace_config.EnableArgumentFilter();
  return trace_config;
}

TraceStartupConfig::TraceStartupConfig()
    : is_enabled_(false),
      is_enabled_from_background_tracing_(false),
      trace_config_(base::trace_event::TraceConfig()),
      startup_duration_(0),
      should_trace_to_result_file_(false) {
  if (EnableFromCommandLine()) {
    DCHECK(IsEnabled());
  } else if (EnableFromConfigFile()) {
    DCHECK(IsEnabled());
  } else if (EnableFromBackgroundTracing()) {
    DCHECK(IsEnabled());
    DCHECK(!IsTracingStartupForDuration());
    DCHECK(GetBackgroundStartupTracingEnabled());
    CHECK(!ShouldTraceToResultFile());
  }
}

TraceStartupConfig::~TraceStartupConfig() = default;

bool TraceStartupConfig::IsEnabled() const {
  return is_enabled_;
}

void TraceStartupConfig::SetDisabled() {
  is_enabled_ = false;
}

bool TraceStartupConfig::IsTracingStartupForDuration() const {
  return is_enabled_ && startup_duration_ > 0;
}

base::trace_event::TraceConfig TraceStartupConfig::GetTraceConfig() const {
  DCHECK(IsEnabled());
  return trace_config_;
}

int TraceStartupConfig::GetStartupDuration() const {
  DCHECK(IsEnabled());
  return startup_duration_;
}

bool TraceStartupConfig::ShouldTraceToResultFile() const {
  return is_enabled_ && should_trace_to_result_file_;
}

base::FilePath TraceStartupConfig::GetResultFile() const {
  DCHECK(IsEnabled());
  DCHECK(ShouldTraceToResultFile());
  return result_file_;
}

bool TraceStartupConfig::GetBackgroundStartupTracingEnabled() const {
  return is_enabled_from_background_tracing_;
}

void TraceStartupConfig::SetBackgroundStartupTracingEnabled(bool enabled) {
#if defined(OS_ANDROID)
  base::android::SetBackgroundStartupTracingFlag(enabled);
#endif
}

bool TraceStartupConfig::EnableFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kTraceStartup))
    return false;
  std::string startup_duration_str =
      command_line->GetSwitchValueASCII(switches::kTraceStartupDuration);
  startup_duration_ = kDefaultStartupDuration;
  if (!startup_duration_str.empty() &&
      !base::StringToInt(startup_duration_str, &startup_duration_)) {
    DLOG(WARNING) << "Could not parse --" << switches::kTraceStartupDuration
                  << "=" << startup_duration_str << " defaulting to 5 (secs)";
    startup_duration_ = kDefaultStartupDuration;
  }

  trace_config_ = base::trace_event::TraceConfig(
      command_line->GetSwitchValueASCII(switches::kTraceStartup),
      command_line->GetSwitchValueASCII(switches::kTraceStartupRecordMode));

  result_file_ = command_line->GetSwitchValuePath(switches::kTraceStartupFile);

  is_enabled_ = true;
  should_trace_to_result_file_ = true;
  return true;
}

bool TraceStartupConfig::EnableFromConfigFile() {
#if defined(OS_ANDROID)
  base::FilePath trace_config_file(kAndroidTraceConfigFile);
#else
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kTraceConfigFile))
    return false;
  base::FilePath trace_config_file =
      command_line->GetSwitchValuePath(switches::kTraceConfigFile);
#endif

  if (trace_config_file.empty()) {
    // If the trace config file path is not specified, trace Chrome with the
    // default configuration for 5 sec.
    startup_duration_ = kDefaultStartupDuration;
    is_enabled_ = true;
    should_trace_to_result_file_ = true;
    DLOG(WARNING) << "Use default trace config.";
    return true;
  }

  if (!base::PathExists(trace_config_file)) {
    DLOG(WARNING) << "The trace config file does not exist.";
    return false;
  }

  std::string trace_config_file_content;
  if (!base::ReadFileToStringWithMaxSize(trace_config_file,
                                         &trace_config_file_content,
                                         kTraceConfigFileSizeLimit)) {
    DLOG(WARNING) << "Cannot read the trace config file correctly.";
    return false;
  }
  is_enabled_ = ParseTraceConfigFileContent(trace_config_file_content);
  if (!is_enabled_)
    DLOG(WARNING) << "Cannot parse the trace config file correctly.";
  should_trace_to_result_file_ = is_enabled_;
  return is_enabled_;
}

bool TraceStartupConfig::EnableFromBackgroundTracing() {
#if defined(OS_ANDROID)
  is_enabled_from_background_tracing_ =
      base::android::GetBackgroundStartupTracingFlag();
#else
  is_enabled_from_background_tracing_ = false;
#endif
  // Do not set the flag to false if it's not enabled unnecessarily.
  if (!is_enabled_from_background_tracing_)
    return false;

  SetBackgroundStartupTracingEnabled(false);
  trace_config_ = GetDefaultBrowserStartupConfig();
  is_enabled_ = true;
  should_trace_to_result_file_ = false;
  // Set startup duration to 0 since background tracing config will configure
  // the durations later.
  startup_duration_ = 0;
  return true;
}

bool TraceStartupConfig::ParseTraceConfigFileContent(
    const std::string& content) {
  std::unique_ptr<base::Value> value(base::JSONReader::Read(content));
  if (!value || !value->is_dict())
    return false;

  std::unique_ptr<base::DictionaryValue> dict(
      static_cast<base::DictionaryValue*>(value.release()));

  base::DictionaryValue* trace_config_dict = nullptr;
  if (!dict->GetDictionary(kTraceConfigParam, &trace_config_dict))
    return false;

  trace_config_ = base::trace_event::TraceConfig(*trace_config_dict);

  if (!dict->GetInteger(kStartupDurationParam, &startup_duration_))
    startup_duration_ = 0;

  if (startup_duration_ < 0)
    startup_duration_ = 0;

  base::FilePath::StringType result_file_str;
  if (dict->GetString(kResultFileParam, &result_file_str))
    result_file_ = base::FilePath(result_file_str);

  return true;
}

}  // namespace tracing
