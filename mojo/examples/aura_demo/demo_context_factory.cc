// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/aura_demo/demo_context_factory.h"

#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "mojo/examples/aura_demo/root_window_host_mojo.h"
#include "mojo/examples/compositor_app/gles2_client_impl.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace mojo {
namespace examples {

namespace {

class MojoContextProvider : public cc::ContextProvider {
 public:
  explicit MojoContextProvider(GLES2ClientImpl* gles2_client_impl)
      : gles2_client_impl_(gles2_client_impl) {}

  // cc::ContextProvider implementation.
  virtual bool BindToCurrentThread() OVERRIDE { return true; }
  virtual gpu::gles2::GLES2Interface* ContextGL() OVERRIDE {
    return gles2_client_impl_->Interface();
  }
  virtual gpu::ContextSupport* ContextSupport() OVERRIDE {
    return gles2_client_impl_->Support();
  }
  virtual class GrContext* GrContext() OVERRIDE { return NULL; }
  virtual Capabilities ContextCapabilities() OVERRIDE { return capabilities_; }
  virtual bool IsContextLost() OVERRIDE {
    return !gles2_client_impl_->Interface();
  }
  virtual void VerifyContexts() OVERRIDE {}
  virtual bool DestroyedOnMainThread() OVERRIDE {
    return !gles2_client_impl_->Interface();
  }
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) OVERRIDE {}
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      OVERRIDE {}

 protected:
  friend class base::RefCountedThreadSafe<MojoContextProvider>;
  virtual ~MojoContextProvider() {}

 private:
  cc::ContextProvider::Capabilities capabilities_;
  GLES2ClientImpl* gles2_client_impl_;
};

}  // namespace

DemoContextFactory::DemoContextFactory(WindowTreeHostMojo* rwhm) : rwhm_(rwhm) {
}

DemoContextFactory::~DemoContextFactory() {
}

bool DemoContextFactory::Initialize() {
  if (!gfx::GLSurface::InitializeOneOff() ||
      gfx::GetGLImplementation() == gfx::kGLImplementationNone) {
    LOG(ERROR) << "Could not load the GL bindings";
    return false;
  }
  return true;
}

scoped_ptr<cc::OutputSurface> DemoContextFactory::CreateOutputSurface(
    ui::Compositor* compositor, bool software_fallback) {
  return make_scoped_ptr(
      new cc::OutputSurface(new MojoContextProvider(rwhm_->gles2_client())));
}

scoped_refptr<ui::Reflector> DemoContextFactory::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  return NULL;
}

void DemoContextFactory::RemoveReflector(
    scoped_refptr<ui::Reflector> reflector) {
}

scoped_refptr<cc::ContextProvider>
DemoContextFactory::OffscreenCompositorContextProvider() {
  if (!offscreen_compositor_contexts_.get() ||
      !offscreen_compositor_contexts_->DestroyedOnMainThread()) {
    offscreen_compositor_contexts_ =
        webkit::gpu::ContextProviderInProcess::CreateOffscreen();
  }
  return offscreen_compositor_contexts_;
}

scoped_refptr<cc::ContextProvider>
DemoContextFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->DestroyedOnMainThread())
    return shared_main_thread_contexts_;

  if (ui::Compositor::WasInitializedWithThread()) {
    shared_main_thread_contexts_ =
        webkit::gpu::ContextProviderInProcess::CreateOffscreen();
  } else {
    shared_main_thread_contexts_ =
        static_cast<webkit::gpu::ContextProviderInProcess*>(
            OffscreenCompositorContextProvider().get());
  }
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;

  return shared_main_thread_contexts_;
}

void DemoContextFactory::RemoveCompositor(ui::Compositor* compositor) {
}

bool DemoContextFactory::DoesCreateTestContexts() { return false; }

}  // namespace examples
}  // namespace mojo

