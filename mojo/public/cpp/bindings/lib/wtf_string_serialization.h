// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_STRING_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_STRING_SERIALIZATION_H_

#include <stddef.h>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"

namespace WTF {
class String;

// TODO(yzshen): These methods are simply wrappers of the Serialzer interface.
// Remove them.
size_t GetSerializedSize_(const WTF::String& input,
                          mojo::internal::SerializationContext* context);
void Serialize_(const WTF::String& input,
                mojo::internal::Buffer* buffer,
                mojo::internal::String_Data** output,
                mojo::internal::SerializationContext* context);
bool Deserialize_(mojo::internal::String_Data* input,
                  WTF::String* output,
                  mojo::internal::SerializationContext* context);

}  // namespace WTF

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_STRING_SERIALIZATION_H_
