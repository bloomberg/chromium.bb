// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/chrome_net_log.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/net_log/net_export_file_writer.h"
#include "components/version_info/version_info.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "net/log/trace_net_log_observer.h"

namespace net_log {

ChromeNetLog::ChromeNetLog() {
  trace_net_log_observer_.reset(new net::TraceNetLogObserver());
  trace_net_log_observer_->WatchForTraceStart(this);
}

ChromeNetLog::~ChromeNetLog() {
  net_export_file_writer_.reset();
  ClearFileNetLogObserver();
  trace_net_log_observer_->StopWatchForTraceStart();
}

void ChromeNetLog::StartWritingToFile(
    const base::FilePath& path,
    net::NetLogCaptureMode capture_mode,
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string) {
  DCHECK(!path.empty());

  // TODO(739485): The log file does not contain about:flags data.
  file_net_log_observer_ = net::FileNetLogObserver::CreateUnbounded(
      path, GetConstants(command_line_string, channel_string));

  file_net_log_observer_->StartObserving(this, capture_mode);
}

NetExportFileWriter* ChromeNetLog::net_export_file_writer() {
  if (!net_export_file_writer_)
    net_export_file_writer_ = base::WrapUnique(new NetExportFileWriter(this));
  return net_export_file_writer_.get();
}

// static
std::unique_ptr<base::Value> ChromeNetLog::GetConstants(
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string) {
  std::unique_ptr<base::DictionaryValue> constants_dict =
      net::GetNetConstants();
  DCHECK(constants_dict);

  // Add a dictionary with the version of the client and its command line
  // arguments.
  auto dict = std::make_unique<base::DictionaryValue>();

  // We have everything we need to send the right values.
  dict->SetString("name", version_info::GetProductName());
  dict->SetString("version", version_info::GetVersionNumber());
  dict->SetString("cl", version_info::GetLastChange());
  dict->SetString("version_mod", channel_string);
  dict->SetString("official",
                  version_info::IsOfficialBuild() ? "official" : "unofficial");
  std::string os_type = base::StringPrintf(
      "%s: %s (%s)", base::SysInfo::OperatingSystemName().c_str(),
      base::SysInfo::OperatingSystemVersion().c_str(),
      base::SysInfo::OperatingSystemArchitecture().c_str());
  dict->SetString("os_type", os_type);
  dict->SetString("command_line", command_line_string);

  constants_dict->Set("clientInfo", std::move(dict));

  data_reduction_proxy::DataReductionProxyEventStore::AddConstants(
      constants_dict.get());

  return std::move(constants_dict);
}

void ChromeNetLog::ShutDownBeforeTaskScheduler() {
  // TODO(eroman): Stop in-progress net_export_file_writer_ or delete its files?

  ClearFileNetLogObserver();
}

void ChromeNetLog::ClearFileNetLogObserver() {
  if (!file_net_log_observer_)
    return;

  // TODO(739487): The log file does not contain any polled data.
  //
  // TODO(eroman): FileNetLogObserver::StopObserving() posts to the file task
  // runner to finish writing the log file. Despite that sequenced task runner
  // being marked BLOCK_SHUTDOWN, those tasks are not actually running.
  //
  // This isn't a big deal when using the unbounded logger since the log
  // loading code can handle such truncated logs. But this will need fixing
  // if switching to log formats that are not so versatile (also if adding
  // polled data).
  file_net_log_observer_->StopObserving(nullptr /*polled_data*/,
                                        base::Closure());
  file_net_log_observer_.reset();
}

}  // namespace net_log
