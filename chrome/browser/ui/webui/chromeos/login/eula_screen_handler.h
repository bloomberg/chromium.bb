// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/eula_screen_actor.h"
#include "chrome/browser/chromeos/login/tpm_password_fetcher.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/browser/webui/web_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

class HelpAppLauncher;

// WebUI implementation of EulaScreenActor. It is used to interact
// with the eula part of the JS page.
class EulaScreenHandler : public EulaScreenActor,
                          public BaseScreenHandler,
                          public TpmPasswordFetcherDelegate {
 public:
  EulaScreenHandler();
  virtual ~EulaScreenHandler();

  // EulaScreenActor implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetDelegate(Delegate* delegate) OVERRIDE;
  virtual void OnPasswordFetched(const std::string& tpm_password) OVERRIDE;

  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // JS messages handlers.
  void HandleOnExit(const base::ListValue* args);
  void HandleOnLearnMore(const base::ListValue* args);
  void HandleOnTpmPopupOpened(const base::ListValue* args);

  Delegate* delegate_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(EulaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
