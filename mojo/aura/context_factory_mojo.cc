// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/aura/context_factory_mojo.h"

#include "cc/output/output_surface.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/cc/context_provider_mojo.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace mojo {

ContextFactoryMojo::ContextFactoryMojo(ScopedMessagePipeHandle gles2_handle)
    : gles2_handle_(gles2_handle.Pass()) {
}

ContextFactoryMojo::~ContextFactoryMojo() {
}

scoped_ptr<cc::OutputSurface> ContextFactoryMojo::CreateOutputSurface(
    ui::Compositor* compositor, bool software_fallback) {
  return make_scoped_ptr(new cc::OutputSurface(
                             new ContextProviderMojo(gles2_handle_.Pass())));
}

scoped_refptr<ui::Reflector> ContextFactoryMojo::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  return NULL;
}

void ContextFactoryMojo::RemoveReflector(
    scoped_refptr<ui::Reflector> reflector) {
}

scoped_refptr<cc::ContextProvider>
ContextFactoryMojo::SharedMainThreadContextProvider() {
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

void ContextFactoryMojo::RemoveCompositor(ui::Compositor* compositor) {
}

bool ContextFactoryMojo::DoesCreateTestContexts() { return false; }

cc::SharedBitmapManager* ContextFactoryMojo::GetSharedBitmapManager() {
  return NULL;
}

}  // namespace mojo
