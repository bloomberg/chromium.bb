// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_INTERFACE_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_INTERFACE_SERIALIZATION_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/interface_data_view.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace internal {

template <typename Base, typename T>
struct Serializer<AssociatedInterfacePtrInfoDataView<Base>,
                  AssociatedInterfacePtrInfo<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(AssociatedInterfacePtrInfo<T>& input,
                        AssociatedInterface_Data* output,
                        SerializationContext* context) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    context->AddAssociatedInterfaceInfo(input.PassHandle(), input.version(),
                                        output);
  }

  static bool Deserialize(AssociatedInterface_Data* input,
                          AssociatedInterfacePtrInfo<T>* output,
                          SerializationContext* context) {
    auto handle = context->TakeAssociatedEndpointHandle(input->handle);
    if (!handle.is_valid()) {
      *output = AssociatedInterfacePtrInfo<T>();
    } else {
      output->set_handle(std::move(handle));
      output->set_version(input->version);
    }
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<AssociatedInterfaceRequestDataView<Base>,
                  AssociatedInterfaceRequest<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(AssociatedInterfaceRequest<T>& input,
                        AssociatedEndpointHandle_Data* output,
                        SerializationContext* context) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    context->AddAssociatedEndpoint(input.PassHandle(), output);
  }

  static bool Deserialize(AssociatedEndpointHandle_Data* input,
                          AssociatedInterfaceRequest<T>* output,
                          SerializationContext* context) {
    auto handle = context->TakeAssociatedEndpointHandle(*input);
    if (!handle.is_valid())
      *output = AssociatedInterfaceRequest<T>();
    else
      *output = AssociatedInterfaceRequest<T>(std::move(handle));
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfacePtrDataView<Base>, InterfacePtr<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(InterfacePtr<T>& input,
                        Interface_Data* output,
                        SerializationContext* context) {
    InterfacePtrInfo<T> info = input.PassInterface();
    context->AddInterfaceInfo(info.PassHandle(), info.version(), output);
  }

  static bool Deserialize(Interface_Data* input,
                          InterfacePtr<T>* output,
                          SerializationContext* context) {
    output->Bind(InterfacePtrInfo<T>(
        context->TakeHandleAs<mojo::MessagePipeHandle>(input->handle),
        input->version));
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfaceRequestDataView<Base>, InterfaceRequest<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static void Serialize(InterfaceRequest<T>& input,
                        Handle_Data* output,
                        SerializationContext* context) {
    context->AddHandle(ScopedHandle::From(input.PassMessagePipe()), output);
  }

  static bool Deserialize(Handle_Data* input,
                          InterfaceRequest<T>* output,
                          SerializationContext* context) {
    *output =
        InterfaceRequest<T>(context->TakeHandleAs<MessagePipeHandle>(*input));
    return true;
  }
};

template <typename T>
struct Serializer<ScopedHandleBase<T>, ScopedHandleBase<T>> {
  static void Serialize(ScopedHandleBase<T>& input,
                        Handle_Data* output,
                        SerializationContext* context) {
    context->AddHandle(ScopedHandle::From(std::move(input)), output);
  }

  static bool Deserialize(Handle_Data* input,
                          ScopedHandleBase<T>* output,
                          SerializationContext* context) {
    *output = context->TakeHandleAs<T>(*input);
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_INTERFACE_SERIALIZATION_H_
