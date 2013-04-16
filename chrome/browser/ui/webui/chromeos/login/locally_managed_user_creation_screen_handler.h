// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
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
    virtual void AuthenticateManager(std::string& manager_id,
                                     std::string& manager_password) = 0;

    // Starts managed user creation flow, with manager identified by
    // |manager_id| and |manager_password|, and locally managed user that would
    // have |display_name| and authenticated by the |managed_user_password|.
    virtual void CreateManagedUser(string16& display_name,
                                   std::string& managed_user_password) = 0;

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

  void ShowSuccessMessage();

  void ShowManagerInconsistentStateErrorPage();
  void ShowErrorPage(const string16& message, bool recoverable);

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // WebUI message handlers.
  void HandleCheckLocallyManagedUserName(const base::ListValue* args);

  void HandleManagerSelected(const base::ListValue* args);

  void HandleFinishLocalManagedUserCreation(const base::ListValue* args);
  void HandleAbortLocalManagedUserCreation(const base::ListValue* args);
  void HandleRetryLocalManagedUserCreation(const base::ListValue* args);

  void HandleAuthenticateManager(const base::ListValue* args);
  void HandleCreateManagedUser(const base::ListValue* args);

  void UpdateText(const std::string& element_id, const string16& text);

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserCreationScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_
