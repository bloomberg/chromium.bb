// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_screen.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/camera_detector.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/managed/managed_user_creation_controller.h"
#include "chrome/browser/chromeos/login/managed/managed_user_creation_controller_new.h"
#include "chrome/browser/chromeos/login/managed/managed_user_creation_controller_old.h"
#include "chrome/browser/chromeos/login/managed/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/network/network_state.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

// Key for (boolean) value that indicates that user already exists on device.
const char kUserExists[] = "exists";
// Key for  value that indicates why user can not be imported.
const char kUserConflict[] = "conflict";
// User is already imported.
const char kUserConflictImported[] = "imported";
// There is another supervised user with same name.
const char kUserConflictName[] = "name";

const char kUserNeedPassword[] = "needPassword";

const char kAvatarURLKey[] = "avatarurl";
const char kRandomAvatarKey[] = "randomAvatar";
const char kNameOfIntroScreen[] = "intro";
const char kNameOfNewUserParametersScreen[] = "username";

void ConfigureErrorScreen(ErrorScreen* screen,
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  switch (status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      NOTREACHED();
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      screen->SetErrorState(ErrorScreen::ERROR_STATE_OFFLINE,
                            std::string());
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      screen->SetErrorState(ErrorScreen::ERROR_STATE_PORTAL,
                            network ? network->name() : std::string());
      screen->FixCaptivePortal();
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      screen->SetErrorState(ErrorScreen::ERROR_STATE_PROXY,
                            std::string());
      break;
    default:
      NOTREACHED();
      break;
  }
}

} // namespace

LocallyManagedUserCreationScreen::LocallyManagedUserCreationScreen(
    ScreenObserver* observer,
    LocallyManagedUserCreationScreenHandler* actor)
    : WizardScreen(observer),
      weak_factory_(this),
      actor_(actor),
      on_error_screen_(false),
      last_page_(kNameOfIntroScreen),
      image_decoder_(NULL),
      apply_photo_after_decoding_(false),
      selected_image_(0) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

LocallyManagedUserCreationScreen::~LocallyManagedUserCreationScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
  if (image_decoder_.get())
    image_decoder_->set_delegate(NULL);
  NetworkPortalDetector::Get()->RemoveObserver(this);
}

void LocallyManagedUserCreationScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

void LocallyManagedUserCreationScreen::Show() {
  if (actor_) {
    actor_->Show();
    // TODO(antrim) : temorary hack (until upcoming hackaton). Should be
    // removed once we have screens reworked.
    if (on_error_screen_)
      actor_->ShowPage(last_page_);
    else
      actor_->ShowIntroPage();
  }

  if (!on_error_screen_)
    NetworkPortalDetector::Get()->AddAndFireObserver(this);
  on_error_screen_ = false;
}

void LocallyManagedUserCreationScreen::OnPageSelected(const std::string& page) {
  last_page_ = page;
}

void LocallyManagedUserCreationScreen::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state)  {
  if (state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
    get_screen_observer()->HideErrorScreen(this);
  } else {
    on_error_screen_ = true;
    ErrorScreen* screen = get_screen_observer()->GetErrorScreen();
    ConfigureErrorScreen(screen, network, state.status);
    screen->SetUIState(ErrorScreen::UI_STATE_LOCALLY_MANAGED);
    get_screen_observer()->ShowErrorScreen();
  }
}

void LocallyManagedUserCreationScreen::
    ShowManagerInconsistentStateErrorScreen() {
  if (!actor_)
    return;
  actor_->ShowErrorPage(
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_INCONSISTENT_STATE_TITLE),
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_INCONSISTENT_STATE),
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_INCONSISTENT_STATE_BUTTON));
}

void LocallyManagedUserCreationScreen::ShowInitialScreen() {
  if (actor_)
    actor_->ShowIntroPage();
}

void LocallyManagedUserCreationScreen::Hide() {
  if (actor_)
    actor_->Hide();
  if (!on_error_screen_)
    NetworkPortalDetector::Get()->RemoveObserver(this);
}

std::string LocallyManagedUserCreationScreen::GetName() const {
  return WizardController::kLocallyManagedUserCreationScreenName;
}

void LocallyManagedUserCreationScreen::AbortFlow() {
  controller_->CancelCreation();
}

void LocallyManagedUserCreationScreen::FinishFlow() {
  controller_->FinishCreation();
}

void LocallyManagedUserCreationScreen::AuthenticateManager(
    const std::string& manager_id,
    const std::string& manager_password) {
  // Make sure no two controllers exist at the same time.
  controller_.reset();
  SupervisedUserAuthentication* authentication =
      UserManager::Get()->GetSupervisedUserManager()->GetAuthentication();

  if (authentication->GetStableSchema() ==
      SupervisedUserAuthentication::SCHEMA_PLAIN) {
    controller_.reset(new ManagedUserCreationControllerOld(this, manager_id));
  } else {
    controller_.reset(new ManagedUserCreationControllerNew(this, manager_id));
  }

  ExistingUserController::current_controller()->
      Login(UserContext(manager_id,
                        manager_password,
                        std::string()  /* auth_code */));
}

void LocallyManagedUserCreationScreen::CreateManagedUser(
    const base::string16& display_name,
    const std::string& managed_user_password) {
  DCHECK(controller_.get());
  int image;
  if (selected_image_ == User::kExternalImageIndex)
    // TODO(dzhioev): crbug/249660
    image = ManagedUserCreationController::kDummyAvatarIndex;
  else
    image = selected_image_;
  controller_->StartCreation(display_name, managed_user_password, image);
}

void LocallyManagedUserCreationScreen::ImportManagedUser(
    const std::string& user_id) {
  DCHECK(controller_.get());
  DCHECK(existing_users_.get());
  VLOG(1) << "Importing user " << user_id;
  base::DictionaryValue* user_info;
  if (!existing_users_->GetDictionary(user_id, &user_info)) {
    LOG(ERROR) << "Can not import non-existing user " << user_id;
    return;
  }
  base::string16 display_name;
  std::string master_key;
  std::string signature_key;
  std::string encryption_key;
  std::string avatar;
  bool exists;
  int avatar_index = ManagedUserCreationController::kDummyAvatarIndex;
  user_info->GetString(ManagedUserSyncService::kName, &display_name);
  user_info->GetString(ManagedUserSyncService::kMasterKey, &master_key);
  user_info->GetString(ManagedUserSyncService::kPasswordSignatureKey,
                       &signature_key);
  user_info->GetString(ManagedUserSyncService::kPasswordEncryptionKey,
                       &encryption_key);
  user_info->GetString(ManagedUserSyncService::kChromeOsAvatar, &avatar);
  user_info->GetBoolean(kUserExists, &exists);

  // We should not get here with existing user selected, so just display error.
  if (exists) {
    actor_->ShowErrorPage(
        l10n_util::GetStringUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR_TITLE),
        l10n_util::GetStringUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR),
        l10n_util::GetStringUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR_BUTTON));
    return;
  }

  ManagedUserSyncService::GetAvatarIndex(avatar, &avatar_index);

  const base::DictionaryValue* password_data = NULL;
  ManagedUserSharedSettingsService* shared_settings_service =
      ManagedUserSharedSettingsServiceFactory::GetForBrowserContext(
          controller_->GetManagerProfile());
  const base::Value* value = shared_settings_service->GetValue(
      user_id, managed_users::kChromeOSPasswordData);

  bool password_right_here = value && value->GetAsDictionary(&password_data) &&
                             !password_data->empty();

  if (password_right_here) {
    controller_->StartImport(display_name,
                             avatar_index,
                             user_id,
                             master_key,
                             password_data,
                             encryption_key,
                             signature_key);
  } else {
    NOTREACHED() << " Oops, no password";
  }
}

// TODO(antrim): Code duplication with previous method will be removed once
// password sync is implemented.
void LocallyManagedUserCreationScreen::ImportManagedUserWithPassword(
    const std::string& user_id,
    const std::string& password) {
  DCHECK(controller_.get());
  DCHECK(existing_users_.get());
  VLOG(1) << "Importing user " << user_id;
  base::DictionaryValue* user_info;
  if (!existing_users_->GetDictionary(user_id, &user_info)) {
    LOG(ERROR) << "Can not import non-existing user " << user_id;
    return;
  }
  base::string16 display_name;
  std::string master_key;
  std::string avatar;
  bool exists;
  int avatar_index = ManagedUserCreationController::kDummyAvatarIndex;
  user_info->GetString(ManagedUserSyncService::kName, &display_name);
  user_info->GetString(ManagedUserSyncService::kMasterKey, &master_key);
  user_info->GetString(ManagedUserSyncService::kChromeOsAvatar, &avatar);
  user_info->GetBoolean(kUserExists, &exists);

  // We should not get here with existing user selected, so just display error.
  if (exists) {
    actor_->ShowErrorPage(
        l10n_util::GetStringUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR_TITLE),
        l10n_util::GetStringUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR),
        l10n_util::GetStringUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR_BUTTON));
    return;
  }

  ManagedUserSyncService::GetAvatarIndex(avatar, &avatar_index);

  controller_->StartImport(display_name,
                           password,
                           avatar_index,
                           user_id,
                           master_key);
}

void LocallyManagedUserCreationScreen::OnManagerLoginFailure() {
  if (actor_)
    actor_->ShowManagerPasswordError();
}

void LocallyManagedUserCreationScreen::OnManagerFullyAuthenticated(
    Profile* manager_profile) {
  DCHECK(controller_.get());
  // For manager user, move desktop to locked container so that windows created
  // during the user image picker step are below it.
  ash::Shell::GetInstance()->
      desktop_background_controller()->MoveDesktopToLockedContainer();

  controller_->SetManagerProfile(manager_profile);
  if (actor_)
    actor_->ShowUsernamePage();

  last_page_ = kNameOfNewUserParametersScreen;

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kAllowCreateExistingManagedUsers))
    return;

  ManagedUserSyncServiceFactory::GetForProfile(manager_profile)->
      GetManagedUsersAsync(base::Bind(
          &LocallyManagedUserCreationScreen::OnGetManagedUsers,
          weak_factory_.GetWeakPtr()));
}

void LocallyManagedUserCreationScreen::OnManagerCryptohomeAuthenticated() {
  if (actor_) {
    actor_->ShowStatusMessage(true /* progress */, l10n_util::GetStringUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_AUTH_PROGRESS_MESSAGE));
  }
}

void LocallyManagedUserCreationScreen::OnActorDestroyed(
    LocallyManagedUserCreationScreenHandler* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

void LocallyManagedUserCreationScreen::OnCreationError(
    ManagedUserCreationController::ErrorCode code) {
  base::string16 title;
  base::string16 message;
  base::string16 button;
  // TODO(antrim) : find out which errors do we really have.
  // We might reuse some error messages from ordinary user flow.
  switch (code) {
    case ManagedUserCreationController::CRYPTOHOME_NO_MOUNT:
    case ManagedUserCreationController::CRYPTOHOME_FAILED_MOUNT:
    case ManagedUserCreationController::CRYPTOHOME_FAILED_TPM:
      title = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_TPM_ERROR_TITLE);
      message = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_TPM_ERROR);
      button = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_TPM_ERROR_BUTTON);
      break;
    case ManagedUserCreationController::CLOUD_SERVER_ERROR:
    case ManagedUserCreationController::TOKEN_WRITE_FAILED:
      title = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR_TITLE);
      message = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR);
      button = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR_BUTTON);
      break;
    case ManagedUserCreationController::NO_ERROR:
      NOTREACHED();
  }
  if (actor_)
    actor_->ShowErrorPage(title, message, button);
}

void LocallyManagedUserCreationScreen::OnCreationTimeout() {
  if (actor_) {
    actor_->ShowStatusMessage(false /* error */, l10n_util::GetStringUTF16(
        IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_CREATION_TIMEOUT_MESSAGE));
  }
}

void LocallyManagedUserCreationScreen::OnLongCreationWarning() {
  if (actor_) {
    actor_->ShowStatusMessage(true /* progress */, l10n_util::GetStringUTF16(
        IDS_PROFILES_CREATE_MANAGED_JUST_SIGNED_IN));
  }
}

bool LocallyManagedUserCreationScreen::FindUserByDisplayName(
    const base::string16& display_name,
    std::string *out_id) const {
  if (!existing_users_.get())
    return false;
  for (base::DictionaryValue::Iterator it(*existing_users_.get());
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* user_info =
        static_cast<const base::DictionaryValue*>(&it.value());
    base::string16 user_display_name;
    if (user_info->GetString(ManagedUserSyncService::kName,
                             &user_display_name)) {
      if (display_name == user_display_name) {
        if (out_id)
          *out_id = it.key();
        return true;
      }
    }
  }
  return false;
}

// TODO(antrim) : this is an explicit code duplications with UserImageScreen.
// It should be removed by issue 251179.

void LocallyManagedUserCreationScreen::ApplyPicture() {
  std::string user_id = controller_->GetManagedUserId();
  UserManager* user_manager = UserManager::Get();
  UserImageManager* image_manager = user_manager->GetUserImageManager(user_id);
  switch (selected_image_) {
    case User::kExternalImageIndex:
      // Photo decoding may not have been finished yet.
      if (user_photo_.isNull()) {
        apply_photo_after_decoding_ = true;
        return;
      }
      image_manager->SaveUserImage(UserImage::CreateAndEncode(user_photo_));
      break;
    case User::kProfileImageIndex:
      NOTREACHED() << "Supervised users have no profile pictures";
      break;
    default:
      DCHECK(selected_image_ >= 0 && selected_image_ < kDefaultImagesCount);
      image_manager->SaveUserDefaultImageIndex(selected_image_);
      break;
  }
  // Proceed to tutorial.
  actor_->ShowTutorialPage();
}

void LocallyManagedUserCreationScreen::OnCreationSuccess() {
  ApplyPicture();
}

void LocallyManagedUserCreationScreen::OnCameraPresenceCheckDone(
    bool is_camera_present) {
  if (actor_)
    actor_->SetCameraPresent(is_camera_present);
}

void LocallyManagedUserCreationScreen::OnGetManagedUsers(
    const base::DictionaryValue* users) {
  // Copy for passing to WebUI, contains only id, name and avatar URL.
  scoped_ptr<base::ListValue> ui_users(new base::ListValue());
  SupervisedUserManager* supervised_user_manager =
      UserManager::Get()->GetSupervisedUserManager();

  // Stored copy, contains all necessary information.
  existing_users_.reset(new base::DictionaryValue());
  for (base::DictionaryValue::Iterator it(*users); !it.IsAtEnd();
       it.Advance()) {
    // Copy that would be stored in this class.
    base::DictionaryValue* local_copy =
        static_cast<base::DictionaryValue*>(it.value().DeepCopy());
    // Copy that would be passed to WebUI. It has some extra values for
    // displaying, but does not contain sensitive data, such as master password.
    base::DictionaryValue* ui_copy =
        static_cast<base::DictionaryValue*>(new base::DictionaryValue());

    int avatar_index = ManagedUserCreationController::kDummyAvatarIndex;
    std::string chromeos_avatar;
    if (local_copy->GetString(ManagedUserSyncService::kChromeOsAvatar,
                              &chromeos_avatar) &&
        !chromeos_avatar.empty() &&
        ManagedUserSyncService::GetAvatarIndex(
            chromeos_avatar, &avatar_index)) {
      ui_copy->SetString(kAvatarURLKey, GetDefaultImageUrl(avatar_index));
    } else {
      int i = base::RandInt(kFirstDefaultImageIndex, kDefaultImagesCount - 1);
      local_copy->SetString(
          ManagedUserSyncService::kChromeOsAvatar,
          ManagedUserSyncService::BuildAvatarString(i));
      local_copy->SetBoolean(kRandomAvatarKey, true);
      ui_copy->SetString(kAvatarURLKey, GetDefaultImageUrl(i));
    }

    local_copy->SetBoolean(kUserExists, false);
    ui_copy->SetBoolean(kUserExists, false);

    base::string16 display_name;
    local_copy->GetString(ManagedUserSyncService::kName, &display_name);

    if (supervised_user_manager->FindBySyncId(it.key())) {
      local_copy->SetBoolean(kUserExists, true);
      ui_copy->SetBoolean(kUserExists, true);
      local_copy->SetString(kUserConflict, kUserConflictImported);
      ui_copy->SetString(kUserConflict, kUserConflictImported);
    } else if (supervised_user_manager->FindByDisplayName(display_name)) {
      local_copy->SetBoolean(kUserExists, true);
      ui_copy->SetBoolean(kUserExists, true);
      local_copy->SetString(kUserConflict, kUserConflictName);
      ui_copy->SetString(kUserConflict, kUserConflictName);
    }
    ui_copy->SetString(ManagedUserSyncService::kName, display_name);

    std::string signature_key;
    bool has_password =
        local_copy->GetString(ManagedUserSyncService::kPasswordSignatureKey,
                              &signature_key) &&
        !signature_key.empty();

    ui_copy->SetBoolean(kUserNeedPassword, !has_password);
    ui_copy->SetString("id", it.key());

    existing_users_->Set(it.key(), local_copy);
    ui_users->Append(ui_copy);
  }
  actor_->ShowExistingManagedUsers(ui_users.get());
}

void LocallyManagedUserCreationScreen::OnPhotoTaken(
    const std::string& raw_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  user_photo_ = gfx::ImageSkia();
  if (image_decoder_.get())
    image_decoder_->set_delegate(NULL);
  image_decoder_ = new ImageDecoder(this, raw_data,
                                    ImageDecoder::DEFAULT_CODEC);
  scoped_refptr<base::MessageLoopProxy> task_runner =
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI);
  image_decoder_->Start(task_runner);
}

void LocallyManagedUserCreationScreen::OnImageDecoded(
    const ImageDecoder* decoder,
    const SkBitmap& decoded_image) {
  DCHECK_EQ(image_decoder_.get(), decoder);
  user_photo_ = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
  if (apply_photo_after_decoding_)
    ApplyPicture();
}

void LocallyManagedUserCreationScreen::OnDecodeImageFailed(
    const ImageDecoder* decoder) {
  NOTREACHED() << "Failed to decode PNG image from WebUI";
}

void LocallyManagedUserCreationScreen::OnImageSelected(
    const std::string& image_type,
    const std::string& image_url) {
  if (image_url.empty())
    return;
  int user_image_index = User::kInvalidImageIndex;
  if (image_type == "default" &&
      IsDefaultImageUrl(image_url, &user_image_index)) {
    selected_image_ = user_image_index;
  } else if (image_type == "camera") {
    selected_image_ = User::kExternalImageIndex;
  } else {
    NOTREACHED() << "Unexpected image type: " << image_type;
  }
}

void LocallyManagedUserCreationScreen::OnImageAccepted() {
}

}  // namespace chromeos
