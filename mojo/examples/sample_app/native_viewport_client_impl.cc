// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/native_viewport_client_impl.h"

#include <stdio.h>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace mojo {
namespace examples {

NativeViewportClientImpl::NativeViewportClientImpl(ScopedMessagePipeHandle pipe)
    : service_(pipe.Pass()) {
  service_.SetPeer(this);
}

NativeViewportClientImpl::~NativeViewportClientImpl() {
  service_->Close();
}

void NativeViewportClientImpl::Open() {
  service_->Open();

  ScopedMessagePipeHandle gles2;
  ScopedMessagePipeHandle gles2_client;
  CreateMessagePipe(&gles2, &gles2_client);

  gles2_client_.reset(new GLES2ClientImpl(gles2.Pass()));
  service_->CreateGLES2Context(gles2_client.Pass());
}

void NativeViewportClientImpl::OnCreated() {
}

void NativeViewportClientImpl::OnDestroyed() {
  base::MessageLoop::current()->Quit();
}

void NativeViewportClientImpl::OnEvent(const Event& event) {
  if (!event.location().is_null()) {
    LOG(INFO) << "Located Event @"
              << event.location().x() << "," << event.location().y();
  }
}

}  // namespace examples
}  // namespace mojo
