// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/ui_touch_selection_helper.h"

#include "base/logging.h"
#include "cc/output/viewport_selection_bound.h"

namespace content {

namespace {

ui::SelectionBound::Type ConvertSelectionBoundType(
    cc::SelectionBoundType type) {
  switch (type) {
    case cc::SELECTION_BOUND_LEFT:
      return ui::SelectionBound::LEFT;
    case cc::SELECTION_BOUND_RIGHT:
      return ui::SelectionBound::RIGHT;
    case cc::SELECTION_BOUND_CENTER:
      return ui::SelectionBound::CENTER;
    case cc::SELECTION_BOUND_EMPTY:
      return ui::SelectionBound::EMPTY;
  }
  NOTREACHED() << "Unknown selection bound type";
  return ui::SelectionBound::EMPTY;
}

}  // namespace

ui::SelectionBound ConvertSelectionBound(
    const cc::ViewportSelectionBound& bound) {
  ui::SelectionBound ui_bound;
  ui_bound.set_type(ConvertSelectionBoundType(bound.type));
  ui_bound.set_visible(bound.visible);
  if (ui_bound.type() != ui::SelectionBound::EMPTY)
    ui_bound.SetEdge(bound.edge_top, bound.edge_bottom);
  return ui_bound;
}

}  // namespace content
