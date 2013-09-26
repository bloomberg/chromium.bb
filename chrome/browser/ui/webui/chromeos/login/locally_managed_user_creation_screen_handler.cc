// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/locally_managed_user_creation_screen_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_flow.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/generated_resources.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kJsScreenPath[] = "login.LocallyManagedUserCreationScreen";

// Locally managed user creation screen id.
const char kLocallyManagedUserCreationScreen[] =
    "locally-managed-user-creation";

}  // namespace

namespace chromeos {

LocallyManagedUserCreationScreenHandler::
LocallyManagedUserCreationScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL) {
}

LocallyManagedUserCreationScreenHandler::
    ~LocallyManagedUserCreationScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void LocallyManagedUserCreationScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add(
      "managedUserCreationFlowRetryButtonTitle",
      IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_RETRY_BUTTON_TITLE);
  builder->Add(
      "managedUserCreationFlowCancelButtonTitle",
      IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_CANCEL_BUTTON_TITLE);
  builder->Add(
      "managedUserCreationFlowGotitButtonTitle",
       IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_GOT_IT_BUTTON_TITLE);

  builder->Add("createManagedUserIntroTextTitle",
               IDS_CREATE_LOCALLY_MANAGED_INTRO_TEXT_TITLE);
  builder->Add("createManagedUserIntroAlternateText",
               IDS_CREATE_LOCALLY_MANAGED_INTRO_ALTERNATE_TEXT);
  builder->Add("createManagedUserIntroText1",
               IDS_CREATE_LOCALLY_MANAGED_INTRO_TEXT_1);
  builder->Add("createManagedUserIntroManagerItem1",
               IDS_CREATE_LOCALLY_MANAGED_INTRO_MANAGER_ITEM_1);
  builder->Add("createManagedUserIntroManagerItem2",
               IDS_CREATE_LOCALLY_MANAGED_INTRO_MANAGER_ITEM_2);
  builder->Add("createManagedUserIntroManagerItem3",
               IDS_CREATE_LOCALLY_MANAGED_INTRO_MANAGER_ITEM_3);
  builder->Add("createManagedUserIntroText2",
               IDS_CREATE_LOCALLY_MANAGED_INTRO_TEXT_2);
  builder->AddF("createManagedUserIntroText3",
               IDS_CREATE_LOCALLY_MANAGED_INTRO_TEXT_3,
               UTF8ToUTF16(chrome::kSupervisedUserManagementDisplayURL));

  builder->Add("createManagedUserPickManagerTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PICK_MANAGER_TITLE);
  builder->AddF("createManagedUserPickManagerTitleExplanation",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PICK_MANAGER_EXPLANATION,
               UTF8ToUTF16(chrome::kSupervisedUserManagementDisplayURL));
  builder->Add("createManagedUserManagerPasswordHint",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_MANAGER_PASSWORD_HINT);
  builder->Add("createManagedUserWrongManagerPasswordText",
               IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_PASSWORD_ERROR);

  builder->Add("createManagedUserNameTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ACCOUNT_NAME_TITLE);
  builder->Add("createManagedUserNameExplanation",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ACCOUNT_NAME_EXPLANATION);
  builder->Add("createManagedUserPasswordTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_TITLE);
  builder->Add("createManagedUserPasswordExplanation",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_EXPLANATION);
  builder->Add("createManagedUserPasswordHint",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_HINT);
  builder->Add("createManagedUserPasswordConfirmHint",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_CONFIRM_HINT);
  builder->Add("managedUserCreationFlowProceedButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_CONTINUE_BUTTON_TEXT);
  builder->Add("managedUserCreationFlowStartButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_START_BUTTON_TEXT);
  builder->Add("managedUserCreationFlowPrevButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PREVIOUS_BUTTON_TEXT);
  builder->Add("managedUserCreationFlowNextButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_NEXT_BUTTON_TEXT);
  builder->Add("managedUserCreationFlowHandleErrorButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_HANDLE_ERROR_BUTTON_TEXT);
  builder->Add("createManagedUserPasswordMismatchError",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_MISMATCH_ERROR);

  builder->Add("createManagedUserCreatedText1",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATED_1_TEXT_1);
  builder->Add("createManagedUserCreatedText2",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATED_1_TEXT_2);
  builder->Add("createManagedUserCreatedText3",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATED_1_TEXT_3);

  builder->Add("managementURL", chrome::kSupervisedUserManagementDisplayURL);

  // TODO(antrim) : this is an explicit code duplications with UserImageScreen.
  // It should be removed by issue 251179.
  builder->Add("takePhoto", IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO);
  builder->Add("discardPhoto", IDS_OPTIONS_CHANGE_PICTURE_DISCARD_PHOTO);
  builder->Add("flipPhoto", IDS_OPTIONS_CHANGE_PICTURE_FLIP_PHOTO);
}

void LocallyManagedUserCreationScreenHandler::Initialize() {}

void LocallyManagedUserCreationScreenHandler::RegisterMessages() {
  AddCallback("finishLocalManagedUserCreation",
              &LocallyManagedUserCreationScreenHandler::
                  HandleFinishLocalManagedUserCreation);
  AddCallback("abortLocalManagedUserCreation",
              &LocallyManagedUserCreationScreenHandler::
                  HandleAbortLocalManagedUserCreation);
  AddCallback("checkLocallyManagedUserName",
              &LocallyManagedUserCreationScreenHandler::
                  HandleCheckLocallyManagedUserName);
  AddCallback("authenticateManagerInLocallyManagedUserCreationFlow",
              &LocallyManagedUserCreationScreenHandler::
                  HandleAuthenticateManager);
  AddCallback("specifyLocallyManagedUserCreationFlowUserData",
              &LocallyManagedUserCreationScreenHandler::
                  HandleCreateManagedUser);
  AddCallback("managerSelectedOnLocallyManagedUserCreationFlow",
              &LocallyManagedUserCreationScreenHandler::
                  HandleManagerSelected);

  // TODO(antrim) : this is an explicit code duplications with UserImageScreen.
  // It should be removed by issue 251179.
  AddCallback("supervisedUserGetImages",
              &LocallyManagedUserCreationScreenHandler::
                  HandleGetImages);

  AddCallback("supervisedUserPhotoTaken",
              &LocallyManagedUserCreationScreenHandler::HandlePhotoTaken);
  AddCallback("supervisedUserSelectImage",
              &LocallyManagedUserCreationScreenHandler::HandleSelectImage);
  AddCallback("supervisedUserCheckCameraPresence",
              &LocallyManagedUserCreationScreenHandler::
                  HandleCheckCameraPresence);
}

void LocallyManagedUserCreationScreenHandler::PrepareToShow() {}

void LocallyManagedUserCreationScreenHandler::Show() {
  scoped_ptr<DictionaryValue> data(new base::DictionaryValue());
  scoped_ptr<ListValue> users_list(new base::ListValue());
  const UserList& users = UserManager::Get()->GetUsers();
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);

  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetType() != User::USER_TYPE_REGULAR)
      continue;
    bool is_owner = ((*it)->email() == owner);
    DictionaryValue* user_dict = new DictionaryValue();
    SigninScreenHandler::FillUserDictionary(*it, is_owner, user_dict);
    users_list->Append(user_dict);
  }
  data->Set("managers", users_list.release());
  ShowScreen(OobeUI::kScreenManagedUserCreationFlow, data.get());

  if (!delegate_)
    return;
  delegate_->CheckCameraPresence();
}

void LocallyManagedUserCreationScreenHandler::Hide() {}

void LocallyManagedUserCreationScreenHandler::ShowIntroPage() {
  CallJS("showIntroPage");
}

void LocallyManagedUserCreationScreenHandler::ShowManagerPasswordError() {
  CallJS("showManagerPasswordError");
}

void LocallyManagedUserCreationScreenHandler::ShowStatusMessage(
    bool is_progress,
    const string16& message) {
  if (is_progress)
    CallJS("showProgress", message);
  else
    CallJS("showStatusError", message);
}

void LocallyManagedUserCreationScreenHandler::ShowUsernamePage() {
  CallJS("showUsernamePage");
}

void LocallyManagedUserCreationScreenHandler::ShowTutorialPage() {
  CallJS("showTutorialPage");
}

void LocallyManagedUserCreationScreenHandler::ShowErrorPage(
    const string16& title,
    const string16& message,
    const string16& button_text) {
  CallJS("showErrorPage", title, message, button_text);
}

void LocallyManagedUserCreationScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void LocallyManagedUserCreationScreenHandler::
    HandleFinishLocalManagedUserCreation() {
  delegate_->FinishFlow();
}

void LocallyManagedUserCreationScreenHandler::
    HandleAbortLocalManagedUserCreation() {
  delegate_->AbortFlow();
}

void LocallyManagedUserCreationScreenHandler::HandleManagerSelected(
    const std::string& manager_id) {
  if (!delegate_)
    return;
  WallpaperManager::Get()->SetUserWallpaper(manager_id);
}

void LocallyManagedUserCreationScreenHandler::HandleCheckLocallyManagedUserName(
    const string16& name) {
  if (NULL != UserManager::Get()->
          FindLocallyManagedUser(CollapseWhitespace(name, true))) {
    CallJS("managedUserNameError", name,
           l10n_util::GetStringUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_USERNAME_ALREADY_EXISTS));
  } else if (net::EscapeForHTML(name) != name) {
    CallJS("managedUserNameError", name,
           l10n_util::GetStringUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ILLEGAL_USERNAME));
  } else {
    CallJS("managedUserNameOk", name);
  }
}

void LocallyManagedUserCreationScreenHandler::HandleCreateManagedUser(
    const string16& new_raw_user_name,
    const std::string& new_user_password) {
  if (!delegate_)
    return;
  const string16 new_user_name = CollapseWhitespace(new_raw_user_name, true);
  if (NULL != UserManager::Get()->FindLocallyManagedUser(new_user_name)) {
    CallJS("managedUserNameError", new_user_name,
           l10n_util::GetStringFUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_USERNAME_ALREADY_EXISTS,
               new_user_name));
    return;
  }
  if (net::EscapeForHTML(new_user_name) != new_user_name) {
    CallJS("managedUserNameError", new_user_name,
           l10n_util::GetStringUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ILLEGAL_USERNAME));
    return;
  }

  if (new_user_password.length() == 0) {
    CallJS("showPasswordError",
           l10n_util::GetStringUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_TOO_SHORT));
    return;
  }

  ShowStatusMessage(true /* progress */, l10n_util::GetStringUTF16(
      IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_CREATION_PROGRESS_MESSAGE));

  delegate_->CreateManagedUser(new_user_name, new_user_password);
}

void LocallyManagedUserCreationScreenHandler::HandleAuthenticateManager(
    const std::string& raw_manager_username,
    const std::string& manager_password) {
  const std::string manager_username =
      gaia::SanitizeEmail(raw_manager_username);

  UserFlow* flow = new LocallyManagedUserCreationFlow(manager_username);
  UserManager::Get()->SetUserFlow(manager_username, flow);

  delegate_->AuthenticateManager(manager_username, manager_password);
}

// TODO(antrim) : this is an explicit code duplications with UserImageScreen.
// It should be removed by issue 251179.
void LocallyManagedUserCreationScreenHandler::HandleGetImages() {
  base::ListValue image_urls;
  for (int i = kFirstDefaultImageIndex; i < kDefaultImagesCount; ++i) {
    scoped_ptr<base::DictionaryValue> image_data(new base::DictionaryValue);
    image_data->SetString("url", GetDefaultImageUrl(i));
    image_data->SetString(
        "author", l10n_util::GetStringUTF16(kDefaultImageAuthorIDs[i]));
    image_data->SetString(
        "website", l10n_util::GetStringUTF16(kDefaultImageWebsiteIDs[i]));
    image_data->SetString("title", GetDefaultImageDescription(i));
    image_urls.Append(image_data.release());
  }
  CallJS("setDefaultImages", image_urls);
}

void LocallyManagedUserCreationScreenHandler::HandlePhotoTaken
    (const std::string& image_url) {
  std::string mime_type, charset, raw_data;
  if (!net::DataURL::Parse(GURL(image_url), &mime_type, &charset, &raw_data))
    NOTREACHED();
  DCHECK_EQ("image/png", mime_type);

  if (delegate_)
    delegate_->OnPhotoTaken(raw_data);
}

void LocallyManagedUserCreationScreenHandler::HandleCheckCameraPresence() {
  if (!delegate_)
    return;
  delegate_->CheckCameraPresence();
}

void LocallyManagedUserCreationScreenHandler::HandleSelectImage(
    const std::string& image_url,
    const std::string& image_type) {
  if (delegate_)
    delegate_->OnImageSelected(image_type, image_url);
}

void LocallyManagedUserCreationScreenHandler::SetCameraPresent(bool present) {
  CallJS("setCameraPresent", present);
}

}  // namespace chromeos
