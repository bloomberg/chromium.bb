// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/aura_demo/demo_context_factory.h"

#include "cc/output/output_surface.h"
#include "mojo/examples/aura_demo/window_tree_host_mojo.h"
#include "mojo/examples/compositor_app/mojo_context_provider.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace mojo {
namespace examples {

DemoContextFactory::DemoContextFactory(WindowTreeHostMojo* rwhm) : rwhm_(rwhm) {
}

DemoContextFactory::~DemoContextFactory() {
}

bool DemoContextFactory::Initialize() {
  if (gfx::GetGLImplementation() != gfx::kGLImplementationNone) {
    return true;
  }

  if (!gfx::GLSurface::InitializeOneOff() ||
      gfx::GetGLImplementation() == gfx::kGLImplementationNone) {
    LOG(ERROR) << "Could not load the GL bindings";
    return false;
  }

  return true;
}

scoped_ptr<cc::OutputSurface> DemoContextFactory::CreateOutputSurface(
    ui::Compositor* compositor, bool software_fallback) {
  return make_scoped_ptr(new cc::OutputSurface(
      new MojoContextProvider(rwhm_->TakeGLES2PipeHandle())));
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
DemoContextFactory::SharedMainThreadContextProvider() {
  if (!shared_main_thread_contexts_ ||
      shared_main_thread_contexts_->DestroyedOnMainThread()) {
    bool lose_context_when_out_of_memory = false;
    shared_main_thread_contexts_ =
        webkit::gpu::ContextProviderInProcess::CreateOffscreen(
            lose_context_when_out_of_memory);
    if (shared_main_thread_contexts_ &&
        !shared_main_thread_contexts_->BindToCurrentThread())
      shared_main_thread_contexts_ = NULL;
  }
  return shared_main_thread_contexts_;
}

void DemoContextFactory::RemoveCompositor(ui::Compositor* compositor) {
}

bool DemoContextFactory::DoesCreateTestContexts() { return false; }

}  // namespace examples
}  // namespace mojo
