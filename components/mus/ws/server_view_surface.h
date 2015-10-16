// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_VIEW_SURFACE_H_
#define COMPONENTS_MUS_WS_SERVER_VIEW_SURFACE_H_

#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "components/mus/ws/ids.h"
#include "mojo/converters/surfaces/custom_surface_converter.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mus {

class ServerView;
class SurfacesState;

// Server side representation of a ViewSurface.
class ServerViewSurface : public mojom::Surface,
                          public cc::SurfaceFactoryClient,
                          public mojo::CustomSurfaceConverter {
 public:
  explicit ServerViewSurface(ServerView* view);

  ~ServerViewSurface() override;

  void Bind(mojo::InterfaceRequest<Surface> request,
            mojom::SurfaceClientPtr client);

  // mojom::Surface:
  void SubmitCompositorFrame(
      mojom::CompositorFramePtr frame,
      const SubmitCompositorFrameCallback& callback) override;

  // Returns the set of views referenced by the last CompositorFrame submitted
  // to this view.
  const std::set<ViewId>& referenced_view_ids() const {
    return referenced_view_ids_;
  }

  const cc::SurfaceId& id() const { return surface_id_; }

 private:
  // Takes a mojom::CompositorFrame |input|, and converts it into a
  // cc::CompositorFrame. Along the way, this conversion ensures that a
  // CompositorFrame of this view can only refer to views within its subtree.
  // Views referenced in |input| are stored in |referenced_view_ids_|.
  scoped_ptr<cc::CompositorFrame> ConvertCompositorFrame(
      const mojom::CompositorFramePtr& input);

  // Overriden from CustomSurfaceConverter:
  bool ConvertSurfaceDrawQuad(const mojom::QuadPtr& input,
                              const mojom::CompositorFrameMetadataPtr& metadata,
                              cc::SharedQuadState* sqs,
                              cc::RenderPass* render_pass) override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  // |view_| owns |this|.
  ServerView* const view_;

  // The set of Views referenced in the last submitted CompositorFrame.
  std::set<ViewId> referenced_view_ids_;
  gfx::Size last_submitted_frame_size_;

  cc::SurfaceId surface_id_;
  // TODO(fsamuel): As an optimization, we may want to move SurfaceIdAllocator
  // and SurfaceFactory to a separate class so that we don't keep destroying
  // them and creating new ones on navigation.
  cc::SurfaceIdAllocator surface_id_allocator_;
  cc::SurfaceFactory surface_factory_;

  mojom::SurfaceClientPtr client_;
  mojo::Binding<Surface> binding_;

  DISALLOW_COPY_AND_ASSIGN(ServerViewSurface);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_VIEW_SURFACE_H_
