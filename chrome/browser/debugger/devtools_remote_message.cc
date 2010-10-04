// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_remote_message.h"

#include "base/string_number_conversions.h"

const char DevToolsRemoteMessageHeaders::kContentLength[] = "Content-Length";
const char DevToolsRemoteMessageHeaders::kTool[] = "Tool";
const char DevToolsRemoteMessageHeaders::kDestination[] = "Destination";

const char DevToolsRemoteMessage::kEmptyValue[] = "";

DevToolsRemoteMessageBuilder& DevToolsRemoteMessageBuilder::instance() {
  static DevToolsRemoteMessageBuilder instance_;
  return instance_;
}

DevToolsRemoteMessage::DevToolsRemoteMessage() {}

DevToolsRemoteMessage::DevToolsRemoteMessage(const HeaderMap& headers,
                                             const std::string& content)
    : header_map_(headers),
      content_(content) {
}

DevToolsRemoteMessage::~DevToolsRemoteMessage() {}

const std::string DevToolsRemoteMessage::GetHeader(
    const std::string& header_name,
    const std::string& default_value) const {
  HeaderMap::const_iterator it = header_map_.find(header_name);
  if (it == header_map_.end()) {
    return default_value;
  }
  return it->second;
}

const std::string DevToolsRemoteMessage::GetHeaderWithEmptyDefault(
    const std::string& header_name) const {
  return GetHeader(header_name, DevToolsRemoteMessage::kEmptyValue);
}

const std::string DevToolsRemoteMessage::ToString() const {
  std::string result;
  for (HeaderMap::const_iterator it = header_map_.begin(),
      end = header_map_.end(); it != end; ++it) {
    result.append(it->first).append(":").append(it->second).append("\r\n");
  }
  result.append("\r\n").append(content_);
  return result;
}

DevToolsRemoteMessage* DevToolsRemoteMessageBuilder::Create(
    const std::string& tool,
    const std::string& destination,
    const std::string& content) {
  DevToolsRemoteMessage::HeaderMap headers;
  headers[DevToolsRemoteMessageHeaders::kContentLength] =
      base::IntToString(content.size());
  headers[DevToolsRemoteMessageHeaders::kTool] = tool;
  headers[DevToolsRemoteMessageHeaders::kDestination] = destination;
  return new DevToolsRemoteMessage(headers, content);
}
