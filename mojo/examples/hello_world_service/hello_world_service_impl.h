// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_HELLO_WORLD_SERVICE_HELLO_WORLD_SERVICE_IMPL_H_
#define MOJO_EXAMPLES_HELLO_WORLD_SERVICE_HELLO_WORLD_SERVICE_IMPL_H_

#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/hello_world_service.h"

namespace mojo {
namespace examples {

class HelloWorldServiceImpl : public HelloWorldServiceStub {
 public:
  explicit HelloWorldServiceImpl(ScopedMessagePipeHandle pipe);
  virtual ~HelloWorldServiceImpl();
  virtual void Greeting(const String& greeting) MOJO_OVERRIDE;

 private:
  RemotePtr<HelloWorldClient> client_;
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_HELLO_WORLD_SERVICE_HELLO_WORLD_SERVICE_IMPL_H_
