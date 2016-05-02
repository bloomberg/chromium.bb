// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/wtf_string_serialization.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/wtf_serialization.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/string_traits.h"
#include "third_party/WebKit/Source/wtf/text/WTFString.h"

namespace {

using StringSerializer =
    mojo::internal::Serializer<mojo::String, const WTF::String>;

}  // namespace

namespace WTF {

size_t GetSerializedSize_(const WTF::String& input,
                          mojo::internal::SerializationContext* context) {
  return StringSerializer::PrepareToSerialize(input, context);
}

void Serialize_(const WTF::String& input,
                mojo::internal::Buffer* buf,
                mojo::internal::String_Data** output,
                mojo::internal::SerializationContext* context) {
  StringSerializer::Serialize(input, buf, output, context);
}

bool Deserialize_(mojo::internal::String_Data* input,
                  WTF::String* output,
                  mojo::internal::SerializationContext* context) {
  return StringSerializer::Deserialize(input, output, context);
}

}  // namespace WTF
