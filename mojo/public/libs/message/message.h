// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_LIBS_MESSAGE_MESSAGE_H_
#define MOJO_PUBLIC_LIBS_MESSAGE_MESSAGE_H_

#include <stdint.h>

#include <vector>

#include "mojo/public/system/core.h"

namespace mojo {

enum MessageFieldClass {
  kMessageFieldClass_U32   = 0x1,
  kMessageFieldClass_U64   = 0x2,
  kMessageFieldClass_Array = 0x3,
};

enum MessageFieldType {
  kMessageFieldType_Bool    = 0x1 << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Int8    = 0x2 << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Int16   = 0x3 << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Int32   = 0x4 << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Int64   = 0x5 << 2 | kMessageFieldClass_U64,
  kMessageFieldType_Uint8   = 0x6 << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Uint16  = 0x7 << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Uint32  = 0x8 << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Uint64  = 0x9 << 2 | kMessageFieldClass_U64,
  kMessageFieldType_Float   = 0xA << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Double  = 0xB << 2 | kMessageFieldClass_U64,
  kMessageFieldType_String  = 0xC << 2 | kMessageFieldClass_Array,
  kMessageFieldType_Bytes   = 0xD << 2 | kMessageFieldClass_Array,
  kMessageFieldType_Handle  = 0xE << 2 | kMessageFieldClass_U32,
  kMessageFieldType_Message = 0xF << 2 | kMessageFieldClass_Array,
};

class Message {
 public:
  Message();
  ~Message();

  const std::vector<uint8_t>& data() const { return data_; }
  const std::vector<Handle>& handles() const { return handles_; }

 private:
  friend class MessageBuilder;

  std::vector<uint8_t> data_;
  std::vector<Handle> handles_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_LIBS_MESSAGE_MESSAGE_H_
