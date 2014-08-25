// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SURFACES_APP_CHILD_GL_IMPL_H_
#define MOJO_EXAMPLES_SURFACES_APP_CHILD_GL_IMPL_H_

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/examples/sample_app/spinning_cube.h"
#include "mojo/examples/surfaces_app/child.mojom.h"
#include "mojo/public/c/gles2/gles2.h"
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

namespace examples {

// Simple example of a child app using surfaces + GL.
class ChildGLImpl : public InterfaceImpl<Child>, public SurfaceClient {
 public:
  ChildGLImpl(ApplicationConnection* surfaces_service_connection,
              CommandBufferPtr command_buffer);
  virtual ~ChildGLImpl();

  // SurfaceClient implementation
  virtual void ReturnResources(Array<ReturnedResourcePtr> resources) OVERRIDE;

 private:
  // Child implementation.
  virtual void ProduceFrame(
      ColorPtr color,
      SizePtr size,
      const mojo::Callback<void(SurfaceIdPtr id)>& callback) OVERRIDE;

  void SurfaceConnectionCreated(SurfacePtr surface, uint32_t id_namespace);
  void AllocateSurface();
  void Draw();

  SkColor color_;
  gfx::Size size_;
  scoped_ptr<cc::SurfaceIdAllocator> allocator_;
  SurfacesServicePtr surfaces_service_;
  SurfacePtr surface_;
  MojoGLES2Context context_;
  cc::SurfaceId id_;
  ::examples::SpinningCube cube_;
  Callback<void(SurfaceIdPtr id)> produce_callback_;
  base::TimeTicks start_time_;
  uint32_t next_resource_id_;
  base::hash_map<uint32_t, GLuint> id_to_tex_map_;
  base::WeakPtrFactory<ChildGLImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildGLImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SURFACES_APP_CHILD_GL_IMPL_H_
