// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/supervised_user_creation_screen_handler.h"

#include "ash/audio/sounds.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_flow.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/url_constants.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

const char kJsScreenPath[] = "login.LocallyManagedUserCreationScreen";

namespace chromeos {

SupervisedUserCreationScreenHandler::SupervisedUserCreationScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  media::SoundsManager* manager = media::SoundsManager::Get();
  manager->Initialize(SOUND_OBJECT_DELETE,
                      bundle.GetRawDataResource(IDR_SOUND_OBJECT_DELETE_WAV));
  manager->Initialize(SOUND_CAMERA_SNAP,
                      bundle.GetRawDataResource(IDR_SOUND_CAMERA_SNAP_WAV));
}

SupervisedUserCreationScreenHandler::~SupervisedUserCreationScreenHandler() {
  if (delegate_) {
    delegate_->OnActorDestroyed(this);
  }
}

void SupervisedUserCreationScreenHandler::DeclareLocalizedValues(
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
               base::UTF8ToUTF16(chrome::kSupervisedUserManagementDisplayURL));

  builder->Add("createManagedUserPickManagerTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PICK_MANAGER_TITLE);
  builder->AddF("createManagedUserPickManagerTitleExplanation",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PICK_MANAGER_EXPLANATION,
               base::UTF8ToUTF16(chrome::kSupervisedUserManagementDisplayURL));
  builder->Add("createManagedUserManagerPasswordHint",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_MANAGER_PASSWORD_HINT);
  builder->Add("createManagedUserWrongManagerPasswordText",
               IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_PASSWORD_ERROR);

  builder->Add("createManagedUserNameTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ACCOUNT_NAME_TITLE);
  builder->Add("createManagedUserNameAccessibleTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_SETUP_ACCESSIBLE_TITLE);
  builder->Add("createManagedUserNameExplanation",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ACCOUNT_NAME_EXPLANATION);
  builder->Add("createManagedUserNameHint",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ACCOUNT_NAME_HINT);
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

  builder->Add("importExistingSupervisedUserTitle",
               IDS_IMPORT_EXISTING_SUPERVISED_USER_TITLE);
  builder->Add("importExistingSupervisedUserText",
               IDS_IMPORT_EXISTING_SUPERVISED_USER_TEXT);
  builder->Add("managedUserCreationFlowImportButtonTitle",
               IDS_IMPORT_EXISTING_SUPERVISED_USER_OK);
  builder->Add("importSupervisedUserLink",
               IDS_PROFILES_IMPORT_EXISTING_SUPERVISED_USER_LINK);
  builder->Add("createSupervisedUserLink",
               IDS_CREATE_NEW_USER_LINK);
  builder->Add("importBubbleText", IDS_SUPERVISED_USER_IMPORT_BUBBLE_TEXT);
  builder->Add("importUserExists", IDS_SUPERVISED_USER_IMPORT_USER_EXIST);
  builder->Add("importUsernameExists",
               IDS_SUPERVISED_USER_IMPORT_USERNAME_EXIST);

  builder->Add("managementURL", chrome::kSupervisedUserManagementDisplayURL);

  // TODO(antrim) : this is an explicit code duplications with UserImageScreen.
  // It should be removed by issue 251179.
  builder->Add("takePhoto", IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO);
  builder->Add("discardPhoto", IDS_OPTIONS_CHANGE_PICTURE_DISCARD_PHOTO);
  builder->Add("flipPhoto", IDS_OPTIONS_CHANGE_PICTURE_FLIP_PHOTO);
  builder->Add("photoFlippedAccessibleText",
               IDS_OPTIONS_PHOTO_FLIP_ACCESSIBLE_TEXT);
  builder->Add("photoFlippedBackAccessibleText",
               IDS_OPTIONS_PHOTO_FLIPBACK_ACCESSIBLE_TEXT);
  builder->Add("photoCaptureAccessibleText",
               IDS_OPTIONS_PHOTO_CAPTURE_ACCESSIBLE_TEXT);
  builder->Add("photoDiscardAccessibleText",
               IDS_OPTIONS_PHOTO_DISCARD_ACCESSIBLE_TEXT);
}

void SupervisedUserCreationScreenHandler::Initialize() {}

void SupervisedUserCreationScreenHandler::RegisterMessages() {
  AddCallback("finishLocalManagedUserCreation",
              &SupervisedUserCreationScreenHandler::
                  HandleFinishLocalSupervisedUserCreation);
  AddCallback("abortLocalManagedUserCreation",
              &SupervisedUserCreationScreenHandler::
                  HandleAbortLocalSupervisedUserCreation);
  AddCallback("checkLocallyManagedUserName",
              &SupervisedUserCreationScreenHandler::
                  HandleCheckSupervisedUserName);
  AddCallback("authenticateManagerInLocallyManagedUserCreationFlow",
              &SupervisedUserCreationScreenHandler::
                  HandleAuthenticateManager);
  AddCallback("specifyLocallyManagedUserCreationFlowUserData",
              &SupervisedUserCreationScreenHandler::
                  HandleCreateSupervisedUser);
  AddCallback("managerSelectedOnLocallyManagedUserCreationFlow",
              &SupervisedUserCreationScreenHandler::
                  HandleManagerSelected);
  AddCallback("userSelectedForImportInManagedUserCreationFlow",
              &SupervisedUserCreationScreenHandler::
                  HandleImportUserSelected);
  AddCallback("importSupervisedUser",
              &SupervisedUserCreationScreenHandler::
                  HandleImportSupervisedUser);
  AddCallback("importSupervisedUserWithPassword",
              &SupervisedUserCreationScreenHandler::
                  HandleImportSupervisedUserWithPassword);


  // TODO(antrim) : this is an explicit code duplications with UserImageScreen.
  // It should be removed by issue 251179.
  AddCallback("supervisedUserGetImages",
              &SupervisedUserCreationScreenHandler::HandleGetImages);

  AddCallback("supervisedUserPhotoTaken",
              &SupervisedUserCreationScreenHandler::HandlePhotoTaken);
  AddCallback("supervisedUserTakePhoto",
              &SupervisedUserCreationScreenHandler::HandleTakePhoto);
  AddCallback("supervisedUserDiscardPhoto",
              &SupervisedUserCreationScreenHandler::HandleDiscardPhoto);
  AddCallback("supervisedUserSelectImage",
              &SupervisedUserCreationScreenHandler::HandleSelectImage);
  AddCallback("currentSupervisedUserPage",
              &SupervisedUserCreationScreenHandler::
                  HandleCurrentSupervisedUserPage);
}

void SupervisedUserCreationScreenHandler::PrepareToShow() {}

void SupervisedUserCreationScreenHandler::Show() {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  scoped_ptr<base::ListValue> users_list(new base::ListValue());
  const UserList& users = UserManager::Get()->GetUsers();
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);

  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetType() != user_manager::USER_TYPE_REGULAR)
      continue;
    bool is_owner = ((*it)->email() == owner);
    base::DictionaryValue* user_dict = new base::DictionaryValue();
    UserSelectionScreen::FillUserDictionary(
        *it,
        is_owner,
        false /* is_signin_to_add */,
        ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
        user_dict);
    users_list->Append(user_dict);
  }
  data->Set("managers", users_list.release());
  ShowScreen(OobeUI::kScreenSupervisedUserCreationFlow, data.get());

  if (!delegate_)
    return;
}

void SupervisedUserCreationScreenHandler::Hide() {
}

void SupervisedUserCreationScreenHandler::ShowIntroPage() {
  CallJS("showIntroPage");
}

void SupervisedUserCreationScreenHandler::ShowManagerPasswordError() {
  CallJS("showManagerPasswordError");
}

void SupervisedUserCreationScreenHandler::ShowStatusMessage(
    bool is_progress,
    const base::string16& message) {
  if (is_progress)
    CallJS("showProgress", message);
  else
    CallJS("showStatusError", message);
}

void SupervisedUserCreationScreenHandler::ShowUsernamePage() {
  CallJS("showUsernamePage");
}

void SupervisedUserCreationScreenHandler::ShowTutorialPage() {
  CallJS("showTutorialPage");
}

void SupervisedUserCreationScreenHandler::ShowErrorPage(
    const base::string16& title,
    const base::string16& message,
    const base::string16& button_text) {
  CallJS("showErrorPage", title, message, button_text);
}

void SupervisedUserCreationScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void SupervisedUserCreationScreenHandler::
    HandleFinishLocalSupervisedUserCreation() {
  delegate_->FinishFlow();
}

void SupervisedUserCreationScreenHandler::
    HandleAbortLocalSupervisedUserCreation() {
  delegate_->AbortFlow();
}

void SupervisedUserCreationScreenHandler::HandleManagerSelected(
    const std::string& manager_id) {
  if (!delegate_)
    return;
  WallpaperManager::Get()->SetUserWallpaperNow(manager_id);
}

void SupervisedUserCreationScreenHandler::HandleImportUserSelected(
    const std::string& user_id) {
  if (!delegate_)
    return;
}

void SupervisedUserCreationScreenHandler::HandleCheckSupervisedUserName(
    const base::string16& name) {
  std::string user_id;
  if (NULL != UserManager::Get()->GetSupervisedUserManager()->
          FindByDisplayName(base::CollapseWhitespace(name, true))) {
    CallJS("managedUserNameError", name,
           l10n_util::GetStringUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_USERNAME_ALREADY_EXISTS));
  } else if (net::EscapeForHTML(name) != name) {
    CallJS("managedUserNameError", name,
           l10n_util::GetStringUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ILLEGAL_USERNAME));
  } else if (delegate_ && delegate_->FindUserByDisplayName(
                 base::CollapseWhitespace(name, true), &user_id)) {
    CallJS("managedUserSuggestImport", name, user_id);
  } else {
    CallJS("managedUserNameOk", name);
  }
}

void SupervisedUserCreationScreenHandler::HandleCreateSupervisedUser(
    const base::string16& new_raw_user_name,
    const std::string& new_user_password) {
  if (!delegate_)
    return;
  const base::string16 new_user_name =
      base::CollapseWhitespace(new_raw_user_name, true);
  if (NULL != UserManager::Get()->GetSupervisedUserManager()->
          FindByDisplayName(new_user_name)) {
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

  delegate_->CreateSupervisedUser(new_user_name, new_user_password);
}

void SupervisedUserCreationScreenHandler::HandleImportSupervisedUser(
    const std::string& user_id) {
  if (!delegate_)
    return;

  ShowStatusMessage(true /* progress */, l10n_util::GetStringUTF16(
      IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_CREATION_PROGRESS_MESSAGE));

  delegate_->ImportSupervisedUser(user_id);
}

void SupervisedUserCreationScreenHandler::
    HandleImportSupervisedUserWithPassword(
        const std::string& user_id,
        const std::string& password) {
  if (!delegate_)
    return;

  ShowStatusMessage(true /* progress */, l10n_util::GetStringUTF16(
      IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_CREATION_PROGRESS_MESSAGE));

  delegate_->ImportSupervisedUserWithPassword(user_id, password);
}

void SupervisedUserCreationScreenHandler::HandleAuthenticateManager(
    const std::string& raw_manager_username,
    const std::string& manager_password) {
  const std::string manager_username =
      gaia::SanitizeEmail(raw_manager_username);

  UserFlow* flow = new SupervisedUserCreationFlow(manager_username);
  UserManager::Get()->SetUserFlow(manager_username, flow);

  delegate_->AuthenticateManager(manager_username, manager_password);
}

// TODO(antrim) : this is an explicit code duplications with UserImageScreen.
// It should be removed by issue 251179.
void SupervisedUserCreationScreenHandler::HandleGetImages() {
  base::ListValue image_urls;
  for (int i = user_manager::kFirstDefaultImageIndex;
       i < user_manager::kDefaultImagesCount;
       ++i) {
    scoped_ptr<base::DictionaryValue> image_data(new base::DictionaryValue);
    image_data->SetString("url", user_manager::GetDefaultImageUrl(i));
    image_data->SetString(
        "author",
        l10n_util::GetStringUTF16(user_manager::kDefaultImageAuthorIDs[i]));
    image_data->SetString(
        "website",
        l10n_util::GetStringUTF16(user_manager::kDefaultImageWebsiteIDs[i]));
    image_data->SetString("title", user_manager::GetDefaultImageDescription(i));
    image_urls.Append(image_data.release());
  }
  CallJS("setDefaultImages", image_urls);
}

void SupervisedUserCreationScreenHandler::HandlePhotoTaken
    (const std::string& image_url) {
  std::string mime_type, charset, raw_data;
  if (!net::DataURL::Parse(GURL(image_url), &mime_type, &charset, &raw_data))
    NOTREACHED();
  DCHECK_EQ("image/png", mime_type);

  if (delegate_)
    delegate_->OnPhotoTaken(raw_data);
}

void SupervisedUserCreationScreenHandler::HandleTakePhoto() {
  ash::PlaySystemSoundIfSpokenFeedback(SOUND_CAMERA_SNAP);
}

void SupervisedUserCreationScreenHandler::HandleDiscardPhoto() {
  ash::PlaySystemSoundIfSpokenFeedback(SOUND_OBJECT_DELETE);
}

void SupervisedUserCreationScreenHandler::HandleSelectImage(
    const std::string& image_url,
    const std::string& image_type) {
  if (delegate_)
    delegate_->OnImageSelected(image_type, image_url);
}

void SupervisedUserCreationScreenHandler::HandleCurrentSupervisedUserPage(
    const std::string& page) {
  if (delegate_)
    delegate_->OnPageSelected(page);
}

void SupervisedUserCreationScreenHandler::ShowPage(
    const std::string& page) {
  CallJS("showPage", page);
}

void SupervisedUserCreationScreenHandler::SetCameraPresent(bool present) {
  CallJS("setCameraPresent", present);
}

void SupervisedUserCreationScreenHandler::ShowExistingSupervisedUsers(
    const base::ListValue* users) {
  CallJS("setExistingManagedUsers", *users);
}

}  // namespace chromeos
