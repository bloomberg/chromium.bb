// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"

namespace mus {

class WindowSurfaceClient;
class Window;

// A WindowSurface is wrapper to simplify submitting CompositorFrames to Views,
// and receiving ReturnedResources.
class WindowSurface : public mojom::SurfaceClient {
 public:
  ~WindowSurface() override;

  // Called to indicate that the current thread has assumed control of this
  // object.
  void BindToThread();

  void SubmitCompositorFrame(mojom::CompositorFramePtr frame,
                             const mojo::Closure& callback);

  void set_client(WindowSurfaceClient* client) { client_ = client; }

 private:
  friend class Window;

  WindowSurface(mojo::InterfacePtrInfo<mojom::Surface> surface_info,
                mojo::InterfaceRequest<mojom::SurfaceClient> client_request);

  // SurfaceClient implementation:
  void ReturnResources(
      mojo::Array<mojom::ReturnedResourcePtr> resources) override;

  WindowSurfaceClient* client_;
  mojo::InterfacePtrInfo<mojom::Surface> surface_info_;
  mojo::InterfaceRequest<mojom::SurfaceClient> client_request_;
  mojom::SurfacePtr surface_;
  scoped_ptr<mojo::Binding<mojom::SurfaceClient>> client_binding_;
  bool bound_to_thread_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_SURFACE_H_
