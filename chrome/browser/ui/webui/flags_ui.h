// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

class PrefRegistrySimple;
class PrefRegistrySyncable;
class Profile;

namespace base {
class RefCountedMemory;
}

class FlagsUI : public content::WebUIController {
 public:
  explicit FlagsUI(content::WebUI* web_ui);
  virtual ~FlagsUI();

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);
  static void RegisterPrefs(PrefRegistrySimple* registry);
#if defined(OS_CHROMEOS)
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);
#endif

 private:
#if defined(OS_CHROMEOS)
  // On ChromeOS verifying if the owner is signed in is async operation and only
  // after finishing it the UI can be properly populated. This function is the
  // callback for whether the owner is signed in. It will respectively pick the
  // proper PrefService for the flags interface.
  void FinishInitialization(
      Profile* profile,
      chromeos::DeviceSettingsService::OwnershipStatus status,
      bool current_user_is_owner);
#endif
  base::WeakPtrFactory<FlagsUI> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FlagsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_
