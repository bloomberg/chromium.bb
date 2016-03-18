// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/wtf_string_serialization.h"

#include <string.h>

#include <queue>

#include "base/logging.h"
#include "third_party/WebKit/Source/wtf/text/StringUTF8Adaptor.h"
#include "third_party/WebKit/Source/wtf/text/WTFString.h"

namespace WTF {
namespace {

struct UTF8AdaptorInfo {
  explicit UTF8AdaptorInfo(const WTF::String& input) : utf8_adaptor(input) {
#if DCHECK_IS_ON()
    original_size_in_bytes = static_cast<size_t>(input.sizeInBytes());
#endif
  }

  ~UTF8AdaptorInfo() {}

  WTF::StringUTF8Adaptor utf8_adaptor;

#if DCHECK_IS_ON()
  // For sanity check only.
  size_t original_size_in_bytes;
#endif
};

class WTFStringContextImpl : public mojo::internal::WTFStringContext {
 public:
  WTFStringContextImpl() {}
  ~WTFStringContextImpl() override {}

  std::queue<UTF8AdaptorInfo>& utf8_adaptors() { return utf8_adaptors_; }

 private:
  // When serializing an object, we call GetSerializedSize_() recursively on
  // all its elements/members to compute the total size, and then call
  // Serialize*_() recursively in the same order to do the actual
  // serialization. If some WTF::Strings need to be converted to UTF8, we don't
  // want to do that twice. Therefore, we store a WTF::StringUTF8Adaptor for
  // each in the first pass, and reuse it in the second pass.
  std::queue<UTF8AdaptorInfo> utf8_adaptors_;

  DISALLOW_COPY_AND_ASSIGN(WTFStringContextImpl);
};

}  // namespace

size_t GetSerializedSize_(const WTF::String& input,
                          mojo::internal::SerializationContext* context) {
  if (input.isNull())
    return 0;

  if (!context->wtf_string_context)
    context->wtf_string_context.reset(new WTFStringContextImpl);

  auto& utf8_adaptors =
      static_cast<WTFStringContextImpl*>(context->wtf_string_context.get())
          ->utf8_adaptors();

  utf8_adaptors.emplace(input);

  return mojo::internal::Align(sizeof(mojo::internal::String_Data) +
                               utf8_adaptors.back().utf8_adaptor.length());
}

void Serialize_(const WTF::String& input,
                mojo::internal::Buffer* buf,
                mojo::internal::String_Data** output,
                mojo::internal::SerializationContext* context) {
  if (input.isNull()) {
    *output = nullptr;
    return;
  }

  auto& utf8_adaptors =
      static_cast<WTFStringContextImpl*>(context->wtf_string_context.get())
          ->utf8_adaptors();

  DCHECK(!utf8_adaptors.empty());
#if DCHECK_IS_ON()
  DCHECK_EQ(utf8_adaptors.front().original_size_in_bytes,
            static_cast<size_t>(input.sizeInBytes()));
#endif

  const WTF::StringUTF8Adaptor& adaptor = utf8_adaptors.front().utf8_adaptor;

  mojo::internal::String_Data* result =
      mojo::internal::String_Data::New(adaptor.length(), buf);
  if (result)
    memcpy(result->storage(), adaptor.data(), adaptor.length());

  utf8_adaptors.pop();

  *output = result;
}

bool Deserialize_(mojo::internal::String_Data* input,
                  WTF::String* output,
                  mojo::internal::SerializationContext* context) {
  if (input) {
    WTF::String result = WTF::String::fromUTF8(input->storage(), input->size());
    output->swap(result);
  } else if (!output->isNull()) {
    WTF::String result;
    output->swap(result);
  }
  return true;
}

}  // namespace WTF
