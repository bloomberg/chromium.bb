// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_

#include <stddef.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace internal {

// A container for handles during serialization/deserialization.
class MOJO_CPP_BINDINGS_EXPORT SerializedHandleVector {
 public:
  SerializedHandleVector();
  ~SerializedHandleVector();

  size_t size() const { return handles_.size(); }

  // Adds a handle to the handle list and returns its index for encoding.
  Handle_Data AddHandle(mojo::Handle handle);

  // Takes a handle from the list of serialized handle data.
  mojo::Handle TakeHandle(const Handle_Data& encoded_handle);

  // Takes a handle from the list of serialized handle data and returns it in
  // |*out_handle| as a specific scoped handle type.
  template <typename T>
  ScopedHandleBase<T> TakeHandleAs(const Handle_Data& encoded_handle) {
    return MakeScopedHandle(T(TakeHandle(encoded_handle).value()));
  }

  // Swaps all owned handles out with another Handle vector.
  void Swap(std::vector<mojo::Handle>* other);

 private:
  // Handles are owned by this object.
  std::vector<mojo::Handle> handles_;

  DISALLOW_COPY_AND_ASSIGN(SerializedHandleVector);
};

// Context information for serialization/deserialization routines.
struct MOJO_CPP_BINDINGS_EXPORT SerializationContext {
  SerializationContext();

  ~SerializationContext();

  // Opaque context pointers returned by StringTraits::SetUpContext().
  std::unique_ptr<std::queue<void*>> custom_contexts;

  // Stashes handles encoded in a message by index.
  SerializedHandleVector handles;

  // The number of ScopedInterfaceEndpointHandles that need to be serialized.
  // It is calculated by PrepareToSerialize().
  uint32_t associated_endpoint_count = 0;

  // Stashes ScopedInterfaceEndpointHandles encoded in a message by index.
  std::vector<ScopedInterfaceEndpointHandle> associated_endpoint_handles;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_
