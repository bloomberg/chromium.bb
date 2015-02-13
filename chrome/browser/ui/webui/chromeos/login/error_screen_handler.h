// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/network_error_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class NetworkErrorModel;

// A class that handles the WebUI hooks in error screen.
class ErrorScreenHandler : public BaseScreenHandler, public NetworkErrorView {
 public:
  ErrorScreenHandler();
  ~ErrorScreenHandler() override;

  // ErrorView:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  void Bind(NetworkErrorModel& model) override;
  void Unbind() override;
  void ShowScreen(OobeUI::Screen screen) override;

 private:
  bool GetScreenName(OobeUI::Screen screen, std::string* name) const;

  // WebUI message handlers.
  void HandleHideCaptivePortal();

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // Non-owning ptr.
  NetworkErrorModel* model_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  base::WeakPtrFactory<ErrorScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ErrorScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
