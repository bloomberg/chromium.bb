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
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // UpdateScreenActor implementation:
  virtual void SetDelegate(UpdateScreenActor::Delegate* screen) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void PrepareToShow() OVERRIDE;
  virtual void ShowManualRebootInfo() OVERRIDE;
  virtual void SetProgress(int progress) OVERRIDE;
  virtual void ShowEstimatedTimeLeft(bool visible) OVERRIDE;
  virtual void SetEstimatedTimeLeft(const base::TimeDelta& time) OVERRIDE;
  virtual void ShowProgressMessage(bool visible) OVERRIDE;
  virtual void SetProgressMessage(ProgressMessage message) OVERRIDE;
  virtual void ShowCurtain(bool visible) OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // NetworkDropdownHandler::Observer implementation:
  virtual void OnConnectToNetworkRequested() OVERRIDE;

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
