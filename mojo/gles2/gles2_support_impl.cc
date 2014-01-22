// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/gles2/gles2_support_impl.h"

#include "base/lazy_instance.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/public/gles2/gles2_interface.h"
#include "mojo/public/gles2/gles2_private.h"

namespace mojo {
namespace gles2 {

namespace {

class GLES2ImplForCommandBuffer : public GLES2Interface {
 public:
  GLES2ImplForCommandBuffer() : gpu_interface_(NULL) {}

  void set_gpu_interface(gpu::gles2::GLES2Interface* gpu_interface) {
    gpu_interface_ = gpu_interface;
  }
  gpu::gles2::GLES2Interface* gpu_interface() const { return gpu_interface_; }

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  virtual ReturnType Function PARAMETERS OVERRIDE {                \
    return gpu_interface_->Function ARGUMENTS;                     \
  }
#include "mojo/public/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL

 private:
  gpu::gles2::GLES2Interface* gpu_interface_;
  DISALLOW_COPY_AND_ASSIGN(GLES2ImplForCommandBuffer);
};

base::LazyInstance<GLES2ImplForCommandBuffer> g_gles2_interface =
    LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

GLES2SupportImpl::~GLES2SupportImpl() {}

// static
void GLES2SupportImpl::Init() { GLES2Support::Init(new GLES2SupportImpl()); }

void GLES2SupportImpl::Initialize() {}

void GLES2SupportImpl::Terminate() {}

void GLES2SupportImpl::MakeCurrent(uint64_t encoded) {
  // Ack, Hans! It's the giant hack.
  // TODO(abarth): Replace this hack with something more disciplined. Most
  // likley, we should receive a MojoHandle that we use to wire up the
  // client side of an out-of-process command buffer. Given that we're
  // still in-process, we just reinterpret_cast the value into a GL interface.
  gpu::gles2::GLES2Interface* gl_interface =
      reinterpret_cast<gpu::gles2::GLES2Interface*>(
          static_cast<uintptr_t>(encoded));
  g_gles2_interface.Get().set_gpu_interface(gl_interface);
}

void GLES2SupportImpl::SwapBuffers() {
  g_gles2_interface.Get().gpu_interface()->SwapBuffers();
}

GLES2Interface* GLES2SupportImpl::GetGLES2InterfaceForCurrentContext() {
  return &g_gles2_interface.Get();
}

}  // namespace gles2
}  // namespace mojo
