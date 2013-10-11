// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/sample/generated/sample_service_stub.h"

#include "mojo/public/bindings/sample/generated/sample_service_serialization.h"

namespace sample {

bool ServiceStub::Accept(mojo::Message* message) {
  switch (message->data->header.name) {
    case internal::kService_Frobinate_Name: {
      internal::Service_Frobinate_Params* params =
          reinterpret_cast<internal::Service_Frobinate_Params*>(
              message->data->payload);

      if (!mojo::internal::DecodePointersAndHandles(params, *message))
        return false;

      Frobinate(params->foo(), params->baz(), params->port());
      break;
    }
  }
  return true;
}

}  // namespace sample
