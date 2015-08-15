// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_IMPL_H_
#define COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "components/view_manager/public/interfaces/display.mojom.h"
#include "components/view_manager/surfaces/surfaces_state.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace cc {
class Display;
class SurfaceFactory;
}

namespace surfaces {

class DisplayDelegate;

class DisplayImpl : public mojo::Display,
                    public mojo::ViewportParameterListener,
                    public cc::DisplayClient,
                    public cc::SurfaceFactoryClient {
 public:
  DisplayImpl(DisplayDelegate* display_delegate,
              const scoped_refptr<SurfacesState>& surfaces_state,
              cc::SurfaceId cc_id,
              mojo::ContextProviderPtr context_provider,
              mojo::ResourceReturnerPtr returner,
              mojo::InterfaceRequest<mojo::Display> display_request);

  // Closes the connection and destroys |this| object.
  void CloseConnection();

 private:
  ~DisplayImpl() override;

  void OnContextCreated(mojo::CommandBufferPtr gles2_client);

  // mojo::Display implementation:
  void SubmitFrame(mojo::FramePtr frame,
                   const SubmitFrameCallback& callback) override;

  // DisplayClient implementation.
  // TODO(rjkroege, fsamuel): This won't work correctly with multiple displays.
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void OutputSurfaceLost() override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;

  // ViewportParameterListener
  void OnVSyncParametersUpdated(int64_t timebase, int64_t interval) override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  void Draw();

  DisplayDelegate* delegate_;
  scoped_refptr<SurfacesState> surfaces_state_;
  cc::SurfaceFactory factory_;
  cc::SurfaceId cc_id_;
  mojo::ContextProviderPtr context_provider_;
  mojo::ResourceReturnerPtr returner_;

  gfx::Size last_submitted_frame_size_;
  mojo::FramePtr pending_frame_;
  SubmitFrameCallback pending_callback_;

  scoped_ptr<cc::Display> display_;

  mojo::Binding<mojo::ViewportParameterListener> viewport_param_binding_;
  mojo::Binding<mojo::Display> display_binding_;
  bool connection_closed_;

  DISALLOW_COPY_AND_ASSIGN(DisplayImpl);
};

}  // namespace surfaces

#endif  // COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_IMPL_H_
