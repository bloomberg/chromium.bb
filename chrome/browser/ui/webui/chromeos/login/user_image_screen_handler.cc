// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/user_image_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/chromeos/camera_detector.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/data_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/web_ui_util.h"

namespace chromeos {

UserImageScreenHandler::UserImageScreenHandler()
    : screen_(NULL),
      show_on_init_(false),
      selected_image_(User::kInvalidImageIndex),
      accept_photo_after_decoding_(false),
      user_photo_data_url_(chrome::kAboutBlankURL),
      profile_picture_data_url_(chrome::kAboutBlankURL),
      profile_picture_absent_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

UserImageScreenHandler::~UserImageScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);
  if (image_decoder_.get())
    image_decoder_->set_delegate(NULL);
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
  localized_strings->SetString("discardPhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DISCARD_PHOTO));
  localized_strings->SetString("flipPhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_FLIP_PHOTO));
  localized_strings->SetString("profilePhoto",
      l10n_util::GetStringUTF16(IDS_IMAGE_SCREEN_PROFILE_PHOTO));
  localized_strings->SetString("profilePhotoLoading",
      l10n_util::GetStringUTF16(IDS_IMAGE_SCREEN_PROFILE_LOADING_PHOTO));
  localized_strings->SetString("okButtonText",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("authorCredit",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SET_WALLPAPER_AUTHOR_TEXT));
  localized_strings->SetString("photoFromCamera",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_PHOTO_FROM_CAMERA));
  localized_strings->SetString("photoCaptureAccessibleText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PHOTO_CAPTURE_ACCESSIBLE_TEXT));
  localized_strings->SetString("photoDiscardAccessibleText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PHOTO_DISCARD_ACCESSIBLE_TEXT));
}

void UserImageScreenHandler::Initialize() {
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
  ShowScreen(OobeUI::kScreenUserImagePicker, NULL);

  // When shown, query camera presence.
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
    web_ui()->CallJavascriptFunction(
        "oobe.UserImageScreen.setSelectedImage",
        image_url);
  }
}

void UserImageScreenHandler::CheckCameraPresence() {
  CameraDetector::StartPresenceCheck(
      base::Bind(&UserImageScreenHandler::OnCameraPresenceCheckDone,
                 weak_factory_.GetWeakPtr()));
}

void UserImageScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getImages",
      base::Bind(&UserImageScreenHandler::HandleGetImages,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("photoTaken",
      base::Bind(&UserImageScreenHandler::HandlePhotoTaken,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("selectImage",
      base::Bind(&UserImageScreenHandler::HandleSelectImage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("checkCameraPresence",
      base::Bind(&UserImageScreenHandler::HandleCheckCameraPresence,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onUserImageAccepted",
      base::Bind(&UserImageScreenHandler::HandleImageAccepted,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onUserImageScreenShown",
      base::Bind(&UserImageScreenHandler::HandleScreenShown,
                 base::Unretained(this)));
}

void UserImageScreenHandler::AddProfileImage(const gfx::ImageSkia& image) {
  profile_picture_data_url_ = webui::GetBitmapDataUrl(*image.bitmap());
  SendProfileImage(profile_picture_data_url_);
}

void UserImageScreenHandler::SendProfileImage(const std::string& data_url) {
  if (page_is_ready()) {
    base::StringValue data_url_value(data_url);
    web_ui()->CallJavascriptFunction("oobe.UserImageScreen.setProfileImage",
                                     data_url_value);
  }
}

void UserImageScreenHandler::OnProfileImageAbsent() {
  profile_picture_absent_ = true;
  if (page_is_ready()) {
    scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());
    web_ui()->CallJavascriptFunction("oobe.UserImageScreen.setProfileImage",
                                     *null_value);
  }
}

void UserImageScreenHandler::HandleGetImages(const base::ListValue* args) {
  DCHECK(args && !args->GetSize());

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
  web_ui()->CallJavascriptFunction("oobe.UserImageScreen.setDefaultImages",
                                   image_urls);

  if (selected_image_ != User::kInvalidImageIndex)
    SelectImage(selected_image_);

  if (profile_picture_data_url_ != chrome::kAboutBlankURL)
    SendProfileImage(profile_picture_data_url_);
  else if (profile_picture_absent_)
    OnProfileImageAbsent();
}

void UserImageScreenHandler::HandlePhotoTaken(const base::ListValue* args) {
  std::string image_url;
  if (!args || args->GetSize() != 1 || !args->GetString(0, &image_url))
    NOTREACHED();
  DCHECK(!image_url.empty());

  std::string mime_type, charset, raw_data;
  if (!net::DataURL::Parse(GURL(image_url), &mime_type, &charset, &raw_data))
    NOTREACHED();
  DCHECK_EQ("image/png", mime_type);

  user_photo_ = gfx::ImageSkia();
  user_photo_data_url_ = image_url;

  if (image_decoder_.get())
    image_decoder_->set_delegate(NULL);
  image_decoder_ = new ImageDecoder(this, raw_data,
                                    ImageDecoder::DEFAULT_CODEC);
  image_decoder_->Start();
}

void UserImageScreenHandler::HandleCheckCameraPresence(
    const base::ListValue* args) {
  DCHECK(args->empty());
  CheckCameraPresence();
}

void UserImageScreenHandler::HandleSelectImage(const base::ListValue* args) {
  std::string image_url;
  std::string image_type;
  if (!args ||
      args->GetSize() != 2 ||
      !args->GetString(0, &image_url) ||
      !args->GetString(1, &image_type)) {
    NOTREACHED();
    return;
  }
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

void UserImageScreenHandler::HandleImageAccepted(const base::ListValue* args) {
  DCHECK(args && args->empty());
  if (!screen_)
    return;
  switch (selected_image_) {
    case User::kExternalImageIndex:
      // Photo decoding may not have been finished yet.
      if (user_photo_.isNull())
        accept_photo_after_decoding_ = true;
      else
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
  web_ui()->CallJavascriptFunction("oobe.UserImageScreen.setCameraPresent",
                                   present_value);
}

void UserImageScreenHandler::OnImageDecoded(const ImageDecoder* decoder,
                                            const SkBitmap& decoded_image) {
  DCHECK_EQ(image_decoder_.get(), decoder);
  user_photo_ = gfx::ImageSkia(decoded_image);
  if (screen_ && accept_photo_after_decoding_)
    screen_->OnPhotoTaken(user_photo_);
}

void UserImageScreenHandler::OnDecodeImageFailed(const ImageDecoder* decoder) {
  NOTREACHED() << "Failed to decode PNG image from WebUI";
}

}  // namespace chromeos
