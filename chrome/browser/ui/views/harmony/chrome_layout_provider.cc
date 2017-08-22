// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/harmony/harmony_layout_provider.h"
#include "ui/base/material_design/material_design_controller.h"

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

gfx::Insets ChromeLayoutProvider::GetInsetsMetric(int metric) const {
  switch (metric) {
    case ChromeInsetsMetric::INSETS_TOAST:
      return gfx::Insets(0, 8);
    default:
      return views::LayoutProvider::GetInsetsMetric(metric);
  }
}

int ChromeLayoutProvider::GetDistanceMetric(int metric) const {
  switch (metric) {
    case DISTANCE_BUTTON_MINIMUM_WIDTH:
      return 48;
    case DISTANCE_CONTROL_LIST_VERTICAL:
      return GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
    case DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL:
      return 8;
    case DISTANCE_RELATED_CONTROL_VERTICAL_SMALL:
      return 4;
    case DISTANCE_RELATED_LABEL_HORIZONTAL_LIST:
      return 8;
    case DISTANCE_SUBSECTION_HORIZONTAL_INDENT:
      return 10;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL:
      return 12;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE:
      return 20;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE:
      return 30;
    case DISTANCE_TOAST_CONTROL_VERTICAL:
      return 8;
    case DISTANCE_TOAST_LABEL_VERTICAL:
      return 12;
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
