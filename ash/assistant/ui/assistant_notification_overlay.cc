// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_notification_overlay.h"

#include <memory>

#include "ash/assistant/ui/assistant_notification_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMarginBottomDip = 64;
constexpr int kMarginHorizontalDip = 32;

}  // namespace

AssistantNotificationOverlay::AssistantNotificationOverlay() {
  InitLayout();
}

AssistantNotificationOverlay::~AssistantNotificationOverlay() = default;

const char* AssistantNotificationOverlay::GetClassName() const {
  return "AssistantNotificationOverlay";
}

AssistantOverlay::LayoutParams AssistantNotificationOverlay::GetLayoutParams()
    const {
  using Gravity = AssistantOverlay::LayoutParams::Gravity;
  AssistantOverlay::LayoutParams layout_params;
  layout_params.gravity = Gravity::kBottom | Gravity::kCenterHorizontal;
  layout_params.margins = gfx::Insets(0, kMarginHorizontalDip, kMarginBottomDip,
                                      kMarginHorizontalDip);
  return layout_params;
}

void AssistantNotificationOverlay::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // TODO(dmblack): Wire up to actual notifications.
  AddChildView(new AssistantNotificationView());
}

};  // namespace ash
