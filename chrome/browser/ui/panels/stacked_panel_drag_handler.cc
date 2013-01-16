// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/stacked_panel_drag_handler.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

// static
void StackedPanelDragHandler::HandleDrag(Panel* panel,
                                         const gfx::Point& target_position) {
  DCHECK_EQ(PanelCollection::STACKED, panel->collection()->type());

  // TODO(jianli): to be implemented.
}
