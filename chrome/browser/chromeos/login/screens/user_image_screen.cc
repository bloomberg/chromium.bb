// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/user_image_screen.h"

#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/camera_detector.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/webui/web_ui_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Time histogram suffix for profile image download.
const char kProfileDownloadReason[] = "OOBE";

}  // namespace

UserImageScreen::UserImageScreen(ScreenObserver* screen_observer,
                                 UserImageScreenActor* actor)
    : WizardScreen(screen_observer),
      actor_(actor),
      weak_factory_(this),
      accept_photo_after_decoding_(false),
      selected_image_(User::kInvalidImageIndex),
      profile_picture_enabled_(false),
      profile_picture_data_url_(chrome::kAboutBlankURL),
      profile_picture_absent_(false) {
  actor_->SetDelegate(this);
  SetProfilePictureEnabled(true);
}

UserImageScreen::~UserImageScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
  if (image_decoder_.get())
    image_decoder_->set_delegate(NULL);
}

void UserImageScreen::CheckCameraPresence() {
  CameraDetector::StartPresenceCheck(
      base::Bind(&UserImageScreen::OnCameraPresenceCheckDone,
                 weak_factory_.GetWeakPtr()));
}

void UserImageScreen::OnCameraPresenceCheckDone() {
  if (actor_) {
    actor_->SetCameraPresent(
        CameraDetector::camera_presence() == CameraDetector::kCameraPresent);
  }
}

void UserImageScreen::OnPhotoTaken(const std::string& raw_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  user_photo_ = gfx::ImageSkia();
  if (image_decoder_.get())
    image_decoder_->set_delegate(NULL);
  image_decoder_ = new ImageDecoder(this, raw_data,
                                    ImageDecoder::DEFAULT_CODEC);
  scoped_refptr<base::MessageLoopProxy> task_runner =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  image_decoder_->Start(task_runner);
}

void UserImageScreen::OnImageDecoded(const ImageDecoder* decoder,
                                     const SkBitmap& decoded_image) {
  DCHECK_EQ(image_decoder_.get(), decoder);
  user_photo_ = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
  if (accept_photo_after_decoding_)
    OnImageAccepted();
}

void UserImageScreen::OnDecodeImageFailed(const ImageDecoder* decoder) {
  NOTREACHED() << "Failed to decode PNG image from WebUI";
}

void UserImageScreen::OnImageSelected(const std::string& image_type,
                                      const std::string& image_url) {
  if (image_url.empty())
    return;
  int user_image_index = User::kInvalidImageIndex;
  if (image_type == "default" &&
      IsDefaultImageUrl(image_url, &user_image_index)) {
    selected_image_ = user_image_index;
  } else if (image_type == "camera") {
    selected_image_ = User::kExternalImageIndex;
  } else if (image_type == "profile") {
    selected_image_ = User::kProfileImageIndex;
  } else {
    NOTREACHED() << "Unexpected image type: " << image_type;
  }
}

void UserImageScreen::OnImageAccepted() {
  UserManager* user_manager = UserManager::Get();
  UserImageManager* image_manager = user_manager->GetUserImageManager();
  std::string user_id = GetUser()->email();
  int uma_index = 0;
  switch (selected_image_) {
    case User::kExternalImageIndex:
      // Photo decoding may not have been finished yet.
      if (user_photo_.isNull()) {
        accept_photo_after_decoding_ = true;
        return;
      }
      image_manager->
          SaveUserImage(user_id, UserImage::CreateAndEncode(user_photo_));
      uma_index = kHistogramImageFromCamera;
      break;
    case User::kProfileImageIndex:
      image_manager->SaveUserImageFromProfileImage(user_id);
      uma_index = kHistogramImageFromProfile;
      break;
    default:
      DCHECK(selected_image_ >= 0 && selected_image_ < kDefaultImagesCount);
      image_manager->SaveUserDefaultImageIndex(user_id, selected_image_);
      uma_index = GetDefaultImageHistogramValue(selected_image_);
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("UserImage.FirstTimeChoice",
                            uma_index,
                            kHistogramImagesCount);
  get_screen_observer()->OnExit(ScreenObserver::USER_IMAGE_SELECTED);
}


void UserImageScreen::SetProfilePictureEnabled(bool profile_picture_enabled) {
  if (profile_picture_enabled_ == profile_picture_enabled)
    return;
  profile_picture_enabled_ = profile_picture_enabled;
  if (profile_picture_enabled) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
        content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
        content::NotificationService::AllSources());
  } else {
    registrar_.Remove(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
        content::NotificationService::AllSources());
    registrar_.Remove(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
        content::NotificationService::AllSources());
  }
  if (actor_)
    actor_->SetProfilePictureEnabled(profile_picture_enabled);
}

void UserImageScreen::SetUserID(const std::string& user_id) {
  DCHECK(!user_id_.empty());
  user_id_ = user_id;
}

void UserImageScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

const User* UserImageScreen::GetUser() {
  if (user_id_.empty())
    return UserManager::Get()->GetLoggedInUser();
  const User* user = UserManager::Get()->FindUser(user_id_);
  DCHECK(user);
  return user;
}

void UserImageScreen::Show() {
  if (!actor_)
    return;

  actor_->Show();
  actor_->SetProfilePictureEnabled(profile_picture_enabled_);

  selected_image_ = GetUser()->image_index();
  actor_->SelectImage(selected_image_);

  if (profile_picture_enabled_) {
    // Start fetching the profile image.
    UserManager::Get()->GetUserImageManager()->
        DownloadProfileImage(kProfileDownloadReason);
  }

  accessibility::MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT));
}

void UserImageScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string UserImageScreen::GetName() const {
  return WizardController::kUserImageScreenName;
}

void UserImageScreen::OnActorDestroyed(UserImageScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

void UserImageScreen::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(profile_picture_enabled_);
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED: {
      // We've got a new profile image.
      profile_picture_data_url_ = webui::GetBitmapDataUrl(
          *content::Details<const gfx::ImageSkia>(details).ptr()->bitmap());
      if (actor_)
        actor_->SendProfileImage(profile_picture_data_url_);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED: {
      // User has a default profile image or fetching profile image has failed.
      profile_picture_absent_ = true;
      if (actor_)
        actor_->OnProfileImageAbsent();
      break;
    }
    default:
      NOTREACHED();
  }
}

bool UserImageScreen::profile_picture_absent() {
  return profile_picture_absent_;
}

int UserImageScreen::selected_image() {
  return selected_image_;
}

std::string UserImageScreen::profile_picture_data_url() {
  return profile_picture_data_url_;
}

}  // namespace chromeos
