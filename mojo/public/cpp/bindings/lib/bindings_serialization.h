// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace internal {

struct SerializationContext;
class MultiplexRouter;

size_t Align(size_t size);
char* AlignPointer(char* ptr);

bool IsAligned(const void* ptr);

// Pointers are encoded as relative offsets. The offsets are relative to the
// address of where the offset value is stored, such that the pointer may be
// recovered with the expression:
//
//   ptr = reinterpret_cast<char*>(offset) + *offset
//
// A null pointer is encoded as an offset value of 0.
//
void EncodePointer(const void* ptr, uint64_t* offset);
// Note: This function doesn't validate the encoded pointer value.
const void* DecodePointerRaw(const uint64_t* offset);

// Note: This function doesn't validate the encoded pointer value.
template <typename T>
inline void DecodePointer(const uint64_t* offset, T** ptr) {
  *ptr = reinterpret_cast<T*>(const_cast<void*>(DecodePointerRaw(offset)));
}

// The following 2 functions are used to encode/decode all objects (structs and
// arrays) in a consistent manner.

template <typename T>
inline void Encode(T* obj) {
  if (obj->ptr)
    obj->ptr->EncodePointers();
  EncodePointer(obj->ptr, &obj->offset);
}

// Note: This function doesn't validate the encoded pointer and handle values.
template <typename T>
inline void Decode(T* obj) {
  DecodePointer(&obj->offset, &obj->ptr);
  if (obj->ptr)
    obj->ptr->DecodePointers();
}

template <typename T>
inline void AssociatedInterfacePtrInfoToData(
    AssociatedInterfacePtrInfo<T> input,
    AssociatedInterface_Data* output) {
  output->version = input.version();
  output->interface_id =
      AssociatedInterfacePtrInfoHelper::PassHandle(&input).release();
}

template <typename T>
inline void AssociatedInterfaceDataToPtrInfo(
    AssociatedInterface_Data* input,
    AssociatedInterfacePtrInfo<T>* output,
    MultiplexRouter* router) {
  AssociatedInterfacePtrInfoHelper::SetHandle(
      output,
      router->CreateLocalEndpointHandle(FetchAndReset(&input->interface_id)));
  output->set_version(input->version);
}

class WTFStringContext {
 public:
  virtual ~WTFStringContext() {}
};

// A container for handles during serialization/deserialization.
class SerializedHandleVector {
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
struct SerializationContext {
  SerializationContext();
  explicit SerializationContext(scoped_refptr<MultiplexRouter> in_router);

  ~SerializationContext();

  // Used to serialize/deserialize associated interface pointers and requests.
  scoped_refptr<MultiplexRouter> router;

  std::unique_ptr<WTFStringContext> wtf_string_context;

  // Stashes handles encoded in a message by index.
  SerializedHandleVector handles;
};

template <typename T>
inline void InterfacePointerToData(InterfacePtr<T> input,
                                   Interface_Data* output,
                                   SerializationContext* context) {
  InterfacePtrInfo<T> info = input.PassInterface();
  output->handle = context->handles.AddHandle(info.PassHandle().release());
  output->version = info.version();
}

template <typename T>
inline void InterfaceDataToPointer(Interface_Data* input,
                                   InterfacePtr<T>* output,
                                   SerializationContext* context) {
  output->Bind(InterfacePtrInfo<T>(
      context->handles.TakeHandleAs<mojo::MessagePipeHandle>(input->handle),
      input->version));
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_H_
