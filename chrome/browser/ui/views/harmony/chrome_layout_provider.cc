// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/harmony/harmony_layout_provider.h"
#include "chrome/browser/ui/views/harmony/material_refresh_layout_provider.h"
#include "ui/base/material_design/material_design_controller.h"

namespace {

ChromeLayoutProvider* g_chrome_layout_provider = nullptr;

}  // namespace

ChromeLayoutProvider::ChromeLayoutProvider() {
  DCHECK_EQ(nullptr, g_chrome_layout_provider);
  g_chrome_layout_provider = this;
}

ChromeLayoutProvider::~ChromeLayoutProvider() {
  DCHECK_EQ(this, g_chrome_layout_provider);
  g_chrome_layout_provider = nullptr;
}

// static
ChromeLayoutProvider* ChromeLayoutProvider::Get() {
  // Check to avoid downcasting a base LayoutProvider.
  DCHECK_EQ(g_chrome_layout_provider, views::LayoutProvider::Get());
  return static_cast<ChromeLayoutProvider*>(views::LayoutProvider::Get());
}

// static
std::unique_ptr<views::LayoutProvider>
ChromeLayoutProvider::CreateLayoutProvider() {
  // TODO(pbos): Consolidate HarmonyLayoutProvider into ChromeLayoutProvider as
  // it is no longer active, or wait until Refresh is always on then consolidate
  // all three.
  if (ui::MaterialDesignController::IsRefreshUi())
    return std::make_unique<MaterialRefreshLayoutProvider>();
  return std::make_unique<HarmonyLayoutProvider>();
}

gfx::Insets ChromeLayoutProvider::GetInsetsMetric(int metric) const {
  CHECK(false);
  return gfx::Insets();
}
int ChromeLayoutProvider::GetDistanceMetric(int metric) const {
  CHECK(false);
  return 0;
}

const views::TypographyProvider& ChromeLayoutProvider::GetTypographyProvider()
    const {
  CHECK(false);
  // This is not a data member because then HarmonyLayoutProvider would inherit
  // it, even when it provides its own.
  CR_DEFINE_STATIC_LOCAL(LegacyTypographyProvider, legacy_provider, ());
  return legacy_provider;
}

views::GridLayout::Alignment
ChromeLayoutProvider::GetControlLabelGridAlignment() const {
  CHECK(false);
  return views::GridLayout::TRAILING;
}

bool ChromeLayoutProvider::UseExtraDialogPadding() const {
  CHECK(false);
  return true;
}

bool ChromeLayoutProvider::ShouldShowWindowIcon() const {
  CHECK(false);
  return true;
}
