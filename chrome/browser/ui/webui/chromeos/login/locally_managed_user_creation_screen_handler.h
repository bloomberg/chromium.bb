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
class ListValue;
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

    // Starts managed user creation flow, with supervised user that would have
    // |display_name| and authenticated by the |managed_user_password|.
    virtual void CreateManagedUser(
        const base::string16& display_name,
        const std::string& managed_user_password) = 0;

    // Look up if user with name |display_name| already exist and can be
    // imported. Returns user ID in |out_id|. Returns true if user was found,
    // false otherwise.
    virtual bool FindUserByDisplayName(const base::string16& display_name,
                                       std::string *out_id) const = 0;

    // Starts managed user import flow for user identified with |user_id|.
    virtual void ImportManagedUser(const std::string& user_id) = 0;
    // Starts managed user import flow for user identified with |user_id| and
    // additional |password|.
    virtual void ImportManagedUserWithPassword(const std::string& user_id,
                                               const std::string& password) = 0;

    virtual void AbortFlow() = 0;
    virtual void FinishFlow() = 0;

    virtual void OnPhotoTaken(const std::string& raw_data) = 0;
    virtual void OnImageSelected(const std::string& image_url,
                                 const std::string& image_type) = 0;
    virtual void OnImageAccepted() = 0;
    virtual void OnPageSelected(const std::string& page) = 0;
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
  void ShowStatusMessage(bool is_progress, const base::string16& message);
  void ShowTutorialPage();

  void ShowErrorPage(const base::string16& title,
                     const base::string16& message,
                     const base::string16& button_text);

  // Navigates to specified page.
  void ShowPage(const std::string& page);

  void SetCameraPresent(bool enabled);

  void ShowExistingManagedUsers(const base::ListValue* users);

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // WebUI message handlers.
  void HandleCheckLocallyManagedUserName(const base::string16& name);

  void HandleManagerSelected(const std::string& manager_id);
  void HandleImportUserSelected(const std::string& user_id);

  void HandleFinishLocalManagedUserCreation();
  void HandleAbortLocalManagedUserCreation();
  void HandleRetryLocalManagedUserCreation(const base::ListValue* args);
  void HandleCurrentSupervisedUserPage(const std::string& current_page);

  void HandleAuthenticateManager(const std::string& raw_manager_username,
                                 const std::string& manager_password);
  void HandleCreateManagedUser(const base::string16& new_raw_user_name,
                               const std::string& new_user_password);
  void HandleImportSupervisedUser(const std::string& user_id);
  void HandleImportSupervisedUserWithPassword(const std::string& user_id,
                                              const std::string& password);

  void HandleGetImages();
  void HandlePhotoTaken(const std::string& image_url);
  void HandleTakePhoto();
  void HandleDiscardPhoto();
  void HandleSelectImage(const std::string& image_url,
                         const std::string& image_type);

  void UpdateText(const std::string& element_id, const base::string16& text);

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserCreationScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCALLY_MANAGED_USER_CREATION_SCREEN_HANDLER_H_
