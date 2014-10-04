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
  virtual ~UpdateScreenHandler();

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  virtual void Initialize() override;

  // UpdateScreenActor implementation:
  virtual void SetDelegate(UpdateScreenActor::Delegate* screen) override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void PrepareToShow() override;
  virtual void ShowManualRebootInfo() override;
  virtual void SetProgress(int progress) override;
  virtual void ShowEstimatedTimeLeft(bool visible) override;
  virtual void SetEstimatedTimeLeft(const base::TimeDelta& time) override;
  virtual void ShowProgressMessage(bool visible) override;
  virtual void SetProgressMessage(ProgressMessage message) override;
  virtual void ShowCurtain(bool visible) override;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() override;

 private:
  // NetworkDropdownHandler::Observer implementation:
  virtual void OnConnectToNetworkRequested() override;

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
