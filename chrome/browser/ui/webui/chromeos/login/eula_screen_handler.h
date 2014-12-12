// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/screens/eula_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/tpm/tpm_password_fetcher.h"
#include "content/public/browser/web_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class CoreOobeActor;
class HelpAppLauncher;

// WebUI implementation of EulaScreenActor. It is used to interact
// with the eula part of the JS page.
class EulaScreenHandler : public EulaView,
                          public BaseScreenHandler,
                          public TpmPasswordFetcherDelegate {
 public:
  explicit EulaScreenHandler(CoreOobeActor* core_oobe_actor);
  virtual ~EulaScreenHandler();

  // EulaView implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void Bind(EulaModel& model) override;
  virtual void Unbind() override;
  virtual void OnPasswordFetched(const std::string& tpm_password) override;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  virtual void DeclareJSCallbacks() override;
  virtual void GetAdditionalParameters(base::DictionaryValue* dict) override;
  virtual void Initialize() override;

 private:
  // JS messages handlers.
  void HandleOnLearnMore();
  void HandleOnChromeCredits();
  void HandleOnChromeOSCredits();
  void HandleOnInstallationSettingsPopupOpened();

  EulaModel* model_;
  CoreOobeActor* core_oobe_actor_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(EulaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
