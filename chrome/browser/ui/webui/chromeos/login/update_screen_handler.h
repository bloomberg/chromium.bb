// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_

#include "chrome/browser/chromeos/login/update_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace chromeos {

class UpdateScreenHandler : public UpdateScreenActor,
                            public OobeMessageHandler {
 public:
  UpdateScreenHandler();
  virtual ~UpdateScreenHandler();

  // OobeMessageHandler implementation:
  virtual void GetLocalizedStrings(DictionaryValue* localized_strings);
  virtual void Initialize();

  // UpdateScreenActor implementation:
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
  DISALLOW_COPY_AND_ASSIGN(UpdateScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
