// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_
#define COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"

namespace viz {

class TestFrameSinkManagerImpl : public mojom::FrameSinkManager {
 public:
  TestFrameSinkManagerImpl();
  ~TestFrameSinkManagerImpl() override;

  void BindRequest(mojom::FrameSinkManagerRequest request);

 private:
  // mojom::FrameSinkManager:
  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id) override {}
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) override {}
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label) override {}
  void CreateRootCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      const RendererSettings& renderer_settings,
      mojom::CompositorFrameSinkAssociatedRequest request,
      mojom::CompositorFrameSinkClientPtr client,
      mojom::DisplayPrivateAssociatedRequest display_private_request) override {
  }
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      mojom::CompositorFrameSinkRequest request,
      mojom::CompositorFrameSinkClientPtr client) override {}
  void RegisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override {}
  void UnregisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override {}
  void AssignTemporaryReference(const SurfaceId& surface_id,
                                const FrameSinkId& owner) override {}
  void DropTemporaryReference(const SurfaceId& surface_id) override {}

  mojo::Binding<mojom::FrameSinkManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameSinkManagerImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_
