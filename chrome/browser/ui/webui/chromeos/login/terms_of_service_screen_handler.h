// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// The sole implementation of the TermsOfServiceScreenActor, using WebUI.
class TermsOfServiceScreenHandler : public BaseScreenHandler,
                                    public TermsOfServiceScreenActor {
 public:
  TermsOfServiceScreenHandler();
  virtual ~TermsOfServiceScreenHandler();

  // content::WebUIMessageHandler:
  virtual void RegisterMessages() OVERRIDE;

  // BaseScreenHandler:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;

  // TermsOfServiceScreenActor:
  virtual void SetDelegate(Delegate* screen) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetDomain(const std::string& domain) OVERRIDE;
  virtual void OnLoadError() OVERRIDE;
  virtual void OnLoadSuccess(const std::string& terms_of_service) OVERRIDE;

 private:
  // BaseScreenHandler:
  virtual void Initialize() OVERRIDE;

  // Update the domain name shown in the UI.
  void UpdateDomainInUI();

  // Update the UI to show an error message or the Terms of Service, depending
  // on whether the download of the Terms of Service was successful. Does
  // nothing if the download is still in progress.
  void UpdateTermsOfServiceInUI();

  // Called when the user declines the Terms of Service by clicking the "back"
  // button.
  void HandleBack();

  // Called when the user accepts the Terms of Service by clicking the "accept
  // and continue" button.
  void HandleAccept();

  TermsOfServiceScreenHandler::Delegate* screen_;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_;

  // The domain name whose Terms of Service are being shown.
  std::string domain_;

  // Set to |true| when the download of the Terms of Service fails.
  bool load_error_;

  // Set to the Terms of Service when the download is successful.
  std::string terms_of_service_;

  DISALLOW_COPY_AND_ASSIGN(TermsOfServiceScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
