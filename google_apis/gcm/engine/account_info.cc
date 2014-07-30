// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/account_info.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace gcm {

namespace {

const char kSeparator[] = "&";
uint32 kEmailIndex = 0;
uint32 kMessageTypeIndex = 1;
uint32 kMessageIdIndex = 2;
uint32 kMessageTimestampIndex = 3;
uint32 kSizeWithNoMessage = kMessageTypeIndex + 1;
uint32 kSizeWithMessage = kMessageTimestampIndex + 1;

std::string MessageTypeToString(AccountInfo::MessageType type) {
  switch (type) {
    case AccountInfo::MSG_NONE:
      return "none";
    case AccountInfo::MSG_ADD:
      return "add";
    case AccountInfo::MSG_REMOVE:
      return "remove";
    default:
      NOTREACHED();
  }
  return "";
}

AccountInfo::MessageType StringToMessageType(const std::string& type) {
  if (type.compare("add") == 0)
    return AccountInfo::MSG_ADD;
  if (type.compare("remove") == 0)
    return AccountInfo::MSG_REMOVE;
  return AccountInfo::MSG_NONE;
}

}  // namespace

AccountInfo::AccountInfo() {
}

AccountInfo::~AccountInfo() {
}

std::string AccountInfo::SerializeAsString() const {
  std::string value;
  value.append(email);
  value.append(kSeparator);
  value.append(MessageTypeToString(last_message_type));
  if (last_message_type != MSG_NONE) {
    value.append(kSeparator);
    value.append(last_message_id);
    value.append(kSeparator);
    value.append(base::Int64ToString(last_message_timestamp.ToInternalValue()));
  }

  return value;
}

bool AccountInfo::ParseFromString(const std::string& value) {
  std::vector<std::string> values;
  Tokenize(value, kSeparator, &values);
  if (values.size() != kSizeWithNoMessage &&
      values.size() != kSizeWithMessage) {
    return false;
  }

  if (values[kEmailIndex].empty() ||
      values[kMessageTypeIndex].empty()) {
    return false;
  }

  if (values.size() == kSizeWithMessage &&
      (values[kMessageIdIndex].empty() ||
       values[kMessageTimestampIndex].empty())) {
    return false;
  }

  if (values.size() == kSizeWithMessage) {
    int64 timestamp = 0LL;
    if (!base::StringToInt64(values[kMessageTimestampIndex], &timestamp))
      return false;

    MessageType message_type = StringToMessageType(values[kMessageTypeIndex]);
    if (message_type == MSG_NONE)
      return false;

    last_message_type = message_type;
    last_message_id = values[kMessageIdIndex];
    last_message_timestamp = base::Time::FromInternalValue(timestamp);
  } else {
    last_message_type = MSG_NONE;
    last_message_id.clear();
    last_message_timestamp = base::Time();
  }

  email = values[kEmailIndex];

  return true;
}

}  // namespace gcm
