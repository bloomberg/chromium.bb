// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/libs/message/message_builder.h"

#include <stdlib.h>
#include <string.h>

#include <limits>

#include "base/logging.h"
#include "mojo/public/libs/message/message.h"

namespace mojo {
namespace {

const size_t kAlignment = 8U;
const size_t kMessageHeaderSize = 8U;

}  // namespace

MessageBuilder::MessageBuilder() {
}

MessageBuilder::~MessageBuilder() {
}

bool MessageBuilder::Initialize(uint32_t message_name) {
  uint32_t message_header[2] = {
    0,  // Length in bytes to be filled out in Finish.
    message_name
  };
  COMPILE_ASSERT(kMessageHeaderSize == sizeof(message_header),
                 unexpected_message_header_size);

  return AppendRawBytes(&message_header, sizeof(message_header));
}

bool MessageBuilder::Append(uint16_t name, int8_t value) {
  return AppendU32(name, kMessageFieldType_Int8, static_cast<uint8_t>(value));
}

bool MessageBuilder::Append(uint16_t name, int16_t value) {
  return AppendU32(name, kMessageFieldType_Int16, static_cast<uint16_t>(value));
}

bool MessageBuilder::Append(uint16_t name, int32_t value) {
  return AppendU32(name, kMessageFieldType_Int32, static_cast<uint32_t>(value));
}

bool MessageBuilder::Append(uint16_t name, int64_t value) {
  return AppendU64(name, kMessageFieldType_Int64, static_cast<uint64_t>(value));
}

bool MessageBuilder::Append(uint16_t name, uint8_t value) {
  return AppendU32(name, kMessageFieldType_Uint8, value);
}

bool MessageBuilder::Append(uint16_t name, uint16_t value) {
  return AppendU32(name, kMessageFieldType_Uint16, value);
}

bool MessageBuilder::Append(uint16_t name, uint32_t value) {
  return AppendU32(name, kMessageFieldType_Uint32, value);
}

bool MessageBuilder::Append(uint16_t name, uint64_t value) {
  return AppendU64(name, kMessageFieldType_Uint64, value);
}

bool MessageBuilder::Append(uint16_t name, const std::string& value) {
  return AppendArray(name, kMessageFieldType_String, value.data(),
                     value.size());
}

bool MessageBuilder::Append(uint16_t name, const void* bytes,
                            size_t num_bytes) {
  return AppendArray(name, kMessageFieldType_Bytes, bytes, num_bytes);
}

bool MessageBuilder::Append(uint16_t name, Handle handle) {
  uint32_t index = static_cast<uint32_t>(handles_.size());

  if (!AppendU32(name, kMessageFieldType_Handle, index))
    return false;

  handles_.push_back(handle);

  // TODO(darin): Should we even worry about memory allocation failure?
  return handles_.size() == index + 1;
}

bool MessageBuilder::Append(uint16_t name, const Message& message) {
  if (!AppendArray(name, kMessageFieldType_Message, &message.data()[0],
                   message.data().size()))
    return false;

  size_t size_before = handles_.size();
  handles_.insert(handles_.end(),
                  message.handles().begin(),
                  message.handles().end());

  // TODO(darin): Should we even worry about memory allocation failure?
  return handles_.size() == size_before + message.handles().size();
}

bool MessageBuilder::Finish(Message* message) {
  if (data_.empty())
    return false;

  // Initialize should have been called.
  DCHECK(data_.size() >= kMessageHeaderSize);

  uint32_t payload_size =
      static_cast<uint32_t>(data_.size() - sizeof(uint32_t));

  // Stamp payload size into the message.
  memcpy(&data_[0], &payload_size, sizeof(payload_size));

  message->data_.swap(data_);
  message->handles_.swap(handles_);
  return true;
}

bool MessageBuilder::AppendU32(uint16_t name, MessageFieldType field_type,
                               uint32_t value) {
  return AppendHeader(name, field_type) &&
         AppendRawBytes(&value, sizeof(value));
}

bool MessageBuilder::AppendU64(uint16_t name, MessageFieldType field_type,
                               uint64_t value) {
  // Insert padding to achieve 8-byte alignment.
  return AppendHeader(name, field_type) &&
         AppendPadding(4U) &&
         AppendRawBytes(&value, sizeof(value));
}

bool MessageBuilder::AppendArray(uint16_t name, MessageFieldType field_type,
                                 const void* bytes, size_t num_bytes) {
  if (num_bytes > std::numeric_limits<uint32_t>::max())
    return false;
  uint32_t size = static_cast<uint32_t>(num_bytes);

  // Append padding to achieve 8-byte alignment.
  size_t padding_size = kAlignment - num_bytes % kAlignment;

  return AppendHeader(name, field_type) &&
         AppendRawBytes(&size, sizeof(size)) &&
         AppendRawBytes(bytes, size) &&
         AppendPadding(padding_size);
}

bool MessageBuilder::AppendHeader(uint16_t name, MessageFieldType field_type) {
  // Initialize should have been called.
  DCHECK(data_.size() >= kMessageHeaderSize);

  uint32_t header = static_cast<uint32_t>(name) << 16 |
                    static_cast<uint32_t>(field_type);
  return AppendRawBytes(&header, sizeof(header));
}

bool MessageBuilder::AppendPadding(size_t num_bytes) {
  if (num_bytes == 0)
    return true;
  DCHECK(num_bytes < kAlignment);

  const uint8_t kNulls[kAlignment - 1] = { 0 };
  return AppendRawBytes(kNulls, num_bytes);
}

bool MessageBuilder::AppendRawBytes(const void* bytes, size_t num_bytes) {
  if (!num_bytes)
    return true;

  const uint8_t* data_start = static_cast<const uint8_t*>(bytes);
  const uint8_t* data_end = data_start + num_bytes;

  size_t size_before = data_.size();

  // TODO(darin): Call reserve() here?
  data_.insert(data_.end(), data_start, data_end);

  // TODO(darin): Should we even worry about memory allocation failure?
  return data_.size() == size_before + num_bytes;
}

}  // namespace mojo
