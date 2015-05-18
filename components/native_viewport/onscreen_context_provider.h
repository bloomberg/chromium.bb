// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NATIVE_VIEWPORT_ONSCREEN_CONTEXT_PROVIDER_H_
#define COMPONENTS_NATIVE_VIEWPORT_ONSCREEN_CONTEXT_PROVIDER_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "components/gpu/public/interfaces/context_provider.mojom.h"
#include "components/gpu/public/interfaces/viewport_parameter_listener.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace gles2 {
class CommandBufferDriver;
class GpuState;
}

namespace native_viewport {

class OnscreenContextProvider : public mojo::ContextProvider {
 public:
  explicit OnscreenContextProvider(const scoped_refptr<gles2::GpuState>& state);
  ~OnscreenContextProvider() override;

  void Bind(mojo::InterfaceRequest<mojo::ContextProvider> request);

  void SetAcceleratedWidget(gfx::AcceleratedWidget widget);

 private:
  // mojo::ContextProvider implementation:
  void Create(mojo::ViewportParameterListenerPtr viewport_parameter_listener,
              const CreateCallback& callback) override;

  void CreateAndReturnCommandBuffer();
  void CommandBufferDestroyed(gles2::CommandBufferDriver* command_buffer);

  scoped_refptr<gles2::GpuState> state_;
  gfx::AcceleratedWidget widget_;
  mojo::ViewportParameterListenerPtr pending_listener_;
  CreateCallback pending_create_callback_;
  mojo::Binding<mojo::ContextProvider> binding_;
  std::set<gles2::CommandBufferDriver*> command_buffers_;

  DISALLOW_COPY_AND_ASSIGN(OnscreenContextProvider);
};

}  // namespace mojo

#endif  // COMPONENTS_NATIVE_VIEWPORT_ONSCREEN_CONTEXT_PROVIDER_H_
