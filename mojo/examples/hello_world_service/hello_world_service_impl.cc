// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/hello_world_service/hello_world_service_impl.h"

#include <string>

#include "base/logging.h"

namespace mojo {
namespace examples {

HelloWorldServiceImpl::HelloWorldServiceImpl(const MessagePipeHandle& pipe)
    : client_(pipe) {
  client_.SetPeer(this);
}

void HelloWorldServiceImpl::Greeting(const String* greeting) {
  LOG(INFO) << greeting->To<std::string>();
  client_->DidReceiveGreeting(42);
}

HelloWorldServiceImpl::~HelloWorldServiceImpl() {}

}  // examples
}  // mojo
