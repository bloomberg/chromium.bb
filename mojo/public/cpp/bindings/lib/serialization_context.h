// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_

#include <stddef.h>

#include <memory>
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

// Context information for serialization/deserialization routines.
class MOJO_CPP_BINDINGS_EXPORT SerializationContext {
 public:
  SerializationContext();
  ~SerializationContext();

  // Enqueues a custom context for a field within this context during
  // pre-serialization. A corresponding call to ConsumeNextCustomContext must be
  // made during serialization for the same field in order to retreive the
  // context.
  void PushCustomContext(void* custom_context);

  // Consumes a field's custom context enqueued during pre-serialization by
  // PushCustomContext().
  void* ConsumeNextCustomContext();

  // Enqueues the null state of a field within this context during
  // pre-serialization. A corresponding call to IsNextFieldNull must be made
  // during serialization for the same field in order to retrieve this state
  // later.
  void PushNextNullState(bool is_null);

  // Consumes a field's null state enqueued during pre-serialization by
  // PushNextNullState().
  bool IsNextFieldNull();

  // Adds a handle to the handle and serialized handle data lists during
  // pre-serialization.
  void AddHandle(mojo::ScopedHandle handle);

  // Consumes the next available serialized handle data which was added by
  // AddHandle() during pre-serialization.
  void ConsumeNextSerializedHandle(Handle_Data* out_data);

  // Adds an interface info to the handle and serialized handle+version data
  // lists during pre-serialization.
  void AddInterfaceInfo(mojo::ScopedMessagePipeHandle handle, uint32_t version);

  // Consumes the next available serialized handle and version data which was
  // added by AddInterfaceInfo() during pre-serialization.
  void ConsumeNextSerializedInterfaceInfo(Interface_Data* out_data);

  // Adds an associated interface endpoint (for e.g. an
  // AssociatedInterfaceRequest) to this context during pre-serialization.
  void AddAssociatedEndpoint(ScopedInterfaceEndpointHandle handle);

  // Consumes the next available serialized handle data which was added by
  // AddAssociatedEndpoint() during pre-serialization.
  void ConsumeNextSerializedAssociatedEndpoint(
      AssociatedEndpointHandle_Data* out_data);

  // Adds an associated interface info to associated endpoint handle and version
  // data lists during pre-serialization.
  void AddAssociatedInterfaceInfo(ScopedInterfaceEndpointHandle handle,
                                  uint32_t version);

  // Consumes the next available serialized associated endpoint handle and
  // version data which was added by AddAssociatedInterfaceInfo() during
  // pre-serialization.
  void ConsumeNextSerializedAssociatedInterfaceInfo(
      AssociatedInterface_Data* out_data);

  // Prepares a new serialized message based on the state accumulated by this
  // SerializationContext so far.
  void PrepareMessage(uint32_t message_name,
                      uint32_t flags,
                      size_t payload_size,
                      Message* message);

  const std::vector<mojo::ScopedHandle>* handles() { return &handles_; }
  std::vector<mojo::ScopedHandle>* mutable_handles() { return &handles_; }

  const std::vector<ScopedInterfaceEndpointHandle>*
  associated_endpoint_handles() const {
    return &associated_endpoint_handles_;
  }
  std::vector<ScopedInterfaceEndpointHandle>*
  mutable_associated_endpoint_handles() {
    return &associated_endpoint_handles_;
  }

  // Takes handles from a received Message object and assumes ownership of them.
  // Individual handles can be extracted using Take* methods below.
  void TakeHandlesFromMessage(Message* message);

  // Takes a handle from the list of serialized handle data.
  mojo::ScopedHandle TakeHandle(const Handle_Data& encoded_handle);

  // Takes a handle from the list of serialized handle data and returns it in
  // |*out_handle| as a specific scoped handle type.
  template <typename T>
  ScopedHandleBase<T> TakeHandleAs(const Handle_Data& encoded_handle) {
    return ScopedHandleBase<T>::From(TakeHandle(encoded_handle));
  }

  mojo::ScopedInterfaceEndpointHandle TakeAssociatedEndpointHandle(
      const AssociatedEndpointHandle_Data& encoded_handle);

 private:
  // A container for tracking the null-ness of every nullable field as a
  // message's arguments are walked during serialization.
  base::StackVector<bool, 32> null_states_;
  size_t null_state_index_ = 0;

  // Opaque context pointers returned by StringTraits::SetUpContext().
  std::vector<void*> custom_contexts_;
  size_t custom_context_index_ = 0;

  // Handles owned by this object. Used during serialization to hold onto
  // handles accumulated during pre-serialization, and used during
  // deserialization to hold onto handles extracted from a message.
  std::vector<mojo::ScopedHandle> handles_;

  // Stashes ScopedInterfaceEndpointHandles encoded in a message by index.
  std::vector<ScopedInterfaceEndpointHandle> associated_endpoint_handles_;

  // Accumulated version numbers for interface info. These may be accumulated
  // and consumed for either regular interface info or associated interface
  // info.
  std::vector<uint32_t> serialized_interface_versions_;
  size_t next_serialized_version_index_ = 0;

  // Serialized handle data. This is accumulated during pre-serialization by
  // AddHandle and AddInterfaceInfo calls, and later consumed during
  // serialization by ConsumeNextSerializedHandle/InterfaceInfo.
  std::vector<Handle_Data> serialized_handles_;
  size_t next_serialized_handle_index_ = 0;

  // Serialized associated handle data. This is accumulated during
  // pre-serialization by AddAssociatedEndpoint and AddAssociatedInterfaceInfo
  // calls, and later consumed during serialization by
  // ConsumeNextSerializedAssociatedEndpoint/InterfaceInfo.
  std::vector<AssociatedEndpointHandle_Data>
      serialized_associated_endpoint_handles_;
  size_t next_serialized_associated_endpoint_handle_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SerializationContext);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_
