// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class LocallyManagedUserCreationScreenHandler : public BaseScreenHandler {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when screen is exited.
    virtual void OnExit() = 0;

    // This method is called, when actor is being destroyed. Note, if Delegate
    // is destroyed earlier then it has to call SetDelegate(NULL).
    virtual void OnActorDestroyed(
        LocallyManagedUserCreationScreenHandler* actor) = 0;

    // Starts managed user creation flow, with manager identified by
    // |manager_id| and |manager_password|.
    virtual void AuthenticateManager(const std::string& manager_id,
                                     const std::string& manager_password) = 0;

    // Starts managed user creation flow, with manager identified by
    // |manager_id| and |manager_password|, and locally managed user that would
    // have |display_name| and authenticated by the |managed_user_password|.
    virtual void CreateManagedUser(
        const string16& display_name,
        const std::string& managed_user_password) = 0;

    // Starts picture selection for created managed user.
    virtual void SelectPicture() = 0;

    virtual void AbortFlow() = 0;
    virtual void FinishFlow() = 0;
  };

  LocallyManagedUserCreationScreenHandler();
  virtual ~LocallyManagedUserCreationScreenHandler();

  virtual void PrepareToShow();
  virtual void Show();
  virtual void Hide();
  virtual void SetDelegate(Delegate* delegate);

  void ShowManagerPasswordError();

  void ShowIntroPage();
  void ShowManagerSelectionPage();
  void ShowUsernamePage();
  void ShowProgressPage();
  void ShowTutorialPage();

  void ShowManagerInconsistentStateErrorPage();
  void ShowErrorPage(const string16& message, bool recoverable);

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // WebUI message handlers.
  void HandleCheckLocallyManagedUserName(const string16& name);

  void HandleManagerSelected(const std::string& manager_id);

  void HandleFinishLocalManagedUserCreation();
  void HandleAbortLocalManagedUserCreation();
  void HandleRetryLocalManagedUserCreation(const base::ListValue* args);

  void HandleAuthenticateManager(const std::string& raw_manager_username,
                                 const std::string& manager_password);
  void HandleCreateManagedUser(const string16& new_raw_user_name,
                               const std::string& new_user_password);

  void UpdateText(const std::string& element_id, const string16& text);

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserCreationScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_
