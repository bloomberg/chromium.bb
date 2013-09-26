// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_screen.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/values.h"
#include "chrome/browser/chromeos/camera_detector.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_controller.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/network/network_state.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

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
                            network->name());
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
      on_image_screen_(false),
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
    if (on_image_screen_)
      actor_->ShowTutorialPage();
    else
      actor_->ShowIntroPage();
  }

  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  if (detector && !on_error_screen_)
    detector->AddAndFireObserver(this);
  on_error_screen_ = false;
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
  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  if (detector && !on_error_screen_)
    detector->RemoveObserver(this);
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
  controller_.reset(new LocallyManagedUserCreationController(this, manager_id));

  ExistingUserController::current_controller()->
      Login(UserContext(manager_id,
                        manager_password,
                        std::string()  /* auth_code */));
}

void LocallyManagedUserCreationScreen::CreateManagedUser(
    const string16& display_name,
    const std::string& managed_user_password) {
  DCHECK(controller_.get());
  controller_->SetUpCreation(display_name, managed_user_password);
  controller_->StartCreation();
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
    LocallyManagedUserCreationController::ErrorCode code) {
  string16 title;
  string16 message;
  string16 button;
  // TODO(antrim) : find out which errors do we really have.
  // We might reuse some error messages from ordinary user flow.
  switch (code) {
    case LocallyManagedUserCreationController::CRYPTOHOME_NO_MOUNT:
    case LocallyManagedUserCreationController::CRYPTOHOME_FAILED_MOUNT:
    case LocallyManagedUserCreationController::CRYPTOHOME_FAILED_TPM:
      title = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_TPM_ERROR_TITLE);
      message = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_TPM_ERROR);
      button = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_TPM_ERROR_BUTTON);
      break;
    case LocallyManagedUserCreationController::CLOUD_SERVER_ERROR:
    case LocallyManagedUserCreationController::TOKEN_WRITE_FAILED:
      title = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR_TITLE);
      message = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR);
      button = l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_GENERIC_ERROR_BUTTON);
      break;
    case LocallyManagedUserCreationController::NO_ERROR:
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

// TODO(antrim) : this is an explicit code duplications with UserImageScreen.
// It should be removed by issue 251179.

void LocallyManagedUserCreationScreen::ApplyPicture() {
  UserManager* user_manager = UserManager::Get();
  UserImageManager* image_manager = user_manager->GetUserImageManager();
  std::string user_id = controller_->GetManagedUserId();
  switch (selected_image_) {
    case User::kExternalImageIndex:
      // Photo decoding may not have been finished yet.
      if (user_photo_.isNull()) {
        apply_photo_after_decoding_ = true;
        return;
      }
      image_manager->
          SaveUserImage(user_id, UserImage::CreateAndEncode(user_photo_));
      break;
    case User::kProfileImageIndex:
      NOTREACHED() << "Supervised users have no profile pictures";
      break;
    default:
      DCHECK(selected_image_ >= 0 && selected_image_ < kDefaultImagesCount);
      image_manager->SaveUserDefaultImageIndex(user_id, selected_image_);
      break;
  }
  // Proceed to tutorial.
  actor_->ShowTutorialPage();
}

void LocallyManagedUserCreationScreen::OnCreationSuccess() {
  ApplyPicture();
}

void LocallyManagedUserCreationScreen::CheckCameraPresence() {
  CameraDetector::StartPresenceCheck(
      base::Bind(&LocallyManagedUserCreationScreen::OnCameraPresenceCheckDone,
                 weak_factory_.GetWeakPtr()));
}

void LocallyManagedUserCreationScreen::OnCameraPresenceCheckDone() {
  if (actor_) {
    actor_->SetCameraPresent(
        CameraDetector::camera_presence() == CameraDetector::kCameraPresent);
  }
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
