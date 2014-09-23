// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_SURFACES_SURFACES_IMPL_H_
#define MOJO_SERVICES_SURFACES_SURFACES_IMPL_H_

#include "base/compiler_specific.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/gles2/command_buffer.mojom.h"
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
  virtual ~SurfacesImpl();

  // Surface implementation.
  virtual void CreateSurface(SurfaceIdPtr id, mojo::SizePtr size) OVERRIDE;
  virtual void SubmitFrame(SurfaceIdPtr id, FramePtr frame) OVERRIDE;
  virtual void DestroySurface(SurfaceIdPtr id) OVERRIDE;
  virtual void CreateGLES2BoundSurface(CommandBufferPtr gles2_client,
                                       SurfaceIdPtr id,
                                       mojo::SizePtr size) OVERRIDE;

  // SurfaceFactoryClient implementation.
  virtual void ReturnResources(
      const cc::ReturnedResourceArray& resources) OVERRIDE;

  // DisplayClient implementation.
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface() OVERRIDE;
  virtual void DisplayDamaged() OVERRIDE;
  virtual void DidSwapBuffers() OVERRIDE;
  virtual void DidSwapBuffersComplete() OVERRIDE;
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) OVERRIDE;

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
