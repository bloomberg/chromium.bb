// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_screen.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/camera_detector.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_controller.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_controller_new.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_flow.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/network_state.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "content/public/browser/browser_thread.h"
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
      screen->SetErrorState(NetworkError::ERROR_STATE_OFFLINE, std::string());
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      screen->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                            network ? network->name() : std::string());
      screen->FixCaptivePortal();
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      screen->SetErrorState(NetworkError::ERROR_STATE_PROXY, std::string());
      break;
    default:
      NOTREACHED();
      break;
  }
}

} // namespace

// static
SupervisedUserCreationScreen* SupervisedUserCreationScreen::Get(
    ScreenManager* manager) {
  return static_cast<SupervisedUserCreationScreen*>(
      manager->GetScreen(WizardController::kSupervisedUserCreationScreenName));
}

SupervisedUserCreationScreen::SupervisedUserCreationScreen(
    BaseScreenDelegate* base_screen_delegate,
    SupervisedUserCreationScreenHandler* actor)
    : BaseScreen(base_screen_delegate),
      actor_(actor),
      on_error_screen_(false),
      manager_signin_in_progress_(false),
      last_page_(kNameOfIntroScreen),
      sync_service_(NULL),
      apply_photo_after_decoding_(false),
      selected_image_(0),
      histogram_helper_(new ErrorScreensHistogramHelper("Supervised")),
      weak_factory_(this) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

SupervisedUserCreationScreen::~SupervisedUserCreationScreen() {
  CameraPresenceNotifier::GetInstance()->RemoveObserver(this);
  if (sync_service_)
    sync_service_->RemoveObserver(this);
  if (actor_)
    actor_->SetDelegate(NULL);
  network_portal_detector::GetInstance()->RemoveObserver(this);
}

void SupervisedUserCreationScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

void SupervisedUserCreationScreen::Show() {
  CameraPresenceNotifier::GetInstance()->AddObserver(this);
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
    network_portal_detector::GetInstance()->AddAndFireObserver(this);
  on_error_screen_ = false;
  histogram_helper_->OnScreenShow();
}

void SupervisedUserCreationScreen::OnPageSelected(const std::string& page) {
  last_page_ = page;
}

void SupervisedUserCreationScreen::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state)  {
  if (state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
    get_base_screen_delegate()->HideErrorScreen(this);
    histogram_helper_->OnErrorHide();
  } else if (state.status !=
             NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN) {
    on_error_screen_ = true;
    ErrorScreen* screen = get_base_screen_delegate()->GetErrorScreen();
    ConfigureErrorScreen(screen, network, state.status);
    screen->SetUIState(NetworkError::UI_STATE_SUPERVISED);
    get_base_screen_delegate()->ShowErrorScreen();
    histogram_helper_->OnErrorShow(screen->GetErrorState());
  }
}

void SupervisedUserCreationScreen::ShowManagerInconsistentStateErrorScreen() {
  manager_signin_in_progress_ = false;
  if (!actor_)
    return;
  actor_->ShowErrorPage(
      l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_MANAGER_INCONSISTENT_STATE_TITLE),
      l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_MANAGER_INCONSISTENT_STATE),
      l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_MANAGER_INCONSISTENT_STATE_BUTTON));
}

void SupervisedUserCreationScreen::ShowInitialScreen() {
  if (actor_)
    actor_->ShowIntroPage();
}

void SupervisedUserCreationScreen::Hide() {
  CameraPresenceNotifier::GetInstance()->RemoveObserver(this);
  if (actor_)
    actor_->Hide();
  if (!on_error_screen_)
    network_portal_detector::GetInstance()->RemoveObserver(this);
}

std::string SupervisedUserCreationScreen::GetName() const {
  return WizardController::kSupervisedUserCreationScreenName;
}

void SupervisedUserCreationScreen::AbortFlow() {
  DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->NotifySupervisedUserCreationFinished();
  controller_->CancelCreation();
}

void SupervisedUserCreationScreen::FinishFlow() {
  DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->NotifySupervisedUserCreationFinished();
  controller_->FinishCreation();
}

void SupervisedUserCreationScreen::HideFlow() {
  Hide();
}

void SupervisedUserCreationScreen::AuthenticateManager(
    const AccountId& manager_id,
    const std::string& manager_password) {
  if (manager_signin_in_progress_)
    return;
  manager_signin_in_progress_ = true;

  UserFlow* flow = new SupervisedUserCreationFlow(manager_id);
  ChromeUserManager::Get()->SetUserFlow(manager_id, flow);

  // Make sure no two controllers exist at the same time.
  controller_.reset();

  controller_.reset(new SupervisedUserCreationControllerNew(this, manager_id));

  UserContext user_context(manager_id);
  user_context.SetKey(Key(manager_password));
  ExistingUserController::current_controller()->Login(user_context,
                                                      SigninSpecifics());
}

void SupervisedUserCreationScreen::CreateSupervisedUser(
    const base::string16& display_name,
    const std::string& supervised_user_password) {
  DCHECK(controller_.get());
  int image;
  if (selected_image_ == user_manager::User::USER_IMAGE_EXTERNAL)
    // TODO(dzhioev): crbug/249660
    image = SupervisedUserCreationController::kDummyAvatarIndex;
  else
    image = selected_image_;
  controller_->StartCreation(display_name, supervised_user_password, image);
}

void SupervisedUserCreationScreen::ImportSupervisedUser(
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
  int avatar_index = SupervisedUserCreationController::kDummyAvatarIndex;
  user_info->GetString(SupervisedUserSyncService::kName, &display_name);
  user_info->GetString(SupervisedUserSyncService::kMasterKey, &master_key);
  user_info->GetString(SupervisedUserSyncService::kPasswordSignatureKey,
                       &signature_key);
  user_info->GetString(SupervisedUserSyncService::kPasswordEncryptionKey,
                       &encryption_key);
  user_info->GetString(SupervisedUserSyncService::kChromeOsAvatar, &avatar);
  user_info->GetBoolean(kUserExists, &exists);

  // We should not get here with existing user selected, so just display error.
  if (exists) {
    actor_->ShowErrorPage(
        l10n_util::GetStringUTF16(
            IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR_TITLE),
        l10n_util::GetStringUTF16(
            IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR),
        l10n_util::GetStringUTF16(
            IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR_BUTTON));
    return;
  }

  SupervisedUserSyncService::GetAvatarIndex(avatar, &avatar_index);

  const base::DictionaryValue* password_data = NULL;
  SupervisedUserSharedSettingsService* shared_settings_service =
      SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
          controller_->GetManagerProfile());
  const base::Value* value = shared_settings_service->GetValue(
      user_id, supervised_users::kChromeOSPasswordData);

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
void SupervisedUserCreationScreen::ImportSupervisedUserWithPassword(
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
  int avatar_index = SupervisedUserCreationController::kDummyAvatarIndex;
  user_info->GetString(SupervisedUserSyncService::kName, &display_name);
  user_info->GetString(SupervisedUserSyncService::kMasterKey, &master_key);
  user_info->GetString(SupervisedUserSyncService::kChromeOsAvatar, &avatar);
  user_info->GetBoolean(kUserExists, &exists);

  // We should not get here with existing user selected, so just display error.
  if (exists) {
    actor_->ShowErrorPage(
        l10n_util::GetStringUTF16(
            IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR_TITLE),
        l10n_util::GetStringUTF16(
            IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR),
        l10n_util::GetStringUTF16(
            IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR_BUTTON));
    return;
  }

  SupervisedUserSyncService::GetAvatarIndex(avatar, &avatar_index);

  controller_->StartImport(display_name,
                           password,
                           avatar_index,
                           user_id,
                           master_key);
}

void SupervisedUserCreationScreen::OnManagerLoginFailure() {
  manager_signin_in_progress_ = false;
  if (actor_)
    actor_->ShowManagerPasswordError();
}

void SupervisedUserCreationScreen::OnManagerFullyAuthenticated(
    Profile* manager_profile) {
  DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->NotifySupervisedUserCreationStarted();
  manager_signin_in_progress_ = false;
  DCHECK(controller_.get());
  // For manager user, move desktop to locked container so that windows created
  // during the user image picker step are below it.
  ash::Shell::GetInstance()->
      desktop_background_controller()->MoveDesktopToLockedContainer();

  // Hide the status area and the control bar, since they will show up at the
  // logged in users's preferred location, which could be on the left or right
  // side of the screen.
  LoginDisplayHost* default_host = LoginDisplayHost::default_host();
  default_host->SetStatusAreaVisible(false);
  default_host->GetOobeUI()->GetCoreOobeActor()->ShowControlBar(false);

  controller_->SetManagerProfile(manager_profile);
  if (actor_)
    actor_->ShowUsernamePage();

  last_page_ = kNameOfNewUserParametersScreen;
  CHECK(!sync_service_);
  sync_service_ = SupervisedUserSyncServiceFactory::GetForProfile(
      manager_profile);
  sync_service_->AddObserver(this);
  OnSupervisedUsersChanged();
}

void SupervisedUserCreationScreen::OnSupervisedUsersChanged() {
  CHECK(sync_service_);
  sync_service_->GetSupervisedUsersAsync(
      base::Bind(&SupervisedUserCreationScreen::OnGetSupervisedUsers,
                 weak_factory_.GetWeakPtr()));
}

void SupervisedUserCreationScreen::OnManagerCryptohomeAuthenticated() {
  if (actor_) {
    actor_->ShowStatusMessage(true /* progress */, l10n_util::GetStringUTF16(
        IDS_CREATE_SUPERVISED_USER_CREATION_AUTH_PROGRESS_MESSAGE));
  }
}

void SupervisedUserCreationScreen::OnActorDestroyed(
    SupervisedUserCreationScreenHandler* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

void SupervisedUserCreationScreen::OnCreationError(
    SupervisedUserCreationController::ErrorCode code) {
  LOG(ERROR) << "Supervised user creation failure, code: " << code;

  base::string16 title;
  base::string16 message;
  base::string16 button;
  // TODO(antrim) : find out which errors do we really have.
  // We might reuse some error messages from ordinary user flow.
  switch (code) {
    case SupervisedUserCreationController::CRYPTOHOME_NO_MOUNT:
    case SupervisedUserCreationController::CRYPTOHOME_FAILED_MOUNT:
    case SupervisedUserCreationController::CRYPTOHOME_FAILED_TPM:
      title = l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_TPM_ERROR_TITLE);
      message = l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_TPM_ERROR);
      button = l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_TPM_ERROR_BUTTON);
      break;
    case SupervisedUserCreationController::CLOUD_SERVER_ERROR:
    case SupervisedUserCreationController::TOKEN_WRITE_FAILED:
      title = l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR_TITLE);
      message = l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR);
      button = l10n_util::GetStringUTF16(
          IDS_CREATE_SUPERVISED_USER_GENERIC_ERROR_BUTTON);
      break;
    case SupervisedUserCreationController::NO_ERROR:
      NOTREACHED();
  }
  if (actor_)
    actor_->ShowErrorPage(title, message, button);
}

void SupervisedUserCreationScreen::OnCreationTimeout() {
  if (actor_) {
    actor_->ShowStatusMessage(false /* error */, l10n_util::GetStringUTF16(
        IDS_CREATE_SUPERVISED_USER_CREATION_CREATION_TIMEOUT_MESSAGE));
  }
}

void SupervisedUserCreationScreen::OnLongCreationWarning() {
  if (actor_) {
    actor_->ShowStatusMessage(true /* progress */, l10n_util::GetStringUTF16(
        IDS_PROFILES_CREATE_SUPERVISED_JUST_SIGNED_IN));
  }
}

bool SupervisedUserCreationScreen::FindUserByDisplayName(
    const base::string16& display_name,
    std::string *out_id) const {
  if (!existing_users_.get())
    return false;
  for (base::DictionaryValue::Iterator it(*existing_users_.get());
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* user_info =
        static_cast<const base::DictionaryValue*>(&it.value());
    base::string16 user_display_name;
    if (user_info->GetString(SupervisedUserSyncService::kName,
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

void SupervisedUserCreationScreen::ApplyPicture() {
  std::string user_id = controller_->GetSupervisedUserId();
  UserImageManager* image_manager =
      ChromeUserManager::Get()->GetUserImageManager(
          AccountId::FromUserEmail(user_id));
  switch (selected_image_) {
    case user_manager::User::USER_IMAGE_EXTERNAL:
      // Photo decoding may not have been finished yet.
      if (user_photo_.isNull()) {
        apply_photo_after_decoding_ = true;
        return;
      }
      image_manager->SaveUserImage(
          user_manager::UserImage::CreateAndEncode(user_photo_));
      break;
    case user_manager::User::USER_IMAGE_PROFILE:
      NOTREACHED() << "Supervised users have no profile pictures";
      break;
    default:
      DCHECK(selected_image_ >= 0 &&
             selected_image_ < default_user_image::kDefaultImagesCount);
      image_manager->SaveUserDefaultImageIndex(selected_image_);
      break;
  }
  // Proceed to tutorial.
  actor_->ShowTutorialPage();
}

void SupervisedUserCreationScreen::OnCreationSuccess() {
  ApplyPicture();
}

void SupervisedUserCreationScreen::OnCameraPresenceCheckDone(
    bool is_camera_present) {
  if (actor_)
    actor_->SetCameraPresent(is_camera_present);
}

void SupervisedUserCreationScreen::OnGetSupervisedUsers(
    const base::DictionaryValue* users) {
  // Copy for passing to WebUI, contains only id, name and avatar URL.
  scoped_ptr<base::ListValue> ui_users(new base::ListValue());
  SupervisedUserManager* supervised_user_manager =
      ChromeUserManager::Get()->GetSupervisedUserManager();

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

    int avatar_index = SupervisedUserCreationController::kDummyAvatarIndex;
    std::string chromeos_avatar;
    if (local_copy->GetString(SupervisedUserSyncService::kChromeOsAvatar,
                              &chromeos_avatar) &&
        !chromeos_avatar.empty() &&
        SupervisedUserSyncService::GetAvatarIndex(
            chromeos_avatar, &avatar_index)) {
      ui_copy->SetString(kAvatarURLKey,
                         default_user_image::GetDefaultImageUrl(avatar_index));
    } else {
      int i = base::RandInt(default_user_image::kFirstDefaultImageIndex,
                            default_user_image::kDefaultImagesCount - 1);
      local_copy->SetString(
          SupervisedUserSyncService::kChromeOsAvatar,
          SupervisedUserSyncService::BuildAvatarString(i));
      local_copy->SetBoolean(kRandomAvatarKey, true);
      ui_copy->SetString(kAvatarURLKey,
                         default_user_image::GetDefaultImageUrl(i));
    }

    local_copy->SetBoolean(kUserExists, false);
    ui_copy->SetBoolean(kUserExists, false);

    base::string16 display_name;
    local_copy->GetString(SupervisedUserSyncService::kName, &display_name);

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
    ui_copy->SetString(SupervisedUserSyncService::kName, display_name);

    std::string signature_key;
    bool has_password =
        local_copy->GetString(SupervisedUserSyncService::kPasswordSignatureKey,
                              &signature_key) &&
        !signature_key.empty();

    ui_copy->SetBoolean(kUserNeedPassword, !has_password);
    ui_copy->SetString("id", it.key());

    existing_users_->Set(it.key(), local_copy);
    ui_users->Append(ui_copy);
  }
  actor_->ShowExistingSupervisedUsers(ui_users.get());
}

void SupervisedUserCreationScreen::OnPhotoTaken(
    const std::string& raw_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  user_photo_ = gfx::ImageSkia();
  ImageDecoder::Cancel(this);
  ImageDecoder::Start(this, raw_data);
}

void SupervisedUserCreationScreen::OnImageDecoded(
    const SkBitmap& decoded_image) {
  user_photo_ = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
  if (apply_photo_after_decoding_)
    ApplyPicture();
}

void SupervisedUserCreationScreen::OnDecodeImageFailed() {
  NOTREACHED() << "Failed to decode PNG image from WebUI";
}

void SupervisedUserCreationScreen::OnImageSelected(
    const std::string& image_type,
    const std::string& image_url) {
  if (image_url.empty())
    return;
  int user_image_index = user_manager::User::USER_IMAGE_INVALID;
  if (image_type == "default" &&
      default_user_image::IsDefaultImageUrl(image_url, &user_image_index)) {
    selected_image_ = user_image_index;
  } else if (image_type == "camera") {
    selected_image_ = user_manager::User::USER_IMAGE_EXTERNAL;
  } else {
    NOTREACHED() << "Unexpected image type: " << image_type;
  }
}

void SupervisedUserCreationScreen::OnImageAccepted() {
}

}  // namespace chromeos
