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

  static size_t PrepareToSerialize(const AssociatedInterfacePtrInfo<T>& input,
                                   SerializationContext* context) {
    if (input.handle().is_valid())
      context->associated_endpoint_count++;
    return 0;
  }

  static void Serialize(AssociatedInterfacePtrInfo<T>& input,
                        AssociatedInterface_Data* output,
                        SerializationContext* context) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    if (input.handle().is_valid()) {
      // Set to the index of the element pushed to the back of the vector.
      output->handle.value =
          static_cast<uint32_t>(context->associated_endpoint_handles.size());
      context->associated_endpoint_handles.push_back(input.PassHandle());
    } else {
      output->handle.value = kEncodedInvalidHandleValue;
    }
    output->version = input.version();
  }

  static bool Deserialize(AssociatedInterface_Data* input,
                          AssociatedInterfacePtrInfo<T>* output,
                          SerializationContext* context) {
    if (input->handle.is_valid()) {
      DCHECK_LT(input->handle.value,
                context->associated_endpoint_handles.size());
      output->set_handle(
          std::move(context->associated_endpoint_handles[input->handle.value]));
    } else {
      output->set_handle(ScopedInterfaceEndpointHandle());
    }
    output->set_version(input->version);
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<AssociatedInterfaceRequestDataView<Base>,
                  AssociatedInterfaceRequest<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static size_t PrepareToSerialize(const AssociatedInterfaceRequest<T>& input,
                                   SerializationContext* context) {
    if (input.handle().is_valid())
      context->associated_endpoint_count++;
    return 0;
  }

  static void Serialize(AssociatedInterfaceRequest<T>& input,
                        AssociatedEndpointHandle_Data* output,
                        SerializationContext* context) {
    DCHECK(!input.handle().is_valid() || input.handle().pending_association());
    if (input.handle().is_valid()) {
      // Set to the index of the element pushed to the back of the vector.
      output->value =
          static_cast<uint32_t>(context->associated_endpoint_handles.size());
      context->associated_endpoint_handles.push_back(input.PassHandle());
    } else {
      output->value = kEncodedInvalidHandleValue;
    }
  }

  static bool Deserialize(AssociatedEndpointHandle_Data* input,
                          AssociatedInterfaceRequest<T>* output,
                          SerializationContext* context) {
    if (input->is_valid()) {
      DCHECK_LT(input->value, context->associated_endpoint_handles.size());
      output->Bind(
          std::move(context->associated_endpoint_handles[input->value]));
    } else {
      output->Bind(ScopedInterfaceEndpointHandle());
    }
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfacePtrDataView<Base>, InterfacePtr<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static size_t PrepareToSerialize(const InterfacePtr<T>& input,
                                   SerializationContext* context) {
    return 0;
  }

  static void Serialize(InterfacePtr<T>& input,
                        Interface_Data* output,
                        SerializationContext* context) {
    InterfacePtrInfo<T> info = input.PassInterface();
    output->handle = context->handles.AddHandle(info.PassHandle().release());
    output->version = info.version();
  }

  static bool Deserialize(Interface_Data* input,
                          InterfacePtr<T>* output,
                          SerializationContext* context) {
    output->Bind(InterfacePtrInfo<T>(
        context->handles.TakeHandleAs<mojo::MessagePipeHandle>(input->handle),
        input->version));
    return true;
  }
};

template <typename Base, typename T>
struct Serializer<InterfaceRequestDataView<Base>, InterfaceRequest<T>> {
  static_assert(std::is_base_of<Base, T>::value, "Interface type mismatch.");

  static size_t PrepareToSerialize(const InterfaceRequest<T>& input,
                                   SerializationContext* context) {
    return 0;
  }

  static void Serialize(InterfaceRequest<T>& input,
                        Handle_Data* output,
                        SerializationContext* context) {
    *output = context->handles.AddHandle(input.PassMessagePipe().release());
  }

  static bool Deserialize(Handle_Data* input,
                          InterfaceRequest<T>* output,
                          SerializationContext* context) {
    output->Bind(context->handles.TakeHandleAs<MessagePipeHandle>(*input));
    return true;
  }
};

template <typename T>
struct Serializer<ScopedHandleBase<T>, ScopedHandleBase<T>> {
  static size_t PrepareToSerialize(const ScopedHandleBase<T>& input,
                                   SerializationContext* context) {
    return 0;
  }

  static void Serialize(ScopedHandleBase<T>& input,
                        Handle_Data* output,
                        SerializationContext* context) {
    *output = context->handles.AddHandle(input.release());
  }

  static bool Deserialize(Handle_Data* input,
                          ScopedHandleBase<T>* output,
                          SerializationContext* context) {
    *output = context->handles.TakeHandleAs<T>(*input);
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_INTERFACE_SERIALIZATION_H_
