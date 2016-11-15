// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/non_accessible_image_view.h"

#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"

void NonAccessibleImageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->AddStateFlag(ui::AX_STATE_INVISIBLE);
}
