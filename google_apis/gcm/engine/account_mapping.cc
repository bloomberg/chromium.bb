// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/account_mapping.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace gcm {

namespace {

const char kSeparator[] = "&";
uint32 kEmailIndex = 0;
uint32 kMappingChangeTimestampIndex = 1;
uint32 kMessageTypeIndex = 2;
uint32 kMessageIdIndex = 3;
uint32 kSizeWithNoMessage = kMessageTypeIndex + 1;
uint32 kSizeWithMessage = kMessageIdIndex + 1;

const char kMessageTypeNoneString[] = "none";
const char kMessageTypeAddString[] = "add";
const char kMessageTypeRemoveString[] = "remove";

std::string MessageTypeToString(AccountMapping::MessageType type) {
  switch (type) {
    case AccountMapping::MSG_NONE:
      return kMessageTypeNoneString;
    case AccountMapping::MSG_ADD:
      return kMessageTypeAddString;
    case AccountMapping::MSG_REMOVE:
      return kMessageTypeRemoveString;
    default:
      NOTREACHED();
  }
  return std::string();
}

bool StringToMessageType(const std::string& type_str,
                         AccountMapping::MessageType* type) {
  if (type_str.compare(kMessageTypeAddString) == 0)
    *type = AccountMapping::MSG_ADD;
  else if (type_str.compare(kMessageTypeRemoveString) == 0)
    *type = AccountMapping::MSG_REMOVE;
  else if (type_str.compare(kMessageTypeNoneString) == 0)
    *type = AccountMapping::MSG_NONE;
  else
    return false;

  return true;
}

}  // namespace

AccountMapping::AccountMapping() {
}

AccountMapping::~AccountMapping() {
}

std::string AccountMapping::SerializeAsString() const {
  std::string value;
  value.append(email);
  value.append(kSeparator);
  value.append(base::Int64ToString(status_change_timestamp.ToInternalValue()));
  value.append(kSeparator);
  value.append(MessageTypeToString(last_message_type));
  if (last_message_type != MSG_NONE) {
    value.append(kSeparator);
    value.append(last_message_id);
  }

  return value;
}

bool AccountMapping::ParseFromString(const std::string& value) {
  std::vector<std::string> values;
  Tokenize(value, kSeparator, &values);
  if (values.size() != kSizeWithNoMessage &&
      values.size() != kSizeWithMessage) {
    return false;
  }

  if (values[kEmailIndex].empty() ||
      values[kMappingChangeTimestampIndex].empty() ||
      values[kMessageTypeIndex].empty()) {
    return false;
  }

  if (values.size() == kSizeWithMessage && values[kMessageIdIndex].empty()) {
    return false;
  }

  MessageType message_type;
  if (!StringToMessageType(values[kMessageTypeIndex], &message_type))
    return false;

  if ((message_type == MSG_NONE && values.size() == kSizeWithMessage) ||
      (message_type != MSG_NONE && values.size() != kSizeWithMessage)) {
    return false;
  }

  last_message_type = message_type;

  int64 status_change_ts_internal = 0LL;
  if (!base::StringToInt64(values[kMappingChangeTimestampIndex],
                           &status_change_ts_internal)) {
    return false;
  }

  if (status_change_ts_internal == 0LL)
    status = ADDING;
  else if (last_message_type == MSG_REMOVE)
    status = REMOVING;
  else
    status = MAPPED;

  if (values.size() == kSizeWithMessage)
    last_message_id = values[kMessageIdIndex];
  else
    last_message_id.clear();

  email = values[kEmailIndex];
  status_change_timestamp =
      base::Time::FromInternalValue(status_change_ts_internal);
  access_token.clear();

  return true;
}

}  // namespace gcm
