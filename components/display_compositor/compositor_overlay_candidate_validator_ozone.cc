// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/compositor_overlay_candidate_validator_ozone.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "cc/output/overlay_strategy_fullscreen.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "cc/output/overlay_strategy_underlay_cast.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace display_compositor {
namespace {
// Templated function used to create an OverlayProcessor::Strategy
// of type |S|.
template <typename S>
std::unique_ptr<cc::OverlayProcessor::Strategy> MakeOverlayStrategy(
    CompositorOverlayCandidateValidatorOzone* capability_checker) {
  return base::MakeUnique<S>(capability_checker);
}

}  // namespace

static gfx::BufferFormat GetBufferFormat(cc::ResourceFormat overlay_format) {
  switch (overlay_format) {
    // TODO(dshwang): overlay video still uses RGBA_8888.
    case cc::RGBA_8888:
    case cc::BGRA_8888:
      return gfx::BufferFormat::BGRA_8888;
    default:
      NOTREACHED();
      return gfx::BufferFormat::BGRA_8888;
  }
}

// |overlay_candidates| is an object used to answer questions about possible
// overlays configuarations.
// |strategies_string| is a comma-separated string containing all the overaly
// strategies that should be returned by GetStrategies.
// If |strategies_string| is empty "single-on-top,underlay" will be used as
// default.
CompositorOverlayCandidateValidatorOzone::
    CompositorOverlayCandidateValidatorOzone(
        std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
        std::string strategies_string)
    : overlay_candidates_(std::move(overlay_candidates)),
      software_mirror_active_(false) {
  if (!strategies_string.length())
    strategies_string = "single-on-top,underlay";

  for (const auto& strategy_name :
       base::SplitStringPiece(strategies_string, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (strategy_name == "single-fullscreen") {
      strategies_instantiators_.push_back(
          base::Bind(MakeOverlayStrategy<cc::OverlayStrategyFullscreen>));
    } else if (strategy_name == "single-on-top") {
      strategies_instantiators_.push_back(
          base::Bind(MakeOverlayStrategy<cc::OverlayStrategySingleOnTop>));
    } else if (strategy_name == "underlay") {
      strategies_instantiators_.push_back(
          base::Bind(MakeOverlayStrategy<cc::OverlayStrategyUnderlay>));
    } else if (strategy_name == "cast") {
      strategies_instantiators_.push_back(
          base::Bind(MakeOverlayStrategy<cc::OverlayStrategyUnderlayCast>));
    } else {
      LOG(WARNING) << "Unrecognized overlay strategy " << strategy_name;
    }
  }
}

CompositorOverlayCandidateValidatorOzone::
    ~CompositorOverlayCandidateValidatorOzone() {}

void CompositorOverlayCandidateValidatorOzone::GetStrategies(
    cc::OverlayProcessor::StrategyList* strategies) {
  for (auto& instantiator : strategies_instantiators_)
    strategies->push_back(instantiator.Run(this));
}

bool CompositorOverlayCandidateValidatorOzone::AllowCALayerOverlays() {
  return false;
}

void CompositorOverlayCandidateValidatorOzone::CheckOverlaySupport(
    cc::OverlayCandidateList* surfaces) {
  // SW mirroring copies out of the framebuffer, so we can't remove any
  // quads for overlaying, otherwise the output is incorrect.
  if (software_mirror_active_) {
    for (size_t i = 0; i < surfaces->size(); i++) {
      surfaces->at(i).overlay_handled = false;
    }
    return;
  }

  DCHECK_GE(2U, surfaces->size());
  ui::OverlayCandidatesOzone::OverlaySurfaceCandidateList ozone_surface_list;
  ozone_surface_list.resize(surfaces->size());

  for (size_t i = 0; i < surfaces->size(); i++) {
    ozone_surface_list.at(i).transform = surfaces->at(i).transform;
    ozone_surface_list.at(i).format = GetBufferFormat(surfaces->at(i).format);
    ozone_surface_list.at(i).display_rect = surfaces->at(i).display_rect;
    ozone_surface_list.at(i).crop_rect = surfaces->at(i).uv_rect;
    ozone_surface_list.at(i).quad_rect_in_target_space =
        surfaces->at(i).quad_rect_in_target_space;
    ozone_surface_list.at(i).clip_rect = surfaces->at(i).clip_rect;
    ozone_surface_list.at(i).is_clipped = surfaces->at(i).is_clipped;
    ozone_surface_list.at(i).plane_z_order = surfaces->at(i).plane_z_order;
    ozone_surface_list.at(i).buffer_size =
        surfaces->at(i).resource_size_in_pixels;
  }

  overlay_candidates_->CheckOverlaySupport(&ozone_surface_list);
  DCHECK_EQ(surfaces->size(), ozone_surface_list.size());

  for (size_t i = 0; i < surfaces->size(); i++) {
    surfaces->at(i).overlay_handled = ozone_surface_list.at(i).overlay_handled;
    surfaces->at(i).display_rect = ozone_surface_list.at(i).display_rect;
  }
}

void CompositorOverlayCandidateValidatorOzone::SetSoftwareMirrorMode(
    bool enabled) {
  software_mirror_active_ = enabled;
}

}  // namespace display_compositor
