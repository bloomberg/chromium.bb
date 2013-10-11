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

class Service_Frobinate_Params {
 public:
  static Service_Frobinate_Params* New(mojo::Buffer* buf) {
    return new (buf->Allocate(sizeof(Service_Frobinate_Params)))
        Service_Frobinate_Params();
  }

  void set_foo(Foo* foo) { d_.foo.ptr = foo; }
  void set_baz(bool baz) { d_.baz = baz; }
  void set_port(mojo::Handle port) { d_.port = port; }

  const Foo* foo() const { return d_.foo.ptr; }
  bool baz() const { return d_.baz; }
  mojo::Handle port() const {
    // NOTE: port is an optional field!
    return header_.num_fields >= 3 ? d_.port : mojo::kInvalidHandle;
  }

 private:
  friend class mojo::internal::ObjectTraits<Service_Frobinate_Params>;

  Service_Frobinate_Params() {
    header_.num_bytes = sizeof(*this);
    header_.num_fields = 3;
  }
  ~Service_Frobinate_Params();  // NOT IMPLEMENTED

  mojo::internal::StructHeader header_;
  struct Data {
    mojo::internal::StructPointer<Foo> foo;
    uint32_t baz : 1;
    mojo::Handle port;
  } d_;
};

}  // namespace internal
}  // namespace sample

namespace mojo {
namespace internal {

template <>
class ObjectTraits<sample::Bar> {
 public:
  static size_t ComputeAlignedSizeOf(const sample::Bar* bar) {
    return Align(sizeof(*bar));
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
  static size_t ComputeAlignedSizeOf(const sample::Foo* foo) {
    return Align(sizeof(*foo)) +
        mojo::internal::ComputeAlignedSizeOf(foo->bar()) +
        mojo::internal::ComputeAlignedSizeOf(foo->data()) +
        mojo::internal::ComputeAlignedSizeOf(foo->extra_bars()) +
        mojo::internal::ComputeAlignedSizeOf(foo->name()) +
        mojo::internal::ComputeAlignedSizeOf(foo->files());
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
    Encode(&foo->d_.bar, handles);
    Encode(&foo->d_.data, handles);
    Encode(&foo->d_.extra_bars, handles);
    Encode(&foo->d_.name, handles);
    Encode(&foo->d_.files, handles);
  }

  static bool DecodePointersAndHandles(sample::Foo* foo,
                                       const mojo::Message& message) {
    if (!Decode(&foo->d_.bar, message))
      return false;
    if (!Decode(&foo->d_.data, message))
      return false;
    if (foo->header_.num_fields >= 8) {
      if (!Decode(&foo->d_.extra_bars, message))
        return false;
    }
    if (foo->header_.num_fields >= 9) {
      if (!Decode(&foo->d_.name, message))
        return false;
    }
    if (foo->header_.num_fields >= 10) {
      if (!Decode(&foo->d_.files, message))
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
    Encode(&params->d_.foo, handles);
    EncodeHandle(&params->d_.port, handles);
  }

  static bool DecodePointersAndHandles(
      sample::internal::Service_Frobinate_Params* params,
      const mojo::Message& message) {
    if (!Decode(&params->d_.foo, message))
      return false;
    if (params->header_.num_fields >= 3) {
      if (!DecodeHandle(&params->d_.port, message.handles))
        return false;
    }

    // TODO: validate
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_SERIALIZATION_H_
