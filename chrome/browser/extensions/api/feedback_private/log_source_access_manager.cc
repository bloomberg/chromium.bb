// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/log_source_access_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/extensions/api/feedback_private/log_source_resource.h"
#include "chrome/browser/extensions/api/feedback_private/single_log_source_factory.h"
#include "extensions/browser/api/api_resource_manager.h"

namespace extensions {

namespace {

namespace feedback_private = api::feedback_private;
using feedback_private::LogSource;
using SingleLogSource = system_logs::SingleLogSource;
using SupportedSource = system_logs::SingleLogSource::SupportedSource;
using SystemLogsResponse = system_logs::SystemLogsResponse;

const int kMaxReadersPerSource = 10;

// The minimum time between consecutive reads of a log source by a particular
// extension.
const int kDefaultRateLimitingTimeoutMs = 1000;

// If this is null, then |kDefaultRateLimitingTimeoutMs| is used as the timeout.
const base::TimeDelta* g_rate_limiting_timeout = nullptr;

base::TimeDelta GetMinTimeBetweenReads() {
  return g_rate_limiting_timeout
             ? *g_rate_limiting_timeout
             : base::TimeDelta::FromMilliseconds(kDefaultRateLimitingTimeoutMs);
}

// Converts from feedback_private::LogSource to SupportedSource.
SupportedSource GetSupportedSourceType(LogSource source) {
  switch (source) {
    case feedback_private::LOG_SOURCE_MESSAGES:
      return SupportedSource::kMessages;
    case feedback_private::LOG_SOURCE_UILATEST:
      return SupportedSource::kUiLatest;
    case feedback_private::LOG_SOURCE_NONE:
    default:
      NOTREACHED() << "Unknown log source type.";
      return SingleLogSource::SupportedSource::kMessages;
  }
  NOTREACHED();
  return SingleLogSource::SupportedSource::kMessages;
}

// SystemLogsResponse is a map of strings -> strings. The map value has the
// actual log contents, a string containing all lines, separated by newlines.
// This function extracts the individual lines and converts them into a vector
// of strings, each string containing a single line.
void GetLogLinesFromSystemLogsResponse(const SystemLogsResponse& response,
                                       std::vector<std::string>* log_lines) {
  for (const std::pair<std::string, std::string>& pair : response) {
    std::vector<std::string> new_lines = base::SplitString(
        pair.second, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    log_lines->reserve(log_lines->size() + new_lines.size());
    log_lines->insert(log_lines->end(), new_lines.begin(), new_lines.end());
  }
}

}  // namespace

LogSourceAccessManager::LogSourceAccessManager(content::BrowserContext* context)
    : context_(context),
      tick_clock_(new base::DefaultTickClock),
      weak_factory_(this) {}

LogSourceAccessManager::~LogSourceAccessManager() {}

// static
void LogSourceAccessManager::SetRateLimitingTimeoutForTesting(
    const base::TimeDelta* timeout) {
  g_rate_limiting_timeout = timeout;
}

bool LogSourceAccessManager::FetchFromSource(
    const feedback_private::ReadLogSourceParams& params,
    const std::string& extension_id,
    const ReadLogSourceCallback& callback) {
  SourceAndExtension key = SourceAndExtension(params.source, extension_id);
  int requested_resource_id = params.reader_id ? *params.reader_id : 0;
  int resource_id =
      requested_resource_id > 0 ? requested_resource_id : CreateResource(key);
  if (resource_id <= 0)
    return false;

  ApiResourceManager<LogSourceResource>* resource_manager =
      ApiResourceManager<LogSourceResource>::Get(context_);
  LogSourceResource* resource =
      resource_manager->Get(extension_id, resource_id);
  if (!resource)
    return false;

  // Enforce the rules: rate-limit access to the source from the current
  // extension. If not enough time has elapsed since the last access, do not
  // read from the source, but instead return an empty response. From the
  // caller's perspective, there is no new data. There is no need for the caller
  // to keep track of the time since last access.
  if (!UpdateSourceAccessTime(key)) {
    feedback_private::ReadLogSourceResult empty_result;
    callback.Run(empty_result);
    return true;
  }

  // If the API call requested a non-incremental access, clean up the
  // SingleLogSource by removing its API resource. Even if the existing source
  // were originally created as incremental, passing in incremental=false on a
  // later access indicates that the source should be closed afterwards.
  bool delete_resource_when_done = !params.incremental;

  resource->GetLogSource()->Fetch(base::Bind(
      &LogSourceAccessManager::OnFetchComplete, weak_factory_.GetWeakPtr(), key,
      delete_resource_when_done, callback));
  return true;
}

void LogSourceAccessManager::OnFetchComplete(
    const SourceAndExtension& key,
    bool delete_resource,
    const ReadLogSourceCallback& callback,
    SystemLogsResponse* response) {
  int resource_id = 0;
  const auto iter = sources_.find(key);
  if (iter != sources_.end())
    resource_id = iter->second;

  feedback_private::ReadLogSourceResult result;
  // Always return reader_id=0 if there is a cleanup.
  result.reader_id = delete_resource ? 0 : resource_id;

  GetLogLinesFromSystemLogsResponse(*response, &result.log_lines);
  if (delete_resource) {
    // This should also remove the entry from |sources_|.
    ApiResourceManager<LogSourceResource>::Get(context_)->Remove(
        key.extension_id, resource_id);
  }

  callback.Run(result);
}

void LogSourceAccessManager::RemoveSource(const SourceAndExtension& key) {
  sources_.erase(key);
}

LogSourceAccessManager::SourceAndExtension::SourceAndExtension(
    api::feedback_private::LogSource source,
    const std::string& extension_id)
    : source(source), extension_id(extension_id) {}

int LogSourceAccessManager::CreateResource(const SourceAndExtension& key) {
  // Enforce the rules: Do not create a new SingleLogSource if there was already
  // one created for |key|.
  if (sources_.find(key) != sources_.end())
    return 0;

  // Enforce the rules: Do not create too many SingleLogSource objects to read
  // from a source, even if they are from different extensions.
  if (GetNumActiveResourcesForSource(key.source) >= kMaxReadersPerSource)
    return 0;

  std::unique_ptr<LogSourceResource> new_resource =
      base::MakeUnique<LogSourceResource>(
          key.extension_id,
          SingleLogSourceFactory::CreateSingleLogSource(
              GetSupportedSourceType(key.source)),
          base::Bind(&LogSourceAccessManager::RemoveSource,
                     weak_factory_.GetWeakPtr(), key));

  int id = ApiResourceManager<LogSourceResource>::Get(context_)->Add(
      new_resource.release());
  sources_[key] = id;

  return id;
}

bool LogSourceAccessManager::UpdateSourceAccessTime(
    const SourceAndExtension& key) {
  base::TimeTicks last = GetLastExtensionAccessTime(key);
  base::TimeTicks now = tick_clock_->NowTicks();
  if (!last.is_null() && now < last + GetMinTimeBetweenReads()) {
    return false;
  }
  last_access_times_[key] = now;
  return true;
}

base::TimeTicks LogSourceAccessManager::GetLastExtensionAccessTime(
    const SourceAndExtension& key) const {
  const auto iter = last_access_times_.find(key);
  if (iter == last_access_times_.end())
    return base::TimeTicks();

  return iter->second;
}

size_t LogSourceAccessManager::GetNumActiveResourcesForSource(
    api::feedback_private::LogSource source) const {
  size_t count = 0;
  // The stored entries are sorted first by source type, then by extension ID.
  // We can take advantage of this fact to avoid iterating over all elements.
  // Instead start from the first element that matches |source|, and end at the
  // first element that does not match |source| anymore.
  for (auto iter = sources_.lower_bound(SourceAndExtension(source, ""));
       iter != sources_.end() && iter->first.source == source; ++iter) {
    ++count;
  }
  return count;
}

}  // namespace extensions
