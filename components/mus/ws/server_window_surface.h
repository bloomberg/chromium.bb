// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_H_
#define COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_H_

#include <set>

#include "base/macros.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/ids.h"
#include "mojo/converters/surfaces/custom_surface_converter.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {

class SurfacesState;

namespace ws {

class ServerWindow;
class ServerWindowSurfaceManager;

// Server side representation of a WindowSurface.
class ServerWindowSurface : public mojom::Surface,
                            public cc::SurfaceFactoryClient,
                            public mojo::CustomSurfaceConverter {
 public:
  ServerWindowSurface(ServerWindowSurfaceManager* manager,
                      mojom::SurfaceType surface_type,
                      mojo::InterfaceRequest<mojom::Surface> request,
                      mojom::SurfaceClientPtr client);

  ~ServerWindowSurface() override;

  const gfx::Size& last_submitted_frame_size() const {
    return last_submitted_frame_size_;
  }

  // mojom::Surface:
  void SubmitCompositorFrame(
      mojom::CompositorFramePtr frame,
      const SubmitCompositorFrameCallback& callback) override;

  // Returns the set of windows referenced by the last CompositorFrame submitted
  // to this window.
  const std::set<WindowId>& referenced_window_ids() const {
    return referenced_window_ids_;
  }

  const cc::SurfaceId& id() const { return surface_id_; }

  // Destroys old surfaces that have been outdated by a new surface.
  void DestroySurfacesScheduledForDestruction();

  // Registers this with the SurfaceManager
  void RegisterForBeginFrames();

 private:
  ServerWindow* window();

  // Takes a mojom::CompositorFrame |input|, and converts it into a
  // cc::CompositorFrame. Along the way, this conversion ensures that a
  // CompositorFrame of this window can only refer to windows within its
  // subtree. Windows referenced in |input| are stored in
  // |referenced_window_ids_|.
  std::unique_ptr<cc::CompositorFrame> ConvertCompositorFrame(
      const mojom::CompositorFramePtr& input);

  // Overriden from CustomSurfaceConverter:
  bool ConvertSurfaceDrawQuad(const mojom::QuadPtr& input,
                              const mojom::CompositorFrameMetadataPtr& metadata,
                              cc::SharedQuadState* sqs,
                              cc::RenderPass* render_pass) override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  ServerWindowSurfaceManager* manager_;  // Owns this.

  const mojom::SurfaceType surface_type_;

  // The set of Windows referenced in the last submitted CompositorFrame.
  std::set<WindowId> referenced_window_ids_;
  gfx::Size last_submitted_frame_size_;

  cc::SurfaceId surface_id_;
  cc::SurfaceFactory surface_factory_;

  mojom::SurfaceClientPtr client_;
  mojo::Binding<Surface> binding_;

  // Set of surface ids that need to be destroyed.
  std::set<cc::SurfaceId> surfaces_scheduled_for_destruction_;

  bool registered_surface_factory_client_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowSurface);
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_H_
