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

  // Adds a handle to the handle list and outputs its serialized form in
  // |*out_data|.
  void AddHandle(mojo::ScopedHandle handle, Handle_Data* out_data);

  // Adds an interface info to the handle list and outputs its serialized form
  // in |*out_data|.
  void AddInterfaceInfo(mojo::ScopedMessagePipeHandle handle,
                        uint32_t version,
                        Interface_Data* out_data);

  // Adds an associated interface endpoint (for e.g. an
  // AssociatedInterfaceRequest) to this context and outputs its serialized form
  // in |*out_data|.
  void AddAssociatedEndpoint(ScopedInterfaceEndpointHandle handle,
                             AssociatedEndpointHandle_Data* out_data);

  // Adds an associated interface info to associated endpoint handle and version
  // data lists and outputs its serialized form in |*out_data|.
  void AddAssociatedInterfaceInfo(ScopedInterfaceEndpointHandle handle,
                                  uint32_t version,
                                  AssociatedInterface_Data* out_data);

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
  // Handles owned by this object. Used during serialization to hold onto
  // handles accumulated during pre-serialization, and used during
  // deserialization to hold onto handles extracted from a message.
  std::vector<mojo::ScopedHandle> handles_;

  // Stashes ScopedInterfaceEndpointHandles encoded in a message by index.
  std::vector<ScopedInterfaceEndpointHandle> associated_endpoint_handles_;

  DISALLOW_COPY_AND_ASSIGN(SerializationContext);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_CONTEXT_H_
