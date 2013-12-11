// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/gles2/gles2_private.h"

#include <assert.h>
#include <stddef.h>

static mojo::GLES2Private* g_private = NULL;

extern "C" {

void MojoGLES2Initialize() {
  g_private->Initialize();
}

void MojoGLES2Terminate() {
  g_private->Terminate();
}

void MojoGLES2MakeCurrent(uint64_t encoded) {
  g_private->MakeCurrent(encoded);
}

void MojoGLES2SwapBuffers() {
  g_private->SwapBuffers();
}

}  // extern "C"

namespace mojo {

GLES2Private::~GLES2Private() {
}

void GLES2Private::Init(GLES2Private* priv) {
  assert(!g_private);
  g_private = priv;
}

}  // namespace mojo
