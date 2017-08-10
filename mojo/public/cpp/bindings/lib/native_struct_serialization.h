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
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/native_struct_data.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/native_struct.h"
#include "mojo/public/cpp/bindings/native_struct_data_view.h"

namespace mojo {
namespace internal {

template <typename MaybeConstUserType>
struct NativeStructSerializerImpl {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = IPC::ParamTraits<UserType>;

  static void Serialize(MaybeConstUserType& value,
                        Buffer* buffer,
                        NativeStruct_Data::BufferWriter* writer,
                        SerializationContext* context) {
    base::Pickle pickle;
    Traits::Write(&pickle, value);

#if DCHECK_IS_ON()
    base::PickleSizer sizer;
    Traits::GetSize(&sizer, value);
    DCHECK_EQ(sizer.payload_size(), pickle.payload_size());
#endif

    // Allocate a uint8 array, initialize its header, and copy the Pickle in.
    writer->Allocate(pickle.payload_size(), buffer);
    memcpy(writer->array_writer()->storage(), pickle.payload(),
           pickle.payload_size());
  }

  static bool Deserialize(NativeStruct_Data* data,
                          UserType* out,
                          SerializationContext* context) {
    if (!data)
      return false;

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

struct MOJO_CPP_BINDINGS_EXPORT UnmappedNativeStructSerializerImpl {
  static void Serialize(const NativeStructPtr& input,
                        Buffer* buffer,
                        NativeStruct_Data::BufferWriter* writer,
                        SerializationContext* context);
  static bool Deserialize(NativeStruct_Data* input,
                          NativeStructPtr* output,
                          SerializationContext* context);
};

template <>
struct NativeStructSerializerImpl<NativeStructPtr>
    : public UnmappedNativeStructSerializerImpl {};

template <>
struct NativeStructSerializerImpl<const NativeStructPtr>
    : public UnmappedNativeStructSerializerImpl {};

template <typename MaybeConstUserType>
struct Serializer<NativeStructDataView, MaybeConstUserType>
    : public NativeStructSerializerImpl<MaybeConstUserType> {};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_
