// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_

#include <string.h>

#include "mojo/public/bindings/lib/bindings_serialization.h"
#include "mojo/public/bindings/sample/generated/sample_service.h"

namespace sample {
namespace internal {

const uint32_t kService_Frobinate_Name = 1;

#pragma pack(push, 1)

class Service_Frobinate_Params {
 public:
  static Service_Frobinate_Params* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Service_Frobinate_Params)))
        Service_Frobinate_Params();
  }

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

  Service_Frobinate_Params() {
    _header_.num_bytes = sizeof(*this);
    _header_.num_fields = 3;
  }
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
class ObjectTraits<sample::Bar> {
 public:
  static size_t ComputeSizeOf(const sample::Bar* bar) {
    return sizeof(*bar);
  }

  static sample::Bar* Clone(const sample::Bar* bar, Buffer* buf) {
    sample::Bar* clone = sample::Bar::New(buf);
    memcpy(clone, bar, sizeof(*bar));
    return clone;
  }

  static void EncodePointersAndHandles(sample::Bar* bar,
                                       std::vector<mojo::Handle>* handles) {
  }

  static bool DecodePointersAndHandles(sample::Bar* bar,
                                       const mojo::Message& message) {
    return true;
  }
};

template <>
class ObjectTraits<sample::Foo> {
 public:
  static size_t ComputeSizeOf(const sample::Foo* foo) {
    return sizeof(*foo) +
        mojo::internal::ComputeSizeOf(foo->bar()) +
        mojo::internal::ComputeSizeOf(foo->data()) +
        mojo::internal::ComputeSizeOf(foo->extra_bars()) +
        mojo::internal::ComputeSizeOf(foo->name()) +
        mojo::internal::ComputeSizeOf(foo->files());
  }

  static sample::Foo* Clone(const sample::Foo* foo, Buffer* buf) {
    sample::Foo* clone = sample::Foo::New(buf);
    memcpy(clone, foo, sizeof(*foo));

    clone->set_bar(mojo::internal::Clone(foo->bar(), buf));
    clone->set_data(mojo::internal::Clone(foo->data(), buf));
    clone->set_extra_bars(mojo::internal::Clone(foo->extra_bars(), buf));
    clone->set_name(mojo::internal::Clone(foo->name(), buf));
    clone->set_files(mojo::internal::Clone(foo->files(), buf));

    return clone;
  }

  static void EncodePointersAndHandles(sample::Foo* foo,
                                       std::vector<mojo::Handle>* handles) {
    Encode(&foo->bar_, handles);
    Encode(&foo->data_, handles);
    Encode(&foo->extra_bars_, handles);
    Encode(&foo->name_, handles);
    Encode(&foo->files_, handles);
  }

  static bool DecodePointersAndHandles(sample::Foo* foo,
                                       const mojo::Message& message) {
    if (!Decode(&foo->bar_, message))
      return false;
    if (!Decode(&foo->data_, message))
      return false;
    if (foo->_header_.num_fields >= 8) {
      if (!Decode(&foo->extra_bars_, message))
        return false;
    }
    if (foo->_header_.num_fields >= 9) {
      if (!Decode(&foo->name_, message))
        return false;
    }
    if (foo->_header_.num_fields >= 10) {
      if (!Decode(&foo->files_, message))
        return false;
    }

    // TODO: validate
    return true;
  }
};

template <>
class ObjectTraits<sample::internal::Service_Frobinate_Params> {
 public:
  static void EncodePointersAndHandles(
      sample::internal::Service_Frobinate_Params* params,
      std::vector<mojo::Handle>* handles) {
    Encode(&params->foo_, handles);
    EncodeHandle(&params->port_, handles);
  }

  static bool DecodePointersAndHandles(
      sample::internal::Service_Frobinate_Params* params,
      const mojo::Message& message) {
    if (!Decode(&params->foo_, message))
      return false;
    if (params->_header_.num_fields >= 3) {
      if (!DecodeHandle(&params->port_, message.handles))
        return false;
    }

    // TODO: validate
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_
