// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/test/chromedriver/performance_logger.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

// DevTools event domain prefixes to intercept.
const char* const kDomains[] = {"Network.", "Page.", "Timeline."};

// Returns whether the event belongs to one of kDomains.
bool ShouldLogEvent(const std::string& method) {
  for (size_t i_domain = 0; i_domain < arraysize(kDomains); ++i_domain) {
    if (StartsWithASCII(method, kDomains[i_domain], true /* case_sensitive */))
      return true;
  }
  return false;
}

bool IsEnabled(const PerfLoggingPrefs::InspectorDomainStatus& domain_status) {
  return (domain_status ==
          PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled ||
          domain_status ==
          PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled);
}

}  // namespace

PerformanceLogger::PerformanceLogger(Log* log)
    : log_(log) {}

PerformanceLogger::PerformanceLogger(Log* log, const PerfLoggingPrefs& prefs)
    : log_(log),
      prefs_(prefs) {
  if (!prefs_.trace_categories.empty()) {
    LOG(WARNING) << "Ignoring trace categories because tracing support not yet "
                 << "implemented: " << prefs_.trace_categories;
    prefs_.trace_categories = "";
  }
}

bool PerformanceLogger::subscribes_to_browser() {
  return true;
}

Status PerformanceLogger::OnConnected(DevToolsClient* client) {
  if (client->GetId() == DevToolsClientImpl::kBrowserwideDevToolsClientId) {
    // TODO(johnmoore): Implement tracing log.
    return Status(kOk);
  }
  std::vector<std::string> enable_commands;
  if (IsEnabled(prefs_.network))
    enable_commands.push_back("Network.enable");
  if (IsEnabled(prefs_.page))
    enable_commands.push_back("Page.enable");
  if (IsEnabled(prefs_.timeline)) {
    // Timeline feed implicitly disabled when trace categories are specified.
    // So even if kDefaultEnabled, don't enable unless empty |trace_categories|.
    if (prefs_.trace_categories.empty() || prefs_.timeline ==
        PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled)
      enable_commands.push_back("Timeline.start");
  }
  for (std::vector<std::string>::const_iterator it = enable_commands.begin();
       it != enable_commands.end(); ++it) {
    base::DictionaryValue params;  // All the enable commands have empty params.
    Status status = client->SendCommand(*it, params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status PerformanceLogger::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (!ShouldLogEvent(method))
    return Status(kOk);

  base::DictionaryValue log_message_dict;
  log_message_dict.SetString("webview", client->GetId());
  log_message_dict.SetString("message.method", method);
  log_message_dict.Set("message.params", params.DeepCopy());
  std::string log_message_json;
  // TODO(klm): extract timestamp from params?
  // Look at where it is for Page, Network, Timeline events.
  base::JSONWriter::Write(&log_message_dict, &log_message_json);

  log_->AddEntry(Log::kInfo, log_message_json);
  return Status(kOk);
}

// TODO(johnmoore): Use BeforeCommand to implement tracing log.
Status PerformanceLogger::BeforeCommand(const std::string& command_name) {
  return Status(kOk);
}
