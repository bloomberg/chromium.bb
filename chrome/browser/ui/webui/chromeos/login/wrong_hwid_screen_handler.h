// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// WebUI implementation of WrongHWIDScreenActor.
class WrongHWIDScreenHandler : public WrongHWIDScreenActor,
                               public BaseScreenHandler {
 public:
  WrongHWIDScreenHandler();
  virtual ~WrongHWIDScreenHandler();

  // WrongHWIDScreenActor implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void SetDelegate(Delegate* delegate) override;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  virtual void Initialize() override;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() override;

 private:
  // JS messages handlers.
  void HandleOnSkip();

  Delegate* delegate_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(WrongHWIDScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_

