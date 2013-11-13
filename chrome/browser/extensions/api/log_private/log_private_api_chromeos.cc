// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/log_private/log_private_api.h"

#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/system_logs/about_system_logs_fetcher.h"
#include "chrome/browser/chromeos/system_logs/scrubbed_system_logs_fetcher.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/browser/extensions/api/log_private/syslog_parser.h"
#include "chrome/common/extensions/api/log_private.h"
#include "extensions/browser/extension_function.h"

namespace extensions {
namespace {

scoped_ptr<LogParser> CreateLogParser(const std::string& log_type) {
  if (log_type == "syslog")
    return scoped_ptr<LogParser>(new SyslogParser());
  // TODO(shinfan): Add more parser here

  NOTREACHED() << "Invalid log type: " << log_type;
  return  scoped_ptr<LogParser>();
}

void CollectLogInfo(
    FilterHandler* filter_handler,
    chromeos::SystemLogsResponse* logs,
    std::vector<linked_ptr<api::log_private::LogEntry> >* output) {
  for (chromeos::SystemLogsResponse::const_iterator request_it = logs->begin();
       request_it != logs->end();
       ++request_it) {
    if (!filter_handler->IsValidSource(request_it->first)) {
      continue;
    }
    scoped_ptr<LogParser> parser(CreateLogParser(request_it->first));
    if (parser) {
      parser->Parse(request_it->second, output, filter_handler);
    }
  }
}

}  // namespace

LogPrivateGetHistoricalFunction::LogPrivateGetHistoricalFunction() {
}

LogPrivateGetHistoricalFunction::~LogPrivateGetHistoricalFunction() {
}

bool LogPrivateGetHistoricalFunction::RunImpl() {
  // Get parameters
  scoped_ptr<api::log_private::GetHistorical::Params> params(
      api::log_private::GetHistorical::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  filter_handler_.reset(new FilterHandler(params->filter));

  chromeos::SystemLogsFetcherBase* fetcher;
  if ((params->filter).scrub) {
    fetcher = new chromeos::ScrubbedSystemLogsFetcher();
  } else {
    fetcher = new chromeos::AboutSystemLogsFetcher();
  }
  fetcher->Fetch(
      base::Bind(&LogPrivateGetHistoricalFunction::OnSystemLogsLoaded, this));

  return true;
}

void LogPrivateGetHistoricalFunction::OnSystemLogsLoaded(
    scoped_ptr<chromeos::SystemLogsResponse> sys_info) {
  std::vector<linked_ptr<api::log_private::LogEntry> > data;

  CollectLogInfo(filter_handler_.get(), sys_info.get(), &data);

  // Prepare result
  api::log_private::Result result;
  result.data = data;
  api::log_private::Filter::Populate(
      *((filter_handler_->GetFilter())->ToValue()), &result.filter);
  SetResult(result.ToValue().release());
  SendResponse(true);
}

}  // namespace extensions
