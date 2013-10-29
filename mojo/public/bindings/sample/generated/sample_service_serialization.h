// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_

#include <string.h>

#include "mojo/public/bindings/lib/bindings_serialization.h"
#include "mojo/public/bindings/sample/generated/sample_foo_serialization.h"
#include "mojo/public/bindings/sample/generated/sample_service.h"

namespace sample {
namespace internal {

const uint32_t kService_Frobinate_Name = 1;

#pragma pack(push, 1)

class Service_Frobinate_Params {
 public:
  static Service_Frobinate_Params* New(mojo::Buffer* buf);

  void set_foo(Foo* foo) { foo_.ptr = foo; }
  void set_baz(bool baz) { baz_ = baz; }
  void set_port(mojo::Handle port) { port_ = port; }

  const Foo* foo() const { return foo_.ptr; }
  bool baz() const { return baz_; }
  mojo::Handle port() const {
    // NOTE: port is an optional field!
    return _header_.num_fields >= 3 ? port_ : mojo::kInvalidHandle;
  }

 private:
  friend class mojo::internal::ObjectTraits<Service_Frobinate_Params>;

  Service_Frobinate_Params();
  ~Service_Frobinate_Params();  // NOT IMPLEMENTED

  mojo::internal::StructHeader _header_;
  mojo::internal::StructPointer<Foo> foo_;
  uint8_t baz_ : 1;
  uint8_t _pad0_[3];
  mojo::Handle port_;
};
MOJO_COMPILE_ASSERT(sizeof(Service_Frobinate_Params) == 24,
                    bad_sizeof_Service_Frobinate_Params);

#pragma pack(pop)

}  // namespace internal
}  // namespace sample

namespace mojo {
namespace internal {

template <>
class ObjectTraits<sample::internal::Service_Frobinate_Params> {
 public:
  static void EncodePointersAndHandles(
      sample::internal::Service_Frobinate_Params* params,
      std::vector<mojo::Handle>* handles);
  static bool DecodePointersAndHandles(
      sample::internal::Service_Frobinate_Params* params,
      const mojo::Message& message);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_
