// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/startup_tracing.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/trace_event/trace_event.h"

namespace tracing {

namespace {

// Maximum trace config file size that will be loaded, in bytes.
const size_t kTraceConfigFileSizeLimit = 64 * 1024;

// Trace config file path:
// - Android: /data/local/.config/chrome-trace-config.json
// - POSIX other than Android: $HOME/.config/chrome-trace-config.json
// - Win: %USERPROFILE%/.config/chrome-trace-config.json
#if defined(OS_ANDROID)
const base::FilePath::CharType kAndroidTraceConfigDir[] =
    FILE_PATH_LITERAL("/data/local");
#endif

const base::FilePath::CharType kChromeConfigDir[] =
    FILE_PATH_LITERAL(".config");
const base::FilePath::CharType kTraceConfigFileName[] =
    FILE_PATH_LITERAL("chrome-trace-config.json");

base::FilePath GetTraceConfigFilePath() {
#if defined(OS_ANDROID)
  base::FilePath path(kAndroidTraceConfigDir);
#elif defined(OS_POSIX) || defined(OS_WIN)
  base::FilePath path;
  PathService::Get(base::DIR_HOME, &path);
#else
  base::FilePath path;
#endif
  path = path.Append(kChromeConfigDir);
  path = path.Append(kTraceConfigFileName);
  return path;
}

} // namespace

void EnableStartupTracingIfConfigFileExists() {
  base::FilePath trace_config_file_path = GetTraceConfigFilePath();
  if (!base::PathExists(trace_config_file_path))
    return;

  std::string trace_config_str;
  if (!base::ReadFileToString(trace_config_file_path,
                              &trace_config_str,
                              kTraceConfigFileSizeLimit)) {
    return;
  }

  base::trace_event::TraceConfig trace_config(trace_config_str);
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      trace_config, base::trace_event::TraceLog::RECORDING_MODE);
}

}  // namespace tracing
