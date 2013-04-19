// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_logger.h"
#include "chrome/test/chromedriver/chrome/status.h"

DevToolsEventLogger::DevToolsEventLogger(
    const std::string& log_type,
    const std::vector<std::string>& domains,
    const std::string& logging_level)
: log_type_(log_type),
  domains_(domains),
  logging_level_(logging_level),
  log_entries_(new base::ListValue()) {
  VLOG(1) << "DevToolsEventLogger(" <<
      log_type_ << ", " << logging_level_ << ")";
}

DevToolsEventLogger::~DevToolsEventLogger() {
  VLOG(1) << "Log type '" << log_type_ <<
      "' lost " << log_entries_->GetSize() << " entries on destruction";
}

const std::string& DevToolsEventLogger::GetLogType() {
  return log_type_;
}

static const std::string GetDomainEnableCommand(const std::string& domain) {
  if ("Timeline" == domain) {
    return "Timeline.start";
  } else {
    return domain + ".enable";
  }
}

Status DevToolsEventLogger::OnConnected(DevToolsClient* client) {
  base::DictionaryValue params;  // All our enable commands have empty params.
  scoped_ptr<base::DictionaryValue> result;
  for (std::vector<std::string>::const_iterator domain = domains_.begin();
       domain != domains_.end(); ++domain) {
    const std::string command = GetDomainEnableCommand(*domain);
    VLOG(1) << "Log type '" << log_type_ << "' sending command: " << command;
    Status status = client->SendCommandAndGetResult(command, params, &result);
    // The client may or may not be connected, e.g. from AddDevToolsClient().
    if (status.IsError() && status.code() != kDisconnected) {
      return status;
    }
  }
  return Status(kOk);
}

scoped_ptr<base::ListValue> DevToolsEventLogger::GetAndClearLogEntries() {
  scoped_ptr<base::ListValue> ret(log_entries_.release());
  log_entries_.reset(new base::ListValue());
  return ret.Pass();
}

bool DevToolsEventLogger::ShouldLogEvent(const std::string& method) {
  for (std::vector<std::string>::const_iterator domain = domains_.begin();
       domain != domains_.end(); ++domain) {
    size_t prefix_len = domain->length();
    if (method.length() > prefix_len && method[prefix_len] == '.' &&
        StartsWithASCII(method, *domain, true)) {
      return true;
    }
  }
  return false;
}

scoped_ptr<DictionaryValue> DevToolsEventLogger::GetLogEntry(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  // Get the log event timestamp ASAP. TODO(klm): extract from params?
  double timestamp_epoch_ms = base::Time::Now().ToJsTime();

  // Form JSON with a writer, verified same performance as concatenation.
  base::DictionaryValue log_message_dict;
  log_message_dict.SetString("webview", client->GetId());
  log_message_dict.SetString("message.method", method);
  log_message_dict.Set("message.params", params.DeepCopy());
  std::string log_message_json;
  base::JSONWriter::Write(&log_message_dict, &log_message_json);

  scoped_ptr<base::DictionaryValue> log_entry_dict(new base::DictionaryValue());
  log_entry_dict->SetDouble("timestamp",
                            static_cast<int64>(timestamp_epoch_ms));
  log_entry_dict->SetString("level", logging_level_);
  log_entry_dict->SetString("message", log_message_json);
  return log_entry_dict.Pass();
}

void DevToolsEventLogger::OnEvent(DevToolsClient* client,
                                  const std::string& method,
                                  const base::DictionaryValue& params) {
  if (!ShouldLogEvent(method)) {
    return;
  }

  scoped_ptr<DictionaryValue> entry = GetLogEntry(client, method, params);
  log_entries_->Append(entry.release());
}
