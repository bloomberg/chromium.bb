// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/safe_json_parser_message_filter.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/safe_json/safe_json_parser_messages.h"
#include "content/public/utility/utility_thread.h"
#include "ipc/ipc_message.h"

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

}  // namespace

namespace safe_json {

SafeJsonParserMessageFilter::SafeJsonParserMessageFilter() {
}
SafeJsonParserMessageFilter::~SafeJsonParserMessageFilter() {
}

bool SafeJsonParserMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeJsonParserMessageFilter, message)
    IPC_MESSAGE_HANDLER(SafeJsonParserMsg_ParseJSON, OnParseJSON)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void SafeJsonParserMessageFilter::OnParseJSON(const std::string& json) {
  int error_code;
  std::string error;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, &error_code, &error);
  if (value) {
    base::ListValue wrapper;
    wrapper.Append(std::move(value));
    Send(new SafeJsonParserHostMsg_ParseJSON_Succeeded(wrapper));
  } else {
    Send(new SafeJsonParserHostMsg_ParseJSON_Failed(error));
  }

  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

}  // namespace safe_json
