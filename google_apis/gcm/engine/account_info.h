// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_ACCOUNT_INFO_H_
#define GOOGLE_APIS_GCM_ENGINE_ACCOUNT_INFO_H_

#include <string>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"

namespace gcm {

// Stores information about Account and a last message sent with the information
// about that account.
struct GCM_EXPORT AccountInfo {
  // Indicates whether a message, if sent, was adding or removing account
  // mapping.
  enum MessageType {
    MSG_NONE,    // No message has been sent.
    MSG_ADD,     // Account was mapped to device by the message.
    MSG_REMOVE,  // Account mapping to device was removed by the message.
  };

  AccountInfo();
  ~AccountInfo();

  // Serializes account info to string without |account_id|.
  std::string SerializeAsString() const;
  // Parses account info from store, without |account_id|.
  bool ParseFromString(const std::string& value);

  // Gaia ID of the account.
  std::string account_id;
  // Email address of the tracked account.
  std::string email;
  // Type of the last mapping message sent to GCM.
  MessageType last_message_type;
  // ID of the last mapping message sent to GCM.
  std::string last_message_id;
  // Timestamp of when the last mapping message was sent to GCM.
  base::Time last_message_timestamp;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_ACCOUNT_INFO_H_
