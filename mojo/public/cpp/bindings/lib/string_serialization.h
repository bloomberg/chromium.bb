// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_

#include <stddef.h>
#include <string.h>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/string_data_view.h"
#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {
namespace internal {

template <typename MaybeConstUserType>
struct Serializer<StringDataView, MaybeConstUserType> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = StringTraits<UserType>;

  static size_t PrepareToSerialize(MaybeConstUserType& input,
                                   SerializationContext* context) {
    const bool is_null = CallIsNullIfExists<Traits>(input);
    context->PushNextNullState(is_null);
    if (is_null)
      return 0;

    void* custom_context = CustomContextHelper<Traits>::SetUp(input, context);
    return Align(sizeof(String_Data) +
                 CallWithContext(Traits::GetSize, input, custom_context));
  }

  static void Serialize(MaybeConstUserType& input,
                        Buffer* buffer,
                        String_Data** output,
                        SerializationContext* context) {
    if (context->IsNextFieldNull()) {
      *output = nullptr;
      return;
    }

    void* custom_context = CustomContextHelper<Traits>::GetNext(context);

    String_Data* result = String_Data::New(
        CallWithContext(Traits::GetSize, input, custom_context), buffer);
    if (result) {
      memcpy(result->storage(),
             CallWithContext(Traits::GetData, input, custom_context),
             CallWithContext(Traits::GetSize, input, custom_context));
    }
    *output = result;

    CustomContextHelper<Traits>::TearDown(input, custom_context);
  }

  static bool Deserialize(String_Data* input,
                          UserType* output,
                          SerializationContext* context) {
    if (!input)
      return CallSetToNullIfExists<Traits>(output);
    return Traits::Read(StringDataView(input, context), output);
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_
