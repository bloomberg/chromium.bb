// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/update_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"

namespace chromeos {

class UpdateScreenHandler : public UpdateScreenActor,
                            public BaseScreenHandler,
                            public NetworkDropdownHandler::Observer {
 public:
  UpdateScreenHandler();
  ~UpdateScreenHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // UpdateScreenActor implementation:
  void SetDelegate(UpdateScreenActor::Delegate* screen) override;
  void Show() override;
  void Hide() override;
  void PrepareToShow() override;
  void ShowManualRebootInfo() override;
  void SetProgress(int progress) override;
  void ShowEstimatedTimeLeft(bool visible) override;
  void SetEstimatedTimeLeft(const base::TimeDelta& time) override;
  void ShowProgressMessage(bool visible) override;
  void SetProgressMessage(ProgressMessage message) override;
  void ShowCurtain(bool visible) override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

 private:
  // NetworkDropdownHandler::Observer implementation:
  void OnConnectToNetworkRequested() override;

#if !defined(OFFICIAL_BUILD)
  // Called when user presses Escape to cancel update.
  void HandleUpdateCancel();
#endif

  UpdateScreenActor::Delegate* screen_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
