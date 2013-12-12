// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHARGER_REPLACEMENT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHARGER_REPLACEMENT_HANDLER_H_

#include "base/compiler_specific.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/native_widget_types.h"

class PrefRegistrySimple;

namespace chromeos {

class ChargerReplacementDialog;

// Handler for spring charger replacement web ui.
class ChargerReplacementHandler : public content::WebUIMessageHandler {
 public:
  enum SpringChargerStatus {
    CHARGER_UNKNOWN,
    CONFIRM_SAFE_CHARGER,
    CONFIRM_NOT_ORDER_NEW_CHARGER,
    CONFIRM_NEW_CHARGER_ORDERED_ONLINE,
    CONFIRM_ORDER_NEW_CHARGER_BY_PHONE,
    USE_BAD_CHARGER_AFTER_ORDER_ONLINE,
    USE_BAD_CHARGER_AFTER_ORDER_BY_PHONE,
  };

  explicit ChargerReplacementHandler(ChargerReplacementDialog* dialog);
  virtual ~ChargerReplacementHandler();

  // Registers preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Sets/Gets charger status pref.
  static SpringChargerStatus GetChargerStatusPref();
  static void SetChargerStatusPref(SpringChargerStatus status);

  // WebUIMessageHandler overrides:
  virtual void RegisterMessages() OVERRIDE;

  // Gets localized strings for web ui.
  static void GetLocalizedValues(base::DictionaryValue* localized_strings);

  void set_charger_window(gfx::NativeWindow window) {
    charger_window_ = window;
  }

 private:
  void ConfirmSafeCharger(const base::ListValue* args);
  void ConfirmNotOrderNewCharger(const base::ListValue* args);
  void ConfirmChargerOrderedOnline(const base::ListValue* args);
  void ConfirmChargerOrderByPhone(const base::ListValue* args);
  void ConfirmStillUseBadCharger(const base::ListValue* args);
  void ShowLink(const base::ListValue* args);

  gfx::NativeWindow charger_window_;
  ChargerReplacementDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(ChargerReplacementHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHARGER_REPLACEMENT_HANDLER_H_
