// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_overlay_candidate_validator_ozone.h"

#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace content {

static ui::SurfaceFactoryOzone::BufferFormat GetOzoneFormat(
    cc::ResourceFormat overlay_format) {
  switch (overlay_format) {
    // TODO(dshwang): overlay video still uses RGBA_8888.
    case cc::RGBA_8888:
    case cc::BGRA_8888:
      return ui::SurfaceFactoryOzone::BGRA_8888;
    case cc::RGBA_4444:
    case cc::ALPHA_8:
    case cc::LUMINANCE_8:
    case cc::RGB_565:
    case cc::ETC1:
    case cc::RED_8:
      break;
  }
  NOTREACHED();
  return ui::SurfaceFactoryOzone::UNKNOWN;
}

BrowserCompositorOverlayCandidateValidatorOzone::
    BrowserCompositorOverlayCandidateValidatorOzone(
        gfx::AcceleratedWidget widget,
        ui::OverlayCandidatesOzone* overlay_candidates)
    : widget_(widget),
      overlay_candidates_(overlay_candidates),
      software_mirror_active_(false) {
}

BrowserCompositorOverlayCandidateValidatorOzone::
    ~BrowserCompositorOverlayCandidateValidatorOzone() {
}

void BrowserCompositorOverlayCandidateValidatorOzone::CheckOverlaySupport(
    cc::OverlayCandidateList* surfaces) {
  // SW mirroring copies out of the framebuffer, so we can't remove any
  // quads for overlaying, otherwise the output is incorrect.
  if (software_mirror_active_)
    return;

  DCHECK_GE(2U, surfaces->size());
  ui::OverlayCandidatesOzone::OverlaySurfaceCandidateList ozone_surface_list;
  ozone_surface_list.resize(surfaces->size());

  for (size_t i = 0; i < surfaces->size(); i++) {
    ozone_surface_list.at(i).transform = surfaces->at(i).transform;
    ozone_surface_list.at(i).format = GetOzoneFormat(surfaces->at(i).format);
    ozone_surface_list.at(i).display_rect = surfaces->at(i).display_rect;
    ozone_surface_list.at(i).crop_rect = surfaces->at(i).uv_rect;
    ozone_surface_list.at(i).plane_z_order = surfaces->at(i).plane_z_order;
  }

  overlay_candidates_->CheckOverlaySupport(&ozone_surface_list);
  DCHECK_EQ(surfaces->size(), ozone_surface_list.size());

  for (size_t i = 0; i < surfaces->size(); i++) {
    surfaces->at(i).overlay_handled = ozone_surface_list.at(i).overlay_handled;
    surfaces->at(i).display_rect = ozone_surface_list.at(i).display_rect;
  }
}

void BrowserCompositorOverlayCandidateValidatorOzone::SetSoftwareMirrorMode(
    bool enabled) {
  software_mirror_active_ = enabled;
}

}  // namespace content
