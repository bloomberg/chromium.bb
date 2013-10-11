// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/sample/generated/sample_service_proxy.h"

#include <stdlib.h>

#include "mojo/public/bindings/lib/message.h"
#include "mojo/public/bindings/lib/message_builder.h"
#include "mojo/public/bindings/sample/generated/sample_service_serialization.h"

namespace sample {

ServiceProxy::ServiceProxy(mojo::MessageReceiver* receiver)
    : receiver_(receiver) {
}

void ServiceProxy::Frobinate(const Foo* foo, bool baz, mojo::Handle port) {
  size_t payload_size =
      mojo::internal::Align(sizeof(internal::Service_Frobinate_Params));
  payload_size += mojo::internal::ComputeAlignedSizeOf(foo);

  mojo::MessageBuilder builder(internal::kService_Frobinate_Name, payload_size);

  // We now go about allocating the anonymous Frobinate_Params struct.  It
  // holds the parameters to the Frobinate message.
  //
  // Notice how foo is cloned. This causes a copy of foo to be generated
  // within the same buffer as the Frobinate_Params struct. That's what we
  // need in order to generate a contiguous blob of message data.

  internal::Service_Frobinate_Params* params =
      internal::Service_Frobinate_Params::New(builder.buffer());
  params->set_foo(mojo::internal::Clone(foo, builder.buffer()));
  params->set_baz(baz);
  params->set_port(port);

  // NOTE: If foo happened to be a graph with cycles, then Clone would not
  // have returned.

  // Next step is to encode pointers and handles so that messages become
  // hermetic. Pointers become offsets and handles becomes indices into the
  // handles array.
  mojo::Message message;
  mojo::internal::EncodePointersAndHandles(params, &message.handles);

  // Finally, we get the generated message data, and forward it to the
  // receiver.
  message.data = builder.Finish();

  receiver_->Accept(&message);

  free(message.data);
}

}  // namespace sample
