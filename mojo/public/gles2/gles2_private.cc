// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/gles2/gles2_private.h"

#include <assert.h>
#include <stddef.h>

#include "mojo/public/gles2/gles2.h"
#include "mojo/public/gles2/gles2_interface.h"

static mojo::GLES2Support* g_gles2_support = NULL;
static mojo::GLES2Interface* g_gles2_interface = NULL;

extern "C" {

void MojoGLES2Initialize() {
  assert(g_gles2_support);
  return g_gles2_support->Initialize();
}

void MojoGLES2Terminate() {
  assert(g_gles2_support);
  return g_gles2_support->Terminate();
}

void MojoGLES2MakeCurrent(uint64_t encoded) {
  assert(g_gles2_support);
  g_gles2_support->MakeCurrent(encoded);
  g_gles2_interface = g_gles2_support->GetGLES2InterfaceForCurrentContext();
}

void MojoGLES2SwapBuffers() {
  assert(g_gles2_support);
  return g_gles2_support->SwapBuffers();
}

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  ReturnType gl##Function PARAMETERS {                             \
    return g_gles2_interface->Function ARGUMENTS;                  \
  }
#include "mojo/public/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL

}  // extern "C"

namespace mojo {

GLES2Support::~GLES2Support() {}

void GLES2Support::Init(GLES2Support* gles2_support) {
  assert(!g_gles2_support);
  g_gles2_support = gles2_support;
}

}  // namespace mojo
