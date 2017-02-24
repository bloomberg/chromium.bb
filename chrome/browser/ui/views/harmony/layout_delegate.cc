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

int LayoutDelegate::GetMetric(Metric metric) const {
  switch (metric) {
    case Metric::BUTTON_HORIZONTAL_PADDING:
      return views::kButtonHorizontalPadding;
    case Metric::BUTTON_MINIMUM_WIDTH:
      return views::kMinimumButtonWidth;
    case Metric::DIALOG_BUTTON_MARGIN:
      return views::kButtonHEdgeMarginNew;
    case Metric::DIALOG_BUTTON_MINIMUM_WIDTH:
      return views::kDialogMinimumButtonWidth;
    case Metric::DIALOG_BUTTON_TOP_SPACING:
      return 0;
    case Metric::DIALOG_CLOSE_BUTTON_MARGIN:
      return views::kCloseButtonMargin;
    case Metric::PANEL_CONTENT_MARGIN:
      return views::kPanelHorizMargin;
    case Metric::RELATED_BUTTON_HORIZONTAL_SPACING:
      return views::kRelatedButtonHSpacing;
    case Metric::RELATED_CONTROL_HORIZONTAL_SPACING:
      return views::kRelatedControlHorizontalSpacing;
    case Metric::RELATED_CONTROL_VERTICAL_SPACING:
      return views::kRelatedControlVerticalSpacing;
    case Metric::RELATED_LABEL_HORIZONTAL_SPACING:
      return views::kItemLabelSpacing;
    case Metric::SUBSECTION_HORIZONTAL_INDENT:
      return views::kCheckboxIndent;
    case Metric::UNRELATED_CONTROL_HORIZONTAL_SPACING:
      return views::kUnrelatedControlHorizontalSpacing;
    case Metric::UNRELATED_CONTROL_HORIZONTAL_SPACING_LARGE:
      return views::kUnrelatedControlLargeHorizontalSpacing;
    case Metric::UNRELATED_CONTROL_VERTICAL_SPACING:
      return views::kUnrelatedControlVerticalSpacing;
    case Metric::UNRELATED_CONTROL_VERTICAL_SPACING_LARGE:
      return views::kUnrelatedControlLargeVerticalSpacing;
  }
  NOTREACHED();
  return 0;
}

views::GridLayout::Alignment LayoutDelegate::GetControlLabelGridAlignment()
    const {
  return views::GridLayout::TRAILING;
}

bool LayoutDelegate::UseExtraDialogPadding() const {
  return true;
}

bool LayoutDelegate::IsHarmonyMode() const {
  return false;
}

int LayoutDelegate::GetDialogPreferredWidth(DialogWidth width) const {
  return 0;
}
