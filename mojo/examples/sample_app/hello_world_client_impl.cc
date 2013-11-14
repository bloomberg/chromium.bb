// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/hello_world_client_impl.h"

#include <stdio.h>

namespace mojo {
namespace examples {

HelloWorldClientImpl::HelloWorldClientImpl(mojo::Handle pipe)
    : service_(pipe) {
  service_.SetPeer(this);
}

void HelloWorldClientImpl::DidReceiveGreeting(int32_t result) {
  printf("DidReceiveGreeting from pipe: %d\n", result);
}

HelloWorldClientImpl::~HelloWorldClientImpl() {}

}  // examples
}  // mojo
