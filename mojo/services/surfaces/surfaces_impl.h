// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_SURFACES_SURFACES_IMPL_H_
#define MOJO_SERVICES_SURFACES_SURFACES_IMPL_H_

#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/public/interfaces/gpu/command_buffer.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"

namespace cc {
class Display;
}

namespace mojo {
class ApplicationManager;

class SurfaceNativeViewportClient;

class SurfacesImpl : public InterfaceImpl<Surface>,
                     public cc::SurfaceFactoryClient,
                     public cc::DisplayClient {
 public:
  class Client {
   public:
    virtual void FrameSubmitted() = 0;
    virtual void SetDisplay(cc::Display*) = 0;
  };

  SurfacesImpl(cc::SurfaceManager* manager,
               uint32_t id_namespace,
               Client* client);
  ~SurfacesImpl() override;

  // Surface implementation.
  void CreateSurface(SurfaceIdPtr id, mojo::SizePtr size) override;
  void SubmitFrame(SurfaceIdPtr id, FramePtr frame) override;
  void DestroySurface(SurfaceIdPtr id) override;
  void CreateGLES2BoundSurface(CommandBufferPtr gles2_client,
                               SurfaceIdPtr id,
                               mojo::SizePtr size) override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  // DisplayClient implementation.
  void DisplayDamaged() override;
  void DidSwapBuffers() override;
  void DidSwapBuffersComplete() override;
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void OutputSurfaceLost() override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;

  cc::SurfaceFactory* factory() { return &factory_; }

 private:
  cc::SurfaceManager* manager_;
  cc::SurfaceFactory factory_;
  uint32_t id_namespace_;
  Client* client_;
  scoped_ptr<cc::Display> display_;
  ScopedMessagePipeHandle command_buffer_handle_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_SURFACES_SURFACES_IMPL_H_
