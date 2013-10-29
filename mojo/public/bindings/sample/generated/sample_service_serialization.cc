// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/sample/generated/sample_service_serialization.h"

namespace sample {
namespace internal {

// static
Service_Frobinate_Params* Service_Frobinate_Params::New(mojo::Buffer* buf) {
  return new (buf->Allocate(sizeof(Service_Frobinate_Params)))
      Service_Frobinate_Params();
}

Service_Frobinate_Params::Service_Frobinate_Params() {
  _header_.num_bytes = sizeof(*this);
  _header_.num_fields = 3;
}

}  // namespace internal
}  // namespace sample

namespace mojo {
namespace internal {

// static
void ObjectTraits<sample::internal::Service_Frobinate_Params>::
    EncodePointersAndHandles(
        sample::internal::Service_Frobinate_Params* params,
        std::vector<mojo::Handle>* handles) {
  Encode(&params->foo_, handles);
  EncodeHandle(&params->port_, handles);
}

// static
bool ObjectTraits<sample::internal::Service_Frobinate_Params>::
    DecodePointersAndHandles(
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

}  // namespace internal
}  // namespace mojo
