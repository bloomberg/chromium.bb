// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/gfx/native_widget_types.h"

class PrefService;

namespace chromeos {
namespace options {

// ChromeOS internet options page UI handler.
class InternetOptionsHandler : public ::options::OptionsPageUIHandler {
 public:
  InternetOptionsHandler();
  ~InternetOptionsHandler() override;

 private:
  // OptionsPageUIHandler
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;

  // WebUIMessageHandler (from OptionsPageUIHandler)
  void RegisterMessages() override;

  // Callbacks to set network state properties.
  void ShowMorePlanInfoCallback(const base::ListValue* args);
  void SimOperationCallback(const base::ListValue* args);

  // Gets the native window for hosting dialogs, etc.
  gfx::NativeWindow GetNativeWindow() const;

  // Gets the user PrefService associated with the WebUI.
  const PrefService* GetPrefs() const;

  // Additional callbacks for managing networks.
  void AddVPNConnection(const base::ListValue* args);
  void AddNonVPNConnection(const base::ListValue* args);
  void ConfigureNetwork(const base::ListValue* args);

  // Weak pointer factory so we can start connections at a later time
  // without worrying that they will actually try to happen after the lifetime
  // of this object.
  base::WeakPtrFactory<InternetOptionsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InternetOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_
