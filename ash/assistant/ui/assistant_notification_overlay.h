// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_OVERLAY_H_
#define ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_OVERLAY_H_

#include "ash/assistant/ui/assistant_overlay.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace ash {

// AssistantNotificationOverlay is a pseudo-child of AssistantMainView which is
// responsible for parenting in-Assistant notifications.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantNotificationOverlay
    : public AssistantOverlay {
 public:
  AssistantNotificationOverlay();
  ~AssistantNotificationOverlay() override;

  // AssistantOverlay:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  LayoutParams GetLayoutParams() const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

 private:
  void InitLayout();

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationOverlay);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_OVERLAY_H_
