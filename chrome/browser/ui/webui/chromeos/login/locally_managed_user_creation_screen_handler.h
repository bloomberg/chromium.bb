// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
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

    virtual void AbortFlow() = 0;
    virtual void FinishFlow() = 0;

    virtual void CheckCameraPresence() = 0;
    virtual void OnPhotoTaken(const std::string& raw_data) = 0;
    virtual void OnImageSelected(const std::string& image_url,
                                 const std::string& image_type) = 0;
    virtual void OnImageAccepted() = 0;
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

  // Shows progress or error message close in the button area. |is_progress| is
  // true for progress messages and false for error messages.
  void ShowStatusMessage(bool is_progress, const string16& message);
  void ShowTutorialPage();

  void ShowErrorPage(const string16& title,
                     const string16& message,
                     const string16& button_text);

  void SetCameraPresent(bool enabled);

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

  void HandleGetImages();
  void HandlePhotoTaken(const std::string& image_url);
  void HandleCheckCameraPresence();
  void HandleSelectImage(const std::string& image_url,
                         const std::string& image_type);

  void UpdateText(const std::string& element_id, const string16& text);

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserCreationScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_
