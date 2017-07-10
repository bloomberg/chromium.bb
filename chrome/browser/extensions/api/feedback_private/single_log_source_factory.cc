// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/single_log_source_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/system_logs/single_log_file_log_source.h"

namespace extensions {

namespace {

namespace feedback_private = api::feedback_private;

using system_logs::SingleLogFileLogSource;
using system_logs::SystemLogsSource;

SingleLogSourceFactory::CreateCallback* g_callback = nullptr;

}  // namespace

// static
std::unique_ptr<SystemLogsSource> SingleLogSourceFactory::CreateSingleLogSource(
    feedback_private::LogSource source_type) {
  if (g_callback)
    return g_callback->Run(source_type);

  switch (source_type) {
    case feedback_private::LOG_SOURCE_MESSAGES:
      return base::MakeUnique<system_logs::SingleLogFileLogSource>(
          SingleLogFileLogSource::SupportedSource::kMessages);
    case feedback_private::LOG_SOURCE_UILATEST:
      return base::MakeUnique<system_logs::SingleLogFileLogSource>(
          SingleLogFileLogSource::SupportedSource::kUiLatest);
    case feedback_private::LOG_SOURCE_NONE:
    default:
      NOTREACHED() << "Unknown log source type.";
      break;
  }
  return std::unique_ptr<SystemLogsSource>(nullptr);
}

// static
void SingleLogSourceFactory::SetForTesting(
    SingleLogSourceFactory::CreateCallback* callback) {
  g_callback = callback;
}

}  // namespace extensions
