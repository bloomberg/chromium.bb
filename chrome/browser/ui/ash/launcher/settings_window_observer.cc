// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/settings_window_observer.h"

#include "ash/common/shelf/shelf_item_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/grit/ash_resources.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/strings/grit/components_strings.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// This class is only used in classic ash to rename the Settings window.
class AuraWindowSettingsTitleTracker : public aura::WindowTracker {
 public:
  AuraWindowSettingsTitleTracker() {}
  ~AuraWindowSettingsTitleTracker() override {}

  // aura::WindowTracker:
  void OnWindowTitleChanged(aura::Window* window) override {
    // Name the window "Settings" instead of "Google Chrome - Settings".
    window->SetTitle(l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AuraWindowSettingsTitleTracker);
};

}  // namespace

SettingsWindowObserver::SettingsWindowObserver() {
  aura_window_tracker_ = base::MakeUnique<AuraWindowSettingsTitleTracker>();
  chrome::SettingsWindowManager::GetInstance()->AddObserver(this);
}

SettingsWindowObserver::~SettingsWindowObserver() {
  chrome::SettingsWindowManager::GetInstance()->RemoveObserver(this);
}

void SettingsWindowObserver::OnNewSettingsWindow(Browser* settings_browser) {
  aura::Window* window = settings_browser->window()->GetNativeWindow();
  window->SetTitle(l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_DIALOG);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* icon =
      base::FeatureList::IsEnabled(features::kMaterialDesignSettings)
          ? rb.GetImageSkiaNamed(IDR_ASH_SHELF_ICON_MD_SETTINGS)
          : rb.GetImageSkiaNamed(IDR_ASH_SHELF_ICON_SETTINGS);
  // The new gfx::ImageSkia instance is owned by the window itself.
  window->SetProperty(aura::client::kWindowIconKey, new gfx::ImageSkia(*icon));
  aura_window_tracker_->Add(window);
}
