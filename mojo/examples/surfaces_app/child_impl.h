// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SURFACES_APP_CHILD_IMPL_H_
#define MOJO_EXAMPLES_SURFACES_APP_CHILD_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/examples/surfaces_app/child.mojom.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/services/public/interfaces/surfaces/surface_id.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

namespace cc {
class CompositorFrame;
}

namespace mojo {

class ApplicationConnection;

namespace surfaces {
class Surface;
}

namespace examples {

// Simple example of a child app using surfaces.
class ChildImpl : public InterfaceImpl<Child>, public SurfaceClient {
 public:
  class Context {
   public:
    virtual ApplicationConnection* ShellConnection(
        const mojo::String& application_url) = 0;
  };
  explicit ChildImpl(ApplicationConnection* surfaces_service_connection);
  virtual ~ChildImpl();

  // SurfaceClient implementation
  virtual void ReturnResources(
      Array<ReturnedResourcePtr> resources) OVERRIDE;

 private:
  // Child implementation.
  virtual void ProduceFrame(
      ColorPtr color,
      SizePtr size,
      const mojo::Callback<void(SurfaceIdPtr id)>& callback) OVERRIDE;

  void SurfaceConnectionCreated(SurfacePtr surface, uint32_t id_namespace);
  void Draw();

  SkColor color_;
  gfx::Size size_;
  scoped_ptr<cc::SurfaceIdAllocator> allocator_;
  SurfacesServicePtr surfaces_service_;
  SurfacePtr surface_;
  cc::SurfaceId id_;
  mojo::Callback<void(SurfaceIdPtr id)> produce_callback_;
  base::WeakPtrFactory<ChildImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SURFACES_APP_CHILD_IMPL_H_
