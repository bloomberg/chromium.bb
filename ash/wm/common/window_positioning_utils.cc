// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/window_positioning_utils.h"

#include <algorithm>

#include "ash/wm/common/wm_screen_util.h"
#include "ash/wm/common/wm_window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace wm {

namespace {

// Returns the default width of a snapped window.
int GetDefaultSnappedWindowWidth(WmWindow* window) {
  const float kSnappedWidthWorkspaceRatio = 0.5f;

  int work_area_width = GetDisplayWorkAreaBoundsInParent(window).width();
  int min_width = window->GetMinimumSize().width();
  int ideal_width =
      static_cast<int>(work_area_width * kSnappedWidthWorkspaceRatio);
  return std::min(work_area_width, std::max(ideal_width, min_width));
}

}  // namespace

void AdjustBoundsSmallerThan(const gfx::Size& max_size, gfx::Rect* bounds) {
  bounds->set_width(std::min(bounds->width(), max_size.width()));
  bounds->set_height(std::min(bounds->height(), max_size.height()));
}

void AdjustBoundsToEnsureWindowVisibility(const gfx::Rect& visible_area,
                                          int min_width,
                                          int min_height,
                                          gfx::Rect* bounds) {
  AdjustBoundsSmallerThan(visible_area.size(), bounds);

  min_width = std::min(min_width, visible_area.width());
  min_height = std::min(min_height, visible_area.height());

  if (bounds->right() < visible_area.x() + min_width) {
    bounds->set_x(visible_area.x() + min_width - bounds->width());
  } else if (bounds->x() > visible_area.right() - min_width) {
    bounds->set_x(visible_area.right() - min_width);
  }
  if (bounds->bottom() < visible_area.y() + min_height) {
    bounds->set_y(visible_area.y() + min_height - bounds->height());
  } else if (bounds->y() > visible_area.bottom() - min_height) {
    bounds->set_y(visible_area.bottom() - min_height);
  }
  if (bounds->y() < visible_area.y())
    bounds->set_y(visible_area.y());
}

void AdjustBoundsToEnsureMinimumWindowVisibility(const gfx::Rect& visible_area,
                                                 gfx::Rect* bounds) {
  AdjustBoundsToEnsureWindowVisibility(visible_area, kMinimumOnScreenArea,
                                       kMinimumOnScreenArea, bounds);
}

gfx::Rect GetDefaultLeftSnappedWindowBoundsInParent(wm::WmWindow* window) {
  gfx::Rect work_area_in_parent(GetDisplayWorkAreaBoundsInParent(window));
  return gfx::Rect(work_area_in_parent.x(), work_area_in_parent.y(),
                   GetDefaultSnappedWindowWidth(window),
                   work_area_in_parent.height());
}

gfx::Rect GetDefaultRightSnappedWindowBoundsInParent(wm::WmWindow* window) {
  gfx::Rect work_area_in_parent(GetDisplayWorkAreaBoundsInParent(window));
  int width = GetDefaultSnappedWindowWidth(window);
  return gfx::Rect(work_area_in_parent.right() - width, work_area_in_parent.y(),
                   width, work_area_in_parent.height());
}

}  // namespace wm
}  // namespace ash
