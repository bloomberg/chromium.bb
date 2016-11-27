// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

class CoreOobeActor;

// WebUI implementation of HIDDetectionScreenView.
class HIDDetectionScreenHandler
    : public HIDDetectionView,
      public BaseScreenHandler {
 public:

  explicit HIDDetectionScreenHandler(CoreOobeActor* core_oobe_actor);
  ~HIDDetectionScreenHandler() override;

  // HIDDetectionView implementation:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  void Bind(HIDDetectionModel& model) override;
  void Unbind() override;
  void CheckIsScreenRequired(
      const base::Callback<void(bool)>& on_check_done) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void Initialize() override;

  // Registers the preference for derelict state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // JS messages handlers.
  void HandleOnContinue();

  HIDDetectionModel* model_;

  CoreOobeActor* core_oobe_actor_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(HIDDetectionScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_

