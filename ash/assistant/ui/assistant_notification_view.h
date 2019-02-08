// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_VIEW_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"

namespace views {
class BorderShadowLayerDelegate;
}  // namespace views

namespace ash {

// AssistantNotificationView is the view for a mojom::AssistantNotification
// which appears in Assistant UI. Its parent is AssistantNotificationOverlay.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantNotificationView
    : public views::View {
 public:
  AssistantNotificationView();
  ~AssistantNotificationView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void InitLayout();
  void UpdateBackground();

  // Background/shadow.
  ui::Layer background_layer_;
  std::unique_ptr<views::BorderShadowLayerDelegate> shadow_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_VIEW_H_
