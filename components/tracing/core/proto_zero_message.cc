// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/proto_zero_message.h"

#include <string.h>

#include "components/tracing/core/proto_utils.h"
#include "components/tracing/core/proto_zero_message_handle.h"

#if !defined(ARCH_CPU_LITTLE_ENDIAN)
// The memcpy() for float and double below needs to be adjusted if we want to
// support big endian CPUs. There doesn't seem to be a compelling need today.
#error big-endian CPUs not supported by this translation unit.
#endif

namespace tracing {
namespace v2 {

ProtoZeroMessage::ProtoZeroMessage() {
  // Do NOT add any code here, use the Reset() method below instead.
  // Ctor and Dtor of ProtoZeroMessage are never called, with the exeception
  // of root (non-nested) messages. Nested messages are allocated in the
  // |nested_messages_arena_| and implictly destroyed when the arena of the
  // root message goes away. This is fine as long as all the fields are PODs,
  // hence the static_assert below.
  static_assert(base::is_trivially_destructible<ProtoZeroMessage>::value,
                "ProtoZeroMessage must be trivially destructible");

  static_assert(
      sizeof(ProtoZeroMessage::nested_messages_arena_) >=
          kMaxNestingDepth * (sizeof(ProtoZeroMessage) -
                              sizeof(ProtoZeroMessage::nested_messages_arena_)),
      "ProtoZeroMessage::nested_messages_arena_ is too small");
}

// This method is called to initialize both root and nested messages.
void ProtoZeroMessage::Reset(ScatteredStreamWriter* stream_writer) {
  stream_writer_ = stream_writer;
  size_ = 0;
  size_field_.reset();
  size_already_written_ = 0;
  nested_message_ = nullptr;
  nesting_depth_ = 0;
#if DCHECK_IS_ON()
  sealed_ = false;
  handle_ = nullptr;
#endif
}

void ProtoZeroMessage::AppendVarIntU64(uint32_t field_id, uint64_t value) {
  if (nested_message_)
    EndNestedMessage();

  uint8_t data[proto::GetMaxVarIntEncodedSize<uint32_t>() +
               proto::GetMaxVarIntEncodedSize<uint64_t>()];
  uint8_t* data_end;
  data_end = proto::WriteVarIntU32(proto::MakeTagVarInt(field_id), data);
  data_end = proto::WriteVarIntU64(value, data_end);
  WriteToStream(data, data_end);
}

void ProtoZeroMessage::AppendVarIntU32(uint32_t field_id, uint32_t value) {
  // TODO(kraynov): this could be perf-optimized. See http://crbug.com/624311 .
  AppendVarIntU64(field_id, value);
}

void ProtoZeroMessage::AppendFloat(uint32_t field_id, float value) {
  if (nested_message_)
    EndNestedMessage();

  uint8_t data[proto::GetMaxVarIntEncodedSize<uint32_t>() + sizeof(value)];
  uint8_t* data_end;
  data_end = proto::WriteVarIntU32(proto::MakeTagFixed32(field_id), data);
  memcpy(data_end, &value, sizeof(value));
  data_end += sizeof(value);
  WriteToStream(data, data_end);
}

void ProtoZeroMessage::AppendDouble(uint32_t field_id, double value) {
  if (nested_message_)
    EndNestedMessage();

  uint8_t data[proto::GetMaxVarIntEncodedSize<uint32_t>() + sizeof(value)];
  uint8_t* data_end;
  data_end = proto::WriteVarIntU32(proto::MakeTagFixed64(field_id), data);
  memcpy(data_end, &value, sizeof(value));
  data_end += sizeof(value);
  WriteToStream(data, data_end);
}

void ProtoZeroMessage::AppendString(uint32_t field_id, const char* str) {
  AppendBytes(field_id, str, strlen(str));
}

void ProtoZeroMessage::AppendBytes(uint32_t field_id,
                                   const void* src,
                                   size_t size) {
  if (nested_message_)
    EndNestedMessage();

  // Write the proto preamble (field id, type and length of the buffer).
  uint8_t data[2 * proto::GetMaxVarIntEncodedSize<uint32_t>()];
  uint8_t* data_end =
      proto::WriteVarIntU32(proto::MakeTagLengthDelimited(field_id), data);
  data_end = proto::WriteVarIntU32(static_cast<uint32_t>(size), data_end);
  WriteToStream(data, data_end);
  const uint8_t* src_u8 = reinterpret_cast<const uint8_t*>(src);
  WriteToStream(src_u8, src_u8 + size);
}

size_t ProtoZeroMessage::Finalize() {
  if (nested_message_)
    EndNestedMessage();

  if (size_field_.is_valid()) {
// Write the length of the nested message a posteriori, using a leading-zero
// redundant varint encoding.
#if DCHECK_IS_ON()
    DCHECK(!sealed_);
#endif
    DCHECK_LT(size_, proto::kMaxMessageLength);
    DCHECK_EQ(proto::kMessageLengthFieldSize, size_field_.size());
    proto::WriteRedundantVarIntU32<proto::kMessageLengthFieldSize>(
        static_cast<uint32_t>(size_ - size_already_written_),
        size_field_.begin);
    size_field_.reset();
  }

#if DCHECK_IS_ON()
  sealed_ = true;
  if (handle_)
    handle_->reset_message();
#endif

  return size_;
}

void ProtoZeroMessage::BeginNestedMessageInternal(uint32_t field_id,
                                                  ProtoZeroMessage* message) {
  if (nested_message_)
    EndNestedMessage();

  // Write the proto preamble for the nested message.
  uint8_t data[proto::GetMaxVarIntEncodedSize<uint32_t>()];
  uint8_t* data_end =
      proto::WriteVarIntU32(proto::MakeTagLengthDelimited(field_id), data);
  WriteToStream(data, data_end);

  message->Reset(stream_writer_);
  CHECK_LT(nesting_depth_, kMaxNestingDepth);
  message->nesting_depth_ = nesting_depth_ + 1;

  // The length of the nested message cannot be known upfront. So right now
  // just reserve the bytes to encode the size after the nested message is done.
  message->set_size_field(
      stream_writer_->ReserveBytes(proto::kMessageLengthFieldSize));
  size_ += proto::kMessageLengthFieldSize;
  nested_message_ = message;
}

void ProtoZeroMessage::EndNestedMessage() {
  size_ += nested_message_->Finalize();
  nested_message_ = nullptr;
}

}  // namespace v2
}  // namespace tracing
