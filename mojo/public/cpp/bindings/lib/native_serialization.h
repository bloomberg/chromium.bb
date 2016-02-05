// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_SERIALIZATION_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"
#include "base/pickle.h"
#include "ipc/ipc_param_traits.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"
#include "mojo/public/cpp/bindings/lib/pickle_buffer.h"

namespace mojo {
namespace internal {

// Generated bindings for native-only types will specialize this to |true|.
// It can be used as a signal (by e.g. the Array serializer) for when to use
// SerializeNative_ with a type.
template <typename E>
struct ShouldUseNativeSerializer { static const bool value = false; };

template <typename T>
size_t GetSerializedSizeNative_(const T& value) {
  base::PickleSizer sizer;
  IPC::ParamTraits<T>::GetSize(&sizer, value);
  return sizer.payload_size() + sizeof(ArrayHeader);
}

template <typename T>
void SerializeNative_(const T& value,
                      Buffer* buffer,
                      Array_Data<uint8_t>** out) {
  PickleBuffer* pickler = buffer->AsPickleBuffer();
  DCHECK(pickler) << "Native types can only be used with PickleBuffers.";

  ArrayHeader* header =
      reinterpret_cast<ArrayHeader*>(buffer->Allocate(sizeof(ArrayHeader)));

  // Remember where the Pickle started before writing.
  base::Pickle* pickle = pickler->pickle();
  const char* data_start = pickle->end_of_payload();

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  const char* payload_base = pickle->payload();
  size_t size_before_write = pickle->payload_size();
#endif

  IPC::ParamTraits<T>::Write(pickle, value);

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  // Ensure the pickle buffer hasn't moved.
  DCHECK_EQ(payload_base, pickle->payload());
  // Explicitly validate that the value returned by GetSize() always equals the
  // number of bytes actually written by Write().
  DCHECK_GE(pickle->payload_size(), size_before_write);
  size_t bytes_written = pickle->payload_size() - size_before_write;
  DCHECK_EQ(bytes_written + sizeof(ArrayHeader),
            GetSerializedSizeNative_(value));
#endif

  // Fix up the ArrayHeader so that num_elements contains the length of the
  // pickled data.
  size_t pickled_size = pickle->end_of_payload() - data_start;
  size_t total_size = pickled_size + sizeof(ArrayHeader);
  DCHECK_LT(total_size, std::numeric_limits<uint32_t>::max());
  header->num_bytes = static_cast<uint32_t>(total_size);
  header->num_elements = static_cast<uint32_t>(pickled_size);

  *out = reinterpret_cast<Array_Data<uint8_t>*>(header);
}

template <typename T>
bool DeserializeNative_(Array_Data<uint8_t>* data,
                        T* out,
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
    if (!IPC::ParamTraits<T>::Read(&pickle_view, &iter, out))
      return false;
  }

  // Return the header to its original state.
  header->num_bytes += sizeof(ArrayHeader);

  return true;
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_SERIALIZATION_H_
