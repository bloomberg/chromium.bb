// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/single_debug_daemon_log_source.h"

#include "base/bind.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "components/feedback/anonymizer_tool.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

using SupportedSource = SingleDebugDaemonLogSource::SupportedSource;

// Converts a logs source type to the corresponding debugd log name.
std::string GetLogName(SupportedSource source_type) {
  switch (source_type) {
    case SupportedSource::kModetest:
      return "modetest";
    case SupportedSource::kLsusb:
      return "lsusb";
    case SupportedSource::kLspci:
      return "lspci";
    case SupportedSource::kIfconfig:
      return "ifconfig";
  }
  NOTREACHED();
  return "";
}

}  // namespace

SingleDebugDaemonLogSource::SingleDebugDaemonLogSource(
    SupportedSource source_type)
    : SystemLogsSource(GetLogName(source_type)), weak_ptr_factory_(this) {}

SingleDebugDaemonLogSource::~SingleDebugDaemonLogSource() {}

void SingleDebugDaemonLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

  client->GetLog(
      source_name(),
      base::Bind(&SingleDebugDaemonLogSource::OnFetchComplete,
                 weak_ptr_factory_.GetWeakPtr(), source_name(), callback));
}

void SingleDebugDaemonLogSource::OnFetchComplete(
    const std::string& log_name,
    const SysLogsSourceCallback& callback,
    bool success,
    const std::string& result) const {
  // |result| and |response| are the same type, but |result| is passed in from
  // DebugDaemonClient, which does not use the SystemLogsResponse alias.
  SystemLogsResponse response;
  // Return an empty result if the call to GetLog() failed.
  std::string final_result;
  if (success)
    response.emplace(log_name, feedback::AnonymizerTool().Anonymize(result));

  callback.Run(&response);
}

}  // namespace system_logs
