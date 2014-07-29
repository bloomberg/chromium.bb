// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "mojo/examples/echo/echo_service.mojom.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/utility/run_loop.h"

namespace mojo {
namespace examples {

class ResponsePrinter {
 public:
  void Run(const String& value) const {
    printf("Response: \"%s\"\n", value.get().c_str());

    RunLoop::current()->Quit();  // All done!
  }
};

class EchoClientDelegate : public ApplicationDelegate {
 public:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    app->ConnectToService("mojo:mojo_echo_service", &echo_service_);

    echo_service_->EchoString("hello world", ResponsePrinter());
  }

 private:
  EchoServicePtr echo_service_;
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::EchoClientDelegate();
}

}  // namespace mojo
