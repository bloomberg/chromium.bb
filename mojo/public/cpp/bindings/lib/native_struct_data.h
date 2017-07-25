// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_DATA_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_DATA_H_

#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace internal {

class ValidationContext;

class MOJO_CPP_BINDINGS_EXPORT NativeStruct_Data {
 public:
  class BufferWriter {
   public:
    BufferWriter() = default;

    void Allocate(size_t num_bytes, Buffer* buffer) {
      array_writer_.Allocate(num_bytes, buffer);
    }

    Array_Data<uint8_t>::BufferWriter& array_writer() { return array_writer_; }

    bool is_null() const { return array_writer_.is_null(); }
    NativeStruct_Data* data() {
      return reinterpret_cast<NativeStruct_Data*>(array_writer_.data());
    }
    NativeStruct_Data* operator->() { return data(); }

   private:
    Array_Data<uint8_t>::BufferWriter array_writer_;

    DISALLOW_COPY_AND_ASSIGN(BufferWriter);
  };

  static bool Validate(const void* data, ValidationContext* validation_context);

  // Unlike normal structs, the memory layout is exactly the same as an array
  // of uint8_t.
  Array_Data<uint8_t> data;

 private:
  NativeStruct_Data() = delete;
  ~NativeStruct_Data() = delete;
};

static_assert(sizeof(Array_Data<uint8_t>) == sizeof(NativeStruct_Data),
              "Mismatched NativeStruct_Data and Array_Data<uint8_t> size");

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_DATA_H_
