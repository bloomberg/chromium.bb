// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_REQUIRED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_REQUIRED_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/update_required_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class UpdateRequiredScreen;

// Interface for dependency injection between UpdateRequiredScreen and its
// WebUI representation.

class UpdateRequiredView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"update-required"};

  virtual ~UpdateRequiredView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds |screen| to the view.
  virtual void Bind(UpdateRequiredScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;
};

class UpdateRequiredScreenHandler : public UpdateRequiredView,
                                    public BaseScreenHandler {
 public:
  using TView = UpdateRequiredView;

  explicit UpdateRequiredScreenHandler(JSCallsContainer* js_calls_container);
  ~UpdateRequiredScreenHandler() override;

 private:
  void Show() override;
  void Hide() override;
  void Bind(UpdateRequiredScreen* screen) override;
  void Unbind() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  UpdateRequiredScreen* screen_ = nullptr;

  // If true, Initialize() will call Show().
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(UpdateRequiredScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_UPDATE_REQUIRED_SCREEN_HANDLER_H_
