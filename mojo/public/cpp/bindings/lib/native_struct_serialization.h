// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"
#include "base/pickle.h"
#include "ipc/ipc_param_traits.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/native_struct_data.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"

namespace mojo {
namespace internal {

template <typename MaybeConstUserType,
          bool unmapped = std::is_same<
              NativeStructPtr,
              typename std::remove_const<MaybeConstUserType>::type>::value>
struct NativeStructSerializerImpl;

template <typename MaybeConstUserType>
struct NativeStructSerializerImpl<MaybeConstUserType, true> {
  static size_t PrepareToSerialize(const NativeStructPtr& input,
                                   SerializationContext* context) {
    if (!input)
      return 0;
    return internal::PrepareToSerialize<Array<uint8_t>>(input->data, context);
  }

  static void Serialize(NativeStructPtr& input,
                        Buffer* buffer,
                        NativeStruct_Data** output,
                        SerializationContext* context) {
    if (!input) {
      *output = nullptr;
      return;
    }

    Array_Data<uint8_t>* data = nullptr;
    const ArrayValidateParams params(0, false, nullptr);
    internal::Serialize<Array<uint8_t>>(input->data, buffer, &data, &params,
                                        context);
    *output = reinterpret_cast<NativeStruct_Data*>(data);
  }

  static bool Deserialize(NativeStruct_Data* input,
                          NativeStructPtr* output,
                          SerializationContext* context) {
    Array_Data<uint8_t>* data = reinterpret_cast<Array_Data<uint8_t>*>(input);

    NativeStructPtr result(NativeStruct::New());
    if (!internal::Deserialize<Array<uint8_t>>(data, &result->data, context)) {
      output = nullptr;
      return false;
    }
    if (!result->data)
      *output = nullptr;
    else
      result.Swap(output);
    return true;
  }
};

template <typename MaybeConstUserType>
struct NativeStructSerializerImpl<MaybeConstUserType, false> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = IPC::ParamTraits<UserType>;

  static size_t PrepareToSerialize(MaybeConstUserType& value,
                                   SerializationContext* context) {
    base::PickleSizer sizer;
    Traits::GetSize(&sizer, value);
    return Align(sizer.payload_size() + sizeof(ArrayHeader));
  }

  static void Serialize(MaybeConstUserType& value,
                        Buffer* buffer,
                        NativeStruct_Data** out,
                        SerializationContext* context) {
    base::Pickle pickle;
    Traits::Write(&pickle, value);

    size_t total_size = pickle.payload_size() + sizeof(ArrayHeader);
    DCHECK_LT(total_size, std::numeric_limits<uint32_t>::max());

    // Allocate a uint8 array, initialize its header, and copy the Pickle in.
    ArrayHeader* header =
        reinterpret_cast<ArrayHeader*>(buffer->Allocate(total_size));
    header->num_bytes = static_cast<uint32_t>(total_size);
    header->num_elements = static_cast<uint32_t>(pickle.payload_size());
    memcpy(reinterpret_cast<char*>(header) + sizeof(ArrayHeader),
           pickle.payload(), pickle.payload_size());

    *out = reinterpret_cast<NativeStruct_Data*>(header);
  }

  static bool Deserialize(NativeStruct_Data* data,
                          UserType* out,
                          SerializationContext* context) {
    if (!data)
      return true;

    // Construct a temporary base::Pickle view over the array data. Note that
    // the Array_Data is laid out like this:
    //
    //   [num_bytes (4 bytes)] [num_elements (4 bytes)] [elements...]
    //
    // and base::Pickle expects to view data like this:
    //
    //   [payload_size (4 bytes)] [header bytes ...] [payload...]
    //
    // Because ArrayHeader's num_bytes includes the length of the header and
    // Pickle's payload_size does not, we need to adjust the stored value
    // momentarily so Pickle can view the data.
    ArrayHeader* header = reinterpret_cast<ArrayHeader*>(data);
    DCHECK_GE(header->num_bytes, sizeof(ArrayHeader));
    header->num_bytes -= sizeof(ArrayHeader);

    {
      // Construct a view over the full Array_Data, including our hacked up
      // header. Pickle will infer from this that the header is 8 bytes long,
      // and the payload will contain all of the pickled bytes.
      base::Pickle pickle_view(reinterpret_cast<const char*>(header),
                               header->num_bytes + sizeof(ArrayHeader));
      base::PickleIterator iter(pickle_view);
      if (!Traits::Read(&pickle_view, &iter, out))
        return false;
    }

    // Return the header to its original state.
    header->num_bytes += sizeof(ArrayHeader);

    return true;
  }
};

template <typename MaybeConstUserType>
struct Serializer<NativeStructPtr, MaybeConstUserType>
    : public NativeStructSerializerImpl<MaybeConstUserType> {};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_
