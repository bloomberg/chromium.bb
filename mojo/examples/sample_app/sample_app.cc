// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/message_loop/message_loop.h"
#include "mojo/common/bindings_support_impl.h"
#include "mojo/examples/sample_app/hello_world_client_impl.h"
#include "mojo/public/bindings/lib/bindings_support.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/macros.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define SAMPLE_APP_EXPORT __declspec(dllexport)
#else
#define CDECL
#define SAMPLE_APP_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace examples {

static HelloWorldClientImpl* g_client = 0;

void SayHello(mojo::Handle pipe) {
  g_client = new HelloWorldClientImpl(pipe);

  mojo::ScratchBuffer buf;
  const std::string kGreeting("hello, world!");
  mojo::String* greeting = mojo::String::NewCopyOf(&buf, kGreeting);

  g_client->service()->Greeting(greeting);
}

}  // examples
}  // mojo

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(
    mojo::Handle pipe) {
  // Create a message loop on this thread for processing incoming messages.
  // This creates a dependency on base that we'll be removing soon.
  base::MessageLoop loop;

  // Set the global bindings support.
  mojo::common::BindingsSupportImpl bindings_support;
  mojo::BindingsSupport::Set(&bindings_support);

  // Send message out.
  mojo::examples::SayHello(pipe);

  // Run loop to receieve Ack.
  loop.Run();

  mojo::BindingsSupport::Set(NULL);
  return MOJO_RESULT_OK;
}
