// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/harmony/harmony_layout_provider.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/layout/layout_constants.h"

// static
ChromeLayoutProvider* ChromeLayoutProvider::Get() {
  return static_cast<ChromeLayoutProvider*>(views::LayoutProvider::Get());
}

// static
std::unique_ptr<views::LayoutProvider>
ChromeLayoutProvider::CreateLayoutProvider() {
  return ui::MaterialDesignController::IsSecondaryUiMaterial()
             ? base::MakeUnique<HarmonyLayoutProvider>()
             : base::MakeUnique<ChromeLayoutProvider>();
}

int ChromeLayoutProvider::GetDistanceMetric(int metric) const {
  switch (metric) {
    case DISTANCE_BUTTON_MINIMUM_WIDTH:
      return views::kMinimumButtonWidth;
    case DISTANCE_CONTROL_LIST_VERTICAL:
      return GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_VERTICAL);
    case DISTANCE_DIALOG_BUTTON_MARGIN:
      return views::kButtonHEdgeMarginNew;
    case DISTANCE_DIALOG_BUTTON_TOP:
      return 0;
    case DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL:
      return views::kRelatedControlSmallHorizontalSpacing;
    case DISTANCE_RELATED_CONTROL_VERTICAL_SMALL:
      return views::kRelatedControlSmallVerticalSpacing;
    case DISTANCE_RELATED_LABEL_HORIZONTAL:
      return views::kItemLabelSpacing;
    case DISTANCE_SUBSECTION_HORIZONTAL_INDENT:
      return views::kCheckboxIndent;
    case DISTANCE_PANEL_CONTENT_MARGIN:
      return views::kPanelHorizMargin;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL:
      return views::kUnrelatedControlHorizontalSpacing;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE:
      return views::kUnrelatedControlLargeHorizontalSpacing;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL:
      return views::kUnrelatedControlVerticalSpacing;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE:
      return views::kUnrelatedControlLargeVerticalSpacing;
    default:
      return views::LayoutProvider::GetDistanceMetric(metric);
  }
}

const views::TypographyProvider& ChromeLayoutProvider::GetTypographyProvider()
    const {
  // This is not a data member because then HarmonyLayoutProvider would inherit
  // it, even when it provides its own.
  CR_DEFINE_STATIC_LOCAL(LegacyTypographyProvider, legacy_provider, ());
  return legacy_provider;
}

views::GridLayout::Alignment
ChromeLayoutProvider::GetControlLabelGridAlignment() const {
  return views::GridLayout::TRAILING;
}

bool ChromeLayoutProvider::UseExtraDialogPadding() const {
  return true;
}

bool ChromeLayoutProvider::ShouldShowWindowIcon() const {
  return true;
}

bool ChromeLayoutProvider::IsHarmonyMode() const {
  return false;
}
