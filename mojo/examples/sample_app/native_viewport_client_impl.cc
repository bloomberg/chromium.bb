// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/native_viewport_client_impl.h"

#include <stdio.h>

#include "mojo/public/bindings/gles2_client/gles2_client_impl.h"

namespace mojo {
namespace examples {

NativeViewportClientImpl::NativeViewportClientImpl(ScopedMessagePipeHandle pipe)
    : service_(pipe.Pass()) {
  service_.SetPeer(this);
}

NativeViewportClientImpl::~NativeViewportClientImpl() {
}

void NativeViewportClientImpl::Open() {
  service_->Open();

  ScopedMessagePipeHandle gles2;
  ScopedMessagePipeHandle gles2_client;
  CreateMessagePipe(&gles2, &gles2_client);

  gles2_client_.reset(new GLES2ClientImpl(&gles2_delegate_, gles2.Pass()));
  service_->CreateGLES2Context(gles2_client.release());
}

void NativeViewportClientImpl::DidOpen() {
}

}  // namespace examples
}  // namespace mojo
