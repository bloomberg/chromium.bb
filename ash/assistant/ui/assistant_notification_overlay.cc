// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_notification_overlay.h"

#include <memory>

#include "ash/assistant/ui/assistant_notification_view.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMarginBottomDip = 64;
constexpr int kMarginHorizontalDip = 32;

}  // namespace

AssistantNotificationOverlay::AssistantNotificationOverlay(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();

  // The AssistantViewDelegate outlives the Assistant view hierarchy.
  delegate_->AddNotificationModelObserver(this);
}

AssistantNotificationOverlay::~AssistantNotificationOverlay() {
  delegate_->RemoveNotificationModelObserver(this);
}

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

void AssistantNotificationOverlay::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.parent == this)
    PreferredSizeChanged();
}

void AssistantNotificationOverlay::OnNotificationAdded(
    const AssistantNotification* notification) {
  // TODO(dmblack): Only add views for notifications of the appropriate type.
  AddChildView(new AssistantNotificationView(delegate_, notification));
}

void AssistantNotificationOverlay::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

};  // namespace ash
