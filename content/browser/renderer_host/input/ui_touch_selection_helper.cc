// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/ui_touch_selection_helper.h"

#include "base/logging.h"
#include "cc/output/viewport_selection_bound.h"

namespace content {

namespace {

gfx::SelectionBound::Type ConvertSelectionBoundType(
    cc::SelectionBoundType type) {
  switch (type) {
    case cc::SELECTION_BOUND_LEFT:
      return gfx::SelectionBound::LEFT;
    case cc::SELECTION_BOUND_RIGHT:
      return gfx::SelectionBound::RIGHT;
    case cc::SELECTION_BOUND_CENTER:
      return gfx::SelectionBound::CENTER;
    case cc::SELECTION_BOUND_EMPTY:
      return gfx::SelectionBound::EMPTY;
  }
  NOTREACHED() << "Unknown selection bound type";
  return gfx::SelectionBound::EMPTY;
}

}  // namespace

gfx::SelectionBound ConvertSelectionBound(
    const cc::ViewportSelectionBound& bound) {
  gfx::SelectionBound ui_bound;
  ui_bound.set_type(ConvertSelectionBoundType(bound.type));
  ui_bound.set_visible(bound.visible);
  if (ui_bound.type() != gfx::SelectionBound::EMPTY)
    ui_bound.SetEdge(bound.edge_top, bound.edge_bottom);
  return ui_bound;
}

}  // namespace content
