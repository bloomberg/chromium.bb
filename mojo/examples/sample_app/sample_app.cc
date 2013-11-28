// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/message_loop/message_loop.h"
#include "mojo/common/bindings_support_impl.h"
#include "mojo/examples/sample_app/native_viewport_client_impl.h"
#include "mojo/public/bindings/gles2_client/gles2_client_impl.h"
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

void Start(ScopedMessagePipeHandle pipe) {
  printf("Starting sample app.\n");
  NativeViewportClientImpl client(pipe.Pass());
  client.Open();
  base::MessageLoop::current()->Run();
}

}  // namespace examples
}  // namespace mojo

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(MojoHandle pipe) {
  base::MessageLoop loop;
  mojo::common::BindingsSupportImpl bindings_support;
  mojo::BindingsSupport::Set(&bindings_support);
  mojo::GLES2ClientImpl::Initialize();

  mojo::ScopedMessagePipeHandle scoped_handle;
  scoped_handle.reset(mojo::MessagePipeHandle(pipe));
  mojo::examples::Start(scoped_handle.Pass());

  mojo::GLES2ClientImpl::Terminate();
  mojo::BindingsSupport::Set(NULL);
  return MOJO_RESULT_OK;
}
