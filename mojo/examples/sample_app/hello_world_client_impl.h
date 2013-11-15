// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_HELLO_WORLD_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_HELLO_WORLD_CLIENT_IMPL_H_

#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/hello_world_service.h"

namespace mojo {
namespace examples {

class HelloWorldClientImpl : public HelloWorldClientStub {
 public:
  explicit HelloWorldClientImpl(mojo::Handle pipe);
  virtual ~HelloWorldClientImpl();

  virtual void DidReceiveGreeting(int32_t result) MOJO_OVERRIDE;

  HelloWorldService* service() {
    return service_.get();
  }

 private:
  mojo::RemotePtr<HelloWorldService> service_;
};

}  // examples
}  // mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_HELLO_WORLD_CLIENT_IMPL_H_
