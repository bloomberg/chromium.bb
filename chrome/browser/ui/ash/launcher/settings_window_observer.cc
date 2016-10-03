// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/settings_window_observer.h"

#include "ash/common/shelf/shelf_item_types.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/wm/window_properties.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/property_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager.h"
#include "components/strings/grit/components_strings.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/l10n/l10n_util.h"

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

// This class is only used in mash (mus+ash) to rename the Settings window.
class UiWindowSettingsTitleTracker : public ui::WindowTracker {
 public:
  UiWindowSettingsTitleTracker() {}
  ~UiWindowSettingsTitleTracker() override {}

  // ui::WindowTracker:
  void OnWindowSharedPropertyChanged(
      ui::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override {
    if (name == ui::mojom::WindowManager::kWindowTitle_Property) {
      // Name the window "Settings" instead of "Google Chrome - Settings".
      window->SetSharedProperty<base::string16>(
          ui::mojom::WindowManager::kWindowTitle_Property,
          l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UiWindowSettingsTitleTracker);
};

}  // namespace

SettingsWindowObserver::SettingsWindowObserver() {
  if (chrome::IsRunningInMash())
    ui_window_tracker_.reset(new UiWindowSettingsTitleTracker);
  else
    aura_window_tracker_.reset(new AuraWindowSettingsTitleTracker);
  chrome::SettingsWindowManager::GetInstance()->AddObserver(this);
}

SettingsWindowObserver::~SettingsWindowObserver() {
  chrome::SettingsWindowManager::GetInstance()->RemoveObserver(this);
}

void SettingsWindowObserver::OnNewSettingsWindow(Browser* settings_browser) {
  aura::Window* window = settings_browser->window()->GetNativeWindow();
  property_util::SetTitle(window,
                          l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
  property_util::SetIntProperty(window, ash::kShelfItemTypeKey,
                                ash::TYPE_DIALOG);
  property_util::SetIntProperty(window, ash::kShelfIconResourceIdKey,
                                IDR_ASH_SHELF_ICON_SETTINGS);

  if (chrome::IsRunningInMash())
    ui_window_tracker_->Add(aura::GetMusWindow(window));
  else
    aura_window_tracker_->Add(window);
}
