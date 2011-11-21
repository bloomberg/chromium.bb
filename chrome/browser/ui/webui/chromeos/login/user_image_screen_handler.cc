// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/user_image_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/camera_detector.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace {

// UserImage screen ID.
const char kUserImageScreen[] = "user-image";

}  // namespace

namespace chromeos {

UserImageScreenHandler::UserImageScreenHandler()
    : screen_(NULL),
      show_on_init_(false),
      selected_image_(User::kInvalidImageIndex),
      user_photo_data_url_(chrome::kAboutBlankURL),
      profile_picture_data_url_(chrome::kAboutBlankURL),
      profile_picture_absent_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

UserImageScreenHandler::~UserImageScreenHandler() {
}

void UserImageScreenHandler::GetLocalizedStrings(
    DictionaryValue *localized_strings) {
  // TODO(ivankr): string should be renamed to something like
  // IDS_USER_IMAGE_SCREEN_TITLE (currently used for Take Photo dialog).
  localized_strings->SetString("userImageScreenTitle",
      l10n_util::GetStringUTF16(IDS_OOBE_PICTURE));
  localized_strings->SetString("userImageScreenDescription",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT));
  localized_strings->SetString("takePhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO));
  localized_strings->SetString("profilePhoto",
      l10n_util::GetStringUTF16(IDS_IMAGE_SCREEN_PROFILE_PHOTO));
  localized_strings->SetString("profilePhotoLoading",
      l10n_util::GetStringUTF16(IDS_IMAGE_SCREEN_PROFILE_LOADING_PHOTO));
  localized_strings->SetString("okButtonText",
      l10n_util::GetStringUTF16(IDS_OK));
}

void UserImageScreenHandler::Initialize() {
  ListValue image_urls;
  for (int i = 0; i < kDefaultImagesCount; ++i) {
    image_urls.Append(new StringValue(GetDefaultImageUrl(i)));
  }
  web_ui_->CallJavascriptFunction("oobe.UserImageScreen.setUserImages",
                                  image_urls);

  if (selected_image_ != User::kInvalidImageIndex)
    SelectImage(selected_image_);

  if (profile_picture_data_url_ != chrome::kAboutBlankURL)
    SendProfileImage(profile_picture_data_url_);
  else if (profile_picture_absent_)
    OnProfileImageAbsent();

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void UserImageScreenHandler::SetDelegate(
    UserImageScreenActor::Delegate* screen) {
  screen_ = screen;
}

void UserImageScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  screen_show_time_ = base::Time::Now();
  ShowScreen(kUserImageScreen, NULL);
  // When shown, query camera presence again (first-time query is done by
  // OobeUI::OnLoginPromptVisible).
  CheckCameraPresence();
}

void UserImageScreenHandler::Hide() {
}

void UserImageScreenHandler::PrepareToShow() {
}

void UserImageScreenHandler::SelectImage(int index) {
  selected_image_ = index;
  if (page_is_ready()) {
    base::StringValue image_url(GetDefaultImageUrl(index));
    web_ui_->CallJavascriptFunction(
        "oobe.UserImageScreen.setSelectedImage",
        image_url);
  }
}

void UserImageScreenHandler::UpdateVideoFrame(const SkBitmap& frame) {
}

void UserImageScreenHandler::ShowCameraError() {
}

void UserImageScreenHandler::ShowCameraInitializing() {
}

void UserImageScreenHandler::CheckCameraPresence() {
  CameraDetector::StartPresenceCheck(
      base::Bind(&UserImageScreenHandler::OnCameraPresenceCheckDone,
                 weak_factory_.GetWeakPtr()));
}

bool UserImageScreenHandler::IsCapturing() const {
  return false;
}

void UserImageScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("takePhoto",
      base::Bind(&UserImageScreenHandler::HandleTakePhoto,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("selectImage",
      base::Bind(&UserImageScreenHandler::HandleSelectImage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("onUserImageAccepted",
      base::Bind(&UserImageScreenHandler::HandleImageAccepted,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("onUserImageScreenShown",
      base::Bind(&UserImageScreenHandler::HandleScreenShown,
                 base::Unretained(this)));
}

void UserImageScreenHandler::AddProfileImage(const SkBitmap& image) {
  profile_picture_data_url_ = web_ui_util::GetImageDataUrl(image);
  SendProfileImage(profile_picture_data_url_);
}

void UserImageScreenHandler::SendProfileImage(const std::string& data_url) {
  if (page_is_ready()) {
    base::StringValue data_url_value(data_url);
    web_ui_->CallJavascriptFunction("oobe.UserImageScreen.setProfileImage",
                                    data_url_value);
  }
}

void UserImageScreenHandler::OnProfileImageAbsent() {
  profile_picture_absent_ = true;
  if (page_is_ready()) {
    scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());
    web_ui_->CallJavascriptFunction("oobe.UserImageScreen.setProfileImage",
                                    *null_value);
  }
}

void UserImageScreenHandler::OnPhotoAccepted(const SkBitmap& photo) {
  user_photo_ = photo;
  user_photo_data_url_ = web_ui_util::GetImageDataUrl(user_photo_);
  selected_image_ = User::kExternalImageIndex;
  base::StringValue data_url(user_photo_data_url_);
  web_ui_->CallJavascriptFunction("oobe.UserImageScreen.setUserPhoto",
                                  data_url);
}

void UserImageScreenHandler::HandleTakePhoto(const base::ListValue* args) {
  DCHECK(args && args->empty());
  TakePhotoDialog* take_photo_dialog = new TakePhotoDialog(this);
  views::Widget* window = browser::CreateViewsWindow(
      GetNativeWindow(),
      take_photo_dialog);
  window->SetAlwaysOnTop(true);
  window->Show();
}

void UserImageScreenHandler::HandleSelectImage(const base::ListValue* args) {
  std::string image_url;
  if (!args ||
      args->GetSize() != 1 ||
      !args->GetString(0, &image_url)) {
    NOTREACHED();
    return;
  }
  if (image_url.empty())
    return;

  int user_image_index = User::kInvalidImageIndex;
  if (IsDefaultImageUrl(image_url, &user_image_index)) {
    selected_image_ = user_image_index;
  } else if (image_url == user_photo_data_url_) {
    selected_image_ = User::kExternalImageIndex;
  } else {
    selected_image_ = User::kProfileImageIndex;
  }
}

void UserImageScreenHandler::HandleImageAccepted(const base::ListValue* args) {
  DCHECK(args && args->empty());
  if (!screen_)
    return;
  switch (selected_image_) {
    case User::kExternalImageIndex:
      screen_->OnPhotoTaken(user_photo_);
      break;

    case User::kProfileImageIndex:
      screen_->OnProfileImageSelected();
      break;

    default:
      DCHECK(selected_image_ >= 0 && selected_image_ < kDefaultImagesCount);
      screen_->OnDefaultImageSelected(selected_image_);
  }
}

void UserImageScreenHandler::HandleScreenShown(const base::ListValue* args) {
  DCHECK(args && args->empty());
  DCHECK(!screen_show_time_.is_null());

  base::TimeDelta delta = base::Time::Now() - screen_show_time_;
  VLOG(1) << "Screen load time: " << delta.InSecondsF();
  UMA_HISTOGRAM_TIMES("UserImage.ScreenIsShownTime", delta);
}

void UserImageScreenHandler::OnCameraPresenceCheckDone() {
  base::FundamentalValue present_value(
      CameraDetector::camera_presence() == CameraDetector::kCameraPresent);
  web_ui_->CallJavascriptFunction("oobe.UserImageScreen.setCameraPresent",
                                  present_value);
}

}  // namespace chromeos
