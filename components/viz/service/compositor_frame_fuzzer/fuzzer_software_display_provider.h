// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_DISPLAY_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_DISPLAY_PROVIDER_H_

#include <memory>

#include "components/viz/service/display/display.h"
#include "components/viz/service/display_embedder/display_provider.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"

namespace viz {

// DisplayProvider implementation that provides Displays that use a
// SoftwareOutputSurface (no-op by default, with an option to dump pixmap to a
// PNG for debugging).
//
// Provided Displays will only draw when explicitly asked to
// (e.g. if Display::ForceImmediateDrawAndSwapIfPossible() is called),
// ignoring the BeginFrameSource parameters passed to CreateDisplay.
class FuzzerSoftwareDisplayProvider : public DisplayProvider {
 public:
  explicit FuzzerSoftwareDisplayProvider(
      base::Optional<base::FilePath> png_dir_path);
  ~FuzzerSoftwareDisplayProvider() override;

  // DisplayProvider implementation.
  std::unique_ptr<Display> CreateDisplay(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      ExternalBeginFrameSource* external_begin_frame_source,
      SyntheticBeginFrameSource* synthetic_begin_frame_source,
      const RendererSettings& renderer_settings,
      bool send_swap_size_notifications) override;
  uint32_t GetRestartId() const override;

 private:
  base::Optional<base::FilePath> png_dir_path_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<StubBeginFrameSource> begin_frame_source_;

  DISALLOW_COPY_AND_ASSIGN(FuzzerSoftwareDisplayProvider);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_DISPLAY_PROVIDER_H_
