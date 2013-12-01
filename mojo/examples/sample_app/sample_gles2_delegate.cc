// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/sample_gles2_delegate.h"

#include <stdio.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace mojo {
namespace examples {

SampleGLES2Delegate::SampleGLES2Delegate()
    : gl_(NULL) {
}

SampleGLES2Delegate::~SampleGLES2Delegate() {
}

void SampleGLES2Delegate::DidCreateContext(
    GLES2* gl, uint32_t width, uint32_t height) {
  gl_ = gl;
  cube_.Init(width, height);
  last_time_ = base::Time::Now();
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(16),
               this, &SampleGLES2Delegate::Draw);
}

void SampleGLES2Delegate::Draw() {
  base::Time now = base::Time::Now();
  base::TimeDelta offset = now - last_time_;
  last_time_ = now;
  cube_.Update(offset.InSecondsF());
  cube_.Draw();
  gl_->SwapBuffers();
}

void SampleGLES2Delegate::ContextLost(GLES2* gl) {
  timer_.Stop();
  gl_ = NULL;
}

}  // namespace examples
}  // namespace mojo
