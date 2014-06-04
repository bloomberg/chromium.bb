// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// WebUI implementation of AutoEnrollmentCheckScreenActor.
class AutoEnrollmentCheckScreenHandler : public AutoEnrollmentCheckScreenActor,
                                         public BaseScreenHandler {
 public:
  AutoEnrollmentCheckScreenHandler();
  virtual ~AutoEnrollmentCheckScreenHandler();

  // AutoEnrollmentCheckScreenActor implementation:
  virtual void Show() OVERRIDE;
  virtual void SetDelegate(Delegate* delegate) OVERRIDE;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  Delegate* delegate_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentCheckScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_

