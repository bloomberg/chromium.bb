// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_

#include "chrome/browser/chromeos/login/update_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class UpdateScreenHandler : public UpdateScreenActor,
                            public BaseScreenHandler {
 public:
  UpdateScreenHandler();
  virtual ~UpdateScreenHandler();

  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(base::DictionaryValue* localized_strings);
  virtual void Initialize();

  // UpdateScreenActor implementation:
  virtual void SetDelegate(UpdateScreenActor::Delegate* screen) OVERRIDE;
  virtual void Show();
  virtual void Hide();
  virtual void PrepareToShow();
  virtual void ShowManualRebootInfo();
  virtual void SetProgress(int progress);
  virtual void ShowCurtain(bool enable);
  virtual void ShowPreparingUpdatesInfo(bool visible);

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages();

 private:
#if !defined(OFFICIAL_BUILD)
  // Called when user presses Escape to cancel update.
  void HandleUpdateCancel(const base::ListValue* args);
#endif

  UpdateScreenActor::Delegate* screen_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
