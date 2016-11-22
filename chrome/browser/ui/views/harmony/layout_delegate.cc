// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/layout_delegate.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/ui/views/harmony/harmony_layout_delegate.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/layout/layout_constants.h"

static base::LazyInstance<LayoutDelegate> layout_delegate_ =
    LAZY_INSTANCE_INITIALIZER;

// static
LayoutDelegate* LayoutDelegate::Get() {
  return ui::MaterialDesignController::IsSecondaryUiMaterial()
             ? HarmonyLayoutDelegate::Get()
             : layout_delegate_.Pointer();
}

int LayoutDelegate::GetLayoutDistance(LayoutDistanceType type) const {
  switch (type) {
    case LayoutDistanceType::PANEL_VERT_MARGIN:
      return views::kPanelVertMargin;
    case LayoutDistanceType::RELATED_BUTTON_HORIZONTAL_SPACING:
      return views::kRelatedButtonHSpacing;
    case LayoutDistanceType::RELATED_CONTROL_HORIZONTAL_SPACING:
      return views::kRelatedControlHorizontalSpacing;
    case LayoutDistanceType::RELATED_CONTROL_VERTICAL_SPACING:
      return views::kRelatedControlVerticalSpacing;
    case LayoutDistanceType::UNRELATED_CONTROL_VERTICAL_SPACING:
      return views::kUnrelatedControlVerticalSpacing;
    case LayoutDistanceType::UNRELATED_CONTROL_LARGE_VERTICAL_SPACING:
      return views::kUnrelatedControlLargeVerticalSpacing;
    case LayoutDistanceType::BUTTON_VEDGE_MARGIN_NEW:
      return views::kButtonVEdgeMarginNew;
    case LayoutDistanceType::BUTTON_HEDGE_MARGIN_NEW:
      return views::kButtonHEdgeMarginNew;
  }
  NOTREACHED();
  return 0;
}

views::GridLayout::Alignment LayoutDelegate::GetControlLabelGridAlignment()
    const {
  return views::kControlLabelGridAlignment;
}

bool LayoutDelegate::UseExtraDialogPadding() const {
  return true;
}

bool LayoutDelegate::IsHarmonyMode() const {
  return false;
}

int LayoutDelegate::GetDialogPreferredWidth(DialogWidthType type) const {
  return 0;
}
