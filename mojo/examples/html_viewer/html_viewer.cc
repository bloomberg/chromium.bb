// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/interfaces/launcher/launcher.mojom.h"

namespace mojo {
namespace examples {

class HTMLViewer;

class LaunchableConnection : public InterfaceImpl<launcher::Launchable> {
 public:
  LaunchableConnection() {}
  virtual ~LaunchableConnection() {}

 private:
  // Overridden from launcher::Launchable:
  virtual void OnLaunch(
      URLResponsePtr response,
      ScopedDataPipeConsumerHandle response_body_stream) MOJO_OVERRIDE {
    printf("In HTMLViewer, rendering url: %s\n", response->url.data());
    printf("HTML: \n");
    for (;;) {
      char buf[512];
      uint32_t num_bytes = sizeof(buf);
      MojoResult result = ReadDataRaw(
          response_body_stream.get(),
          buf,
          &num_bytes,
          MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        Wait(response_body_stream.get(),
             MOJO_WAIT_FLAG_READABLE,
             MOJO_DEADLINE_INDEFINITE);
      } else if (result == MOJO_RESULT_OK) {
        fwrite(buf, num_bytes, 1, stdout);
      } else {
        break;
      }
    }
    printf("\n>>>> EOF <<<<\n\n");
  }
};

class HTMLViewer : public Application {
 public:
  HTMLViewer() {}
  virtual ~HTMLViewer() {}

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    AddService<LaunchableConnection>();
  }
};

}

// static
Application* Application::Create() {
  return new examples::HTMLViewer;
}

}
