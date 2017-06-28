// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_

#include <stddef.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/containers/stack_container.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {

class Message;

namespace internal {

// A container for handles during serialization/deserialization.
class MOJO_CPP_BINDINGS_EXPORT SerializedHandleVector {
 public:
  SerializedHandleVector();
  ~SerializedHandleVector();

  size_t size() const { return handles_.size(); }
  std::vector<mojo::ScopedHandle>* mutable_handles() { return &handles_; }
  std::vector<Handle_Data>* serialized_handles() {
    return &serialized_handles_;
  }

  // Adds a handle to the handle and serialized handle data lists.
  void AddHandle(mojo::ScopedHandle handle);

  // Adds an interface info to the handle and serialized handle+version data
  // lists.
  void AddInterfaceInfo(mojo::ScopedMessagePipeHandle handle, uint32_t version);

  // Consumes the next available serialized handle data.
  void ConsumeNextSerializedHandle(Handle_Data* out_data);

  // Consumes the next available serialized handle and version data.
  void ConsumeNextSerializedInterfaceInfo(Interface_Data* out_data);

  // Takes a handle from the list of serialized handle data.
  mojo::ScopedHandle TakeHandle(const Handle_Data& encoded_handle);

  // Takes a handle from the list of serialized handle data and returns it in
  // |*out_handle| as a specific scoped handle type.
  template <typename T>
  ScopedHandleBase<T> TakeHandleAs(const Handle_Data& encoded_handle) {
    return ScopedHandleBase<T>::From(TakeHandle(encoded_handle));
  }

  // Swaps all owned handles out with another Handle vector.
  void Swap(std::vector<mojo::ScopedHandle>* other);

 private:
  // Handles are owned by this object.
  std::vector<mojo::ScopedHandle> handles_;

  // Serialized handle and (optional) version data. This is accumulated during
  // pre-serialization by AddHandle and AddInterfaceInfo calls, and later
  // consumed during serialization by ConsumeNextSerializedHandle/InterfaceInfo.
  size_t next_serialized_handle_index_ = 0;
  std::vector<Handle_Data> serialized_handles_;
  size_t next_serialized_version_index_ = 0;
  std::vector<uint32_t> serialized_interface_versions_;

  DISALLOW_COPY_AND_ASSIGN(SerializedHandleVector);
};

// Context information for serialization/deserialization routines.
struct MOJO_CPP_BINDINGS_EXPORT SerializationContext {
  SerializationContext();

  ~SerializationContext();

  bool IsNextFieldNull() {
    DCHECK_LT(null_state_index, null_states.container().size());
    return null_states.container()[null_state_index++];
  }

  // Transfers ownership of any accumulated associated endpoint handles into
  // |*message|.
  void AttachHandlesToMessage(Message* message);

  // Opaque context pointers returned by StringTraits::SetUpContext().
  std::unique_ptr<std::queue<void*>> custom_contexts;

  // A container for tracking the null-ness of every nullable field as a
  // message's arguments are walked during serialization.
  base::StackVector<bool, 32> null_states;
  size_t null_state_index = 0;

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
