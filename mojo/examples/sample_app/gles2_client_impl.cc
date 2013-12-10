// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/gles2_client_impl.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "mojo/public/gles2/gles2.h"

namespace mojo {
namespace examples {

GLES2ClientImpl::GLES2ClientImpl(ScopedMessagePipeHandle pipe)
    : service_(pipe.Pass()) {
  service_.SetPeer(this);
}

GLES2ClientImpl::~GLES2ClientImpl() {
  service_->Destroy();
}

void GLES2ClientImpl::DidCreateContext(uint64_t encoded,
                                       uint32_t width,
                                       uint32_t height) {
  MojoGLES2MakeCurrent(encoded);

  cube_.Init(width, height);
  last_time_ = base::Time::Now();
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(16),
               this, &GLES2ClientImpl::Draw);
}

void GLES2ClientImpl::Draw() {
  base::Time now = base::Time::Now();
  base::TimeDelta offset = now - last_time_;
  last_time_ = now;
  cube_.Update(offset.InSecondsF());
  cube_.Draw();

  MojoGLES2SwapBuffers();
}

void GLES2ClientImpl::ContextLost() {
  timer_.Stop();
}

}  // namespace examples
}  // namespace mojo
