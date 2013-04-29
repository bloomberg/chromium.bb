// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREEN_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREEN_MANAGER_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ScreenManagerHandler : public BaseScreenHandler {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnButtonPressed(const std::string& screen_name,
                                 const std::string& button_id) = 0;
    virtual void OnContextChanged(const std::string& screen_name,
                                  const base::DictionaryValue* diff) = 0;
  };

  ScreenManagerHandler();
  virtual ~ScreenManagerHandler();

  void SetDelegate(Delegate* delegate);

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void HandleButtonPressed(const std::string& screen_name,
                           const std::string& button_id);
  void HandleContextChanged(const std::string& screen_name,
                            const base::DictionaryValue* diff);

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManagerHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREEN_MANAGER_HANDLER_H_
