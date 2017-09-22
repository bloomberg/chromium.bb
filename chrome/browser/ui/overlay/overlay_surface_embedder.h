// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OVERLAY_OVERLAY_SURFACE_EMBEDDER_H_
#define CHROME_BROWSER_UI_OVERLAY_OVERLAY_SURFACE_EMBEDDER_H_

#include "chrome/browser/ui/overlay/overlay_window.h"
#include "components/viz/common/surfaces/surface_reference_factory.h"

namespace viz {
class SurfaceInfo;
}

// Embed a surface into the OverlayWindow to show content. Responsible for
// setting up the surface layers that contain content to show on the
// OverlayWindow.
class OverlaySurfaceEmbedder {
 public:
  explicit OverlaySurfaceEmbedder(OverlayWindow* window);
  ~OverlaySurfaceEmbedder();

  void SetPrimarySurfaceInfo(const viz::SurfaceInfo& surface_info);

 private:
  // The window which embeds the client.
  std::unique_ptr<OverlayWindow> window_;

  // Contains the client's content.
  std::unique_ptr<ui::Layer> surface_layer_;

  scoped_refptr<viz::SurfaceReferenceFactory> ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(OverlaySurfaceEmbedder);
};

#endif  // CHROME_BROWSER_UI_OVERLAY_OVERLAY_SURFACE_EMBEDDER_H_
