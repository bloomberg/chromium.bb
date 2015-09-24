// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_VIEW_SURFACE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_VIEW_SURFACE_H_

#include "base/observer_list.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr_info.h"

namespace mus {

class ViewSurfaceClient;
class View;

// A ViewSurface is wrapper to simplify submitting CompositorFrames to Views,
// and receiving ReturnedResources.
class ViewSurface : public mojo::SurfaceClient {
 public:
  ~ViewSurface() override;

  // Called to indicate that the current thread has assumed control of this
  // object.
  void BindToThread();

  void SubmitCompositorFrame(mojo::CompositorFramePtr frame,
                             const mojo::Closure& callback);

  void set_client(ViewSurfaceClient* client) { client_ = client; }

 private:
  friend class View;

  ViewSurface(mojo::InterfacePtrInfo<mojo::Surface> surface_info,
              mojo::InterfaceRequest<mojo::SurfaceClient> client_request);

  // SurfaceClient implementation:
  void ReturnResources(
      mojo::Array<mojo::ReturnedResourcePtr> resources) override;

  ViewSurfaceClient* client_;
  mojo::InterfacePtrInfo<mojo::Surface> surface_info_;
  mojo::InterfaceRequest<mojo::SurfaceClient> client_request_;
  mojo::SurfacePtr surface_;
  mojo::Binding<mojo::SurfaceClient> client_binding_;
  bool bound_to_thread_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_VIEW_SURFACE_H_
