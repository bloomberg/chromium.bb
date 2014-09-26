// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/wire_message.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"

// The wire messages have a simple format:
// [ message version ] [ body length ] [ JSON body ]
//       1 byte            2 bytes      body length
// The JSON body contains two fields: an optional permit_id field and a required
// data field.

namespace proximity_auth {
namespace {

// The length of the message header, in bytes.
const size_t kHeaderLength = 3;

// The protocol version of the message format.
const int kExpectedMessageFormatVersion = 3;

const char kPayloadKey[] = "payload";
const char kPermitIdKey[] = "permit_id";

// Parses the |serialized_message|'s header. Returns |true| iff the message has
// a valid header, is complete, and is well-formed according to the header. Sets
// |is_incomplete_message| to true iff the message does not have enough data to
// parse the header, or if the message length encoded in the message header
// exceeds the size of the |serialized_message|.
bool ParseHeader(const std::string& serialized_message,
                 bool* is_incomplete_message) {
  *is_incomplete_message = false;
  if (serialized_message.size() < kHeaderLength) {
    *is_incomplete_message = true;
    return false;
  }

  COMPILE_ASSERT(kHeaderLength > 2, header_length_too_small);
  size_t version = serialized_message[0];
  if (version != kExpectedMessageFormatVersion) {
    VLOG(1) << "Error: Invalid message version. Got " << version
            << ", expected " << kExpectedMessageFormatVersion;
    return false;
  }

  size_t expected_body_length =
      (static_cast<size_t>(serialized_message[1]) << 8) |
      (static_cast<size_t>(serialized_message[2]) << 0);
  size_t expected_message_length = kHeaderLength + expected_body_length;
  if (serialized_message.size() < expected_message_length) {
    *is_incomplete_message = true;
    return false;
  }
  if (serialized_message.size() != expected_message_length) {
    VLOG(1) << "Error: Invalid message length. Got "
            << serialized_message.size() << ", expected "
            << expected_message_length;
    return false;
  }

  return true;
}

}  // namespace

WireMessage::~WireMessage() {
}

// static
scoped_ptr<WireMessage> WireMessage::Deserialize(
    const std::string& serialized_message,
    bool* is_incomplete_message) {
  if (!ParseHeader(serialized_message, is_incomplete_message))
    return scoped_ptr<WireMessage>();

  scoped_ptr<base::Value> body_value(
      base::JSONReader::Read(serialized_message.substr(kHeaderLength)));
  if (!body_value || !body_value->IsType(base::Value::TYPE_DICTIONARY)) {
    VLOG(1) << "Error: Unable to parse message as JSON.";
    return scoped_ptr<WireMessage>();
  }

  base::DictionaryValue* body;
  bool success = body_value->GetAsDictionary(&body);
  DCHECK(success);

  // The permit ID is optional. In the Easy Unlock protocol, only the first
  // message includes this field.
  std::string permit_id;
  body->GetString(kPermitIdKey, &permit_id);

  std::string payload_base64;
  if (!body->GetString(kPayloadKey, &payload_base64) ||
      payload_base64.empty()) {
    VLOG(1) << "Error: Missing payload.";
    return scoped_ptr<WireMessage>();
  }

  std::string payload;
  if (!base::Base64Decode(payload_base64, &payload)) {
    VLOG(1) << "Error: Invalid base64 encoding for payload.";
    return scoped_ptr<WireMessage>();
  }

  return scoped_ptr<WireMessage>(new WireMessage(permit_id, payload));
}

std::string WireMessage::Serialize() const {
  // TODO(isherman): Implement.
  return "This method is not yet implemented.";
}

WireMessage::WireMessage(const std::string& permit_id,
                         const std::string& payload)
    : permit_id_(permit_id), payload_(payload) {
}

}  // namespace proximity_auth
