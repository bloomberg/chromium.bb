// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_json.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/common/automation_messages.h"

namespace {

// Util for creating a JSON error return string (dict with key
// 'error' and error string value).  No need to quote input.
std::string JSONErrorString(const std::string& err) {
  std::string prefix = "{\"error\": \"";
  std::string no_quote_err;
  std::string suffix = "\"}";

  base::JsonDoubleQuote(err, false, &no_quote_err);
  return prefix + no_quote_err + suffix;
}

}  // namespace

AutomationJSONReply::AutomationJSONReply(AutomationProvider* provider,
                                         IPC::Message* reply_message)
  : provider_(provider),
    message_(reply_message) {
}

AutomationJSONReply::~AutomationJSONReply() {
  DCHECK(!message_) << "JSON automation request not replied!";
}

void AutomationJSONReply::SendSuccess(const Value* value) {
  DCHECK(message_) << "Resending reply for JSON automation request";
  std::string json_string = "{}";
  if (value)
    base::JSONWriter::Write(value, false, &json_string);
  AutomationMsg_SendJSONRequest::WriteReplyParams(
      message_, json_string, true);
  provider_->Send(message_);
  message_ = NULL;
}

void AutomationJSONReply::SendError(const std::string& error_message) {
  DCHECK(message_) << "Resending reply for JSON automation request";
  std::string json_string = JSONErrorString(error_message);
  AutomationMsg_SendJSONRequest::WriteReplyParams(
      message_, json_string, false);
  provider_->Send(message_);
  message_ = NULL;
}
