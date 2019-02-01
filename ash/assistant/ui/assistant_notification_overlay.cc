// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_notification_overlay.h"

#include "ui/gfx/canvas.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMarginBottomDip = 64;
constexpr int kPreferredHeightDip = 60;
constexpr int kPreferredWidthDip = 576;

}  // namespace

AssistantNotificationOverlay::AssistantNotificationOverlay() {
  InitLayout();
}

AssistantNotificationOverlay::~AssistantNotificationOverlay() = default;

const char* AssistantNotificationOverlay::GetClassName() const {
  return "AssistantNotificationOverlay";
}

gfx::Size AssistantNotificationOverlay::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip));
}

int AssistantNotificationOverlay::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

AssistantOverlay::LayoutParams AssistantNotificationOverlay::GetLayoutParams()
    const {
  using Gravity = AssistantOverlay::LayoutParams::Gravity;
  AssistantOverlay::LayoutParams layout_params;
  layout_params.gravity = Gravity::kBottom | Gravity::kCenterHorizontal;
  layout_params.margins = gfx::Insets(0, 0, kMarginBottomDip, 0);
  return layout_params;
}

// TODO(dmblack): Remove when notification views have been implemented.
void AssistantNotificationOverlay::OnPaintBackground(gfx::Canvas* canvas) {
  canvas->DrawColor(0x20000000);
}

void AssistantNotificationOverlay::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

};  // namespace ash
