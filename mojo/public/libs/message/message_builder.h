// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_LIBS_MESSAGE_MESSAGE_BUIDER_H_
#define MOJO_PUBLIC_LIBS_MESSAGE_MESSAGE_BUIDER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "mojo/public/libs/message/message.h"

namespace mojo {

class MessageBuilder {
 public:
  MessageBuilder();
  ~MessageBuilder();

  bool Initialize(uint32_t message_name);

  bool Append(uint16_t name, bool value);

  bool Append(uint16_t name, int8_t value);
  bool Append(uint16_t name, int16_t value);
  bool Append(uint16_t name, int32_t value);
  bool Append(uint16_t name, int64_t value);

  bool Append(uint16_t name, uint8_t value);
  bool Append(uint16_t name, uint16_t value);
  bool Append(uint16_t name, uint32_t value);
  bool Append(uint16_t name, uint64_t value);

  bool Append(uint16_t name, float value);
  bool Append(uint16_t name, double value);

  bool Append(uint16_t name, const std::string& value);
  bool Append(uint16_t name, const void* bytes, size_t num_bytes);

  template <typename C>
  bool Append(uint16_t name, const C& container) {
    return Append(name, container.data(), container.size());
  }

  // TODO(darin): Add support for arrays of other types.

  bool Append(uint16_t name, Handle handle);
  bool Append(uint16_t name, const Message& message);

  bool Finish(Message* message);

 private:
  bool AppendU32(uint16_t name, MessageFieldType field_type, uint32_t value);
  bool AppendU64(uint16_t name, MessageFieldType field_type, uint64_t value);
  bool AppendArray(uint16_t name, MessageFieldType field_type,
                   const void* bytes, size_t num_bytes);

  bool AppendHeader(uint16_t name, MessageFieldType field_type);
  bool AppendPadding(size_t num_bytes);
  bool AppendRawBytes(const void* bytes, size_t num_bytes);

  std::vector<uint8_t> data_;
  std::vector<Handle> handles_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_LIBS_MESSAGE_BUILDER_H_
