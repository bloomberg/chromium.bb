// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/update_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class UpdateScreenHandler : public UpdateView, public BaseScreenHandler {
 public:
  UpdateScreenHandler();
  ~UpdateScreenHandler() override;

  // UpdateView:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  void Bind(UpdateModel& model) override;
  void Unbind() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  UpdateModel* model_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_SCREEN_HANDLER_H_
