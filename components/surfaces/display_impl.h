// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACES_DISPLAY_IMPL_H_
#define COMPONENTS_SURFACES_DISPLAY_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "components/surfaces/public/interfaces/display.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace cc {
class Display;
class SurfaceFactory;
}

namespace surfaces {
class SurfacesScheduler;
class SurfacesServiceApplication;

class DisplayImpl : public mojo::Display,
                    public mojo::ViewportParameterListener,
                    public cc::DisplayClient,
                    public cc::SurfaceFactoryClient {
 public:
  DisplayImpl(SurfacesServiceApplication* application,
              cc::SurfaceManager* manager,
              cc::SurfaceId cc_id,
              SurfacesScheduler* scheduler,
              mojo::ContextProviderPtr context_provider,
              mojo::ResourceReturnerPtr returner,
              mojo::InterfaceRequest<mojo::Display> display_request);
  ~DisplayImpl() override;

 private:
  void OnContextCreated(mojo::CommandBufferPtr gles2_client);

  // mojo::Display implementation:
  void SubmitFrame(mojo::FramePtr frame,
                   const SubmitFrameCallback& callback) override;

  // DisplayClient implementation.
  void DisplayDamaged() override;
  void DidSwapBuffers() override;
  void DidSwapBuffersComplete() override;
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void OutputSurfaceLost() override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;

  // ViewportParameterListener
  void OnVSyncParametersUpdated(int64_t timebase, int64_t interval) override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  void Draw();

  SurfacesServiceApplication* application_;
  cc::SurfaceManager* manager_;
  cc::SurfaceFactory factory_;
  cc::SurfaceId cc_id_;
  SurfacesScheduler* scheduler_;
  mojo::ContextProviderPtr context_provider_;
  mojo::ResourceReturnerPtr returner_;

  mojo::FramePtr pending_frame_;
  SubmitFrameCallback pending_callback_;

  scoped_ptr<cc::Display> display_;

  mojo::Binding<mojo::ViewportParameterListener> viewport_param_binding_;
  mojo::StrongBinding<mojo::Display> display_binding_;

  DISALLOW_COPY_AND_ASSIGN(DisplayImpl);
};

}  // namespace surfaces

#endif  // COMPONENTS_SURFACES_DISPLAY_IMPL_H_
