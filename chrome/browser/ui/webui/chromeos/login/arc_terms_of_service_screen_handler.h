// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_TERMS_OF_SERVICE_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/settings/timezone_settings.h"

namespace arc {
class ArcOptInPreferenceHandler;
}

namespace chromeos {

// The sole implementation of the ArcTermsOfServiceScreenActor, using WebUI.
class ArcTermsOfServiceScreenHandler :
    public BaseScreenHandler,
    public ArcTermsOfServiceScreenActor,
    public arc::ArcOptInPreferenceHandlerObserver,
    public system::TimezoneSettings::Observer {
 public:
  ArcTermsOfServiceScreenHandler();
  ~ArcTermsOfServiceScreenHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // ArcTermsOfServiceScreenActor:
  void SetDelegate(Delegate* screen) override;
  void Show() override;
  void Hide() override;

  // system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

 private:
  // BaseScreenHandler:
  void Initialize() override;

  void DoShow();
  void HandleSkip();
  void HandleAccept(bool enable_backup_restore,
                    bool enable_location_services);
  void UpdateTimeZone();

  // arc::ArcOptInPreferenceHandlerObserver:
  void OnMetricsModeChanged(bool enabled, bool managed) override;
  void OnBackupAndRestoreModeChanged(bool enabled, bool managed) override;
  void OnLocationServicesModeChanged(bool enabled, bool managed) override;

  Delegate* screen_ = nullptr;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;

  // To prevent redundant updates, keep last country code used for update.
  std::string last_applied_contry_code_;

  std::unique_ptr<arc::ArcOptInPreferenceHandler> pref_handler_;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
