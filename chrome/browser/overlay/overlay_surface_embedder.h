// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OVERLAY_OVERLAY_SURFACE_EMBEDDER_H_
#define CHROME_BROWSER_OVERLAY_OVERLAY_SURFACE_EMBEDDER_H_

#include <memory>

#include "chrome/browser/overlay/overlay_window.h"

namespace viz {
class SurfaceId;
}

// Embed a surface into the OverlayWindow to show content. Responsible for
// setting up the surface layers that contain content to show on the
// OverlayWindow.
class OverlaySurfaceEmbedder {
 public:
  explicit OverlaySurfaceEmbedder(OverlayWindow* window);
  ~OverlaySurfaceEmbedder();

  void SetPrimarySurfaceId(const viz::SurfaceId& surface_id);

 private:
  // The window which embeds the client. Weak pointer since the
  // PictureInPictureWindowController owns the window.
  OverlayWindow* window_;

  // Contains the client's content.
  std::unique_ptr<ui::Layer> surface_layer_;

  DISALLOW_COPY_AND_ASSIGN(OverlaySurfaceEmbedder);
};

#endif  // CHROME_BROWSER_OVERLAY_OVERLAY_SURFACE_EMBEDDER_H_
