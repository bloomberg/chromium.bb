// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/user_image_screen_handler.h"

#include "ash/audio/sounds.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/default_user_images.h"
#include "grit/browser_resources.h"
#include "net/base/data_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

const char kJsScreenPath[] = "login.UserImageScreen";

}  // namespace

namespace chromeos {

UserImageScreenHandler::UserImageScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      screen_(NULL),
      show_on_init_(false),
      is_ready_(false) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  media::SoundsManager* manager = media::SoundsManager::Get();
  manager->Initialize(SOUND_OBJECT_DELETE,
                      bundle.GetRawDataResource(IDR_SOUND_OBJECT_DELETE_WAV));
  manager->Initialize(SOUND_CAMERA_SNAP,
                      bundle.GetRawDataResource(IDR_SOUND_CAMERA_SNAP_WAV));
}

UserImageScreenHandler::~UserImageScreenHandler() {
  if (screen_) {
    screen_->OnActorDestroyed(this);
  }
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
  if (!screen_)
    return;
  if (is_ready_)
    screen_->OnScreenReady();
}

void UserImageScreenHandler::Hide() {
}

void UserImageScreenHandler::PrepareToShow() {
}

void UserImageScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("userImageScreenTitle", IDS_USER_IMAGE_SCREEN_TITLE);
  builder->Add("userImageScreenDescription",
               IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT);
  builder->Add("takePhoto", IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO);
  builder->Add("discardPhoto", IDS_OPTIONS_CHANGE_PICTURE_DISCARD_PHOTO);
  builder->Add("flipPhoto", IDS_OPTIONS_CHANGE_PICTURE_FLIP_PHOTO);
  builder->Add("profilePhoto", IDS_IMAGE_SCREEN_PROFILE_PHOTO);
  builder->Add("profilePhotoLoading",
               IDS_IMAGE_SCREEN_PROFILE_LOADING_PHOTO);
  builder->Add("okButtonText", IDS_OK);
  builder->Add("authorCredit", IDS_OPTIONS_SET_WALLPAPER_AUTHOR_TEXT);
  builder->Add("photoFromCamera", IDS_OPTIONS_CHANGE_PICTURE_PHOTO_FROM_CAMERA);
  builder->Add("photoFlippedAccessibleText",
               IDS_OPTIONS_PHOTO_FLIP_ACCESSIBLE_TEXT);
  builder->Add("photoFlippedBackAccessibleText",
               IDS_OPTIONS_PHOTO_FLIPBACK_ACCESSIBLE_TEXT);
  builder->Add("photoCaptureAccessibleText",
               IDS_OPTIONS_PHOTO_CAPTURE_ACCESSIBLE_TEXT);
  builder->Add("photoDiscardAccessibleText",
               IDS_OPTIONS_PHOTO_DISCARD_ACCESSIBLE_TEXT);
  builder->Add("syncingPreferences", IDS_IMAGE_SCREEN_SYNCING_PREFERENCES);
}

void UserImageScreenHandler::RegisterMessages() {
  AddCallback("getImages", &UserImageScreenHandler::HandleGetImages);
  AddCallback("screenReady", &UserImageScreenHandler::HandleScreenReady);
  AddCallback("takePhoto", &UserImageScreenHandler::HandleTakePhoto);
  AddCallback("discardPhoto", &UserImageScreenHandler::HandleDiscardPhoto);
  AddCallback("photoTaken", &UserImageScreenHandler::HandlePhotoTaken);
  AddCallback("selectImage", &UserImageScreenHandler::HandleSelectImage);
  AddCallback("onUserImageAccepted",
              &UserImageScreenHandler::HandleImageAccepted);
  AddCallback("onUserImageScreenShown",
              &UserImageScreenHandler::HandleScreenShown);
}

void UserImageScreenHandler::SelectImage(int index) {
  if (page_is_ready())
    CallJS("setSelectedImage", user_manager::GetDefaultImageUrl(index));
}

void UserImageScreenHandler::SendProfileImage(const std::string& data_url) {
  if (page_is_ready())
    CallJS("setProfileImage", data_url);
}

void UserImageScreenHandler::OnProfileImageAbsent() {
  if (page_is_ready()) {
    scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());
    CallJS("setProfileImage", *null_value);
  }
}

// TODO(antrim) : It looks more like parameters for "Init" rather than callback.
void UserImageScreenHandler::HandleGetImages() {
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
  if (!screen_)
    return;
  if (screen_->selected_image() != user_manager::User::USER_IMAGE_INVALID)
    SelectImage(screen_->selected_image());

  if (screen_->profile_picture_data_url() != url::kAboutBlankURL)
    SendProfileImage(screen_->profile_picture_data_url());
  else if (screen_->profile_picture_absent())
    OnProfileImageAbsent();
}

void UserImageScreenHandler::HandleScreenReady() {
  is_ready_ = true;
  if (screen_)
    screen_->OnScreenReady();
}

void UserImageScreenHandler::HandlePhotoTaken(const std::string& image_url) {
  std::string mime_type, charset, raw_data;
  if (!net::DataURL::Parse(GURL(image_url), &mime_type, &charset, &raw_data))
    NOTREACHED();
  DCHECK_EQ("image/png", mime_type);

  if (screen_)
    screen_->OnPhotoTaken(raw_data);
}

void UserImageScreenHandler::HandleTakePhoto() {
  ash::PlaySystemSoundIfSpokenFeedback(SOUND_CAMERA_SNAP);
}

void UserImageScreenHandler::HandleDiscardPhoto() {
  ash::PlaySystemSoundIfSpokenFeedback(SOUND_OBJECT_DELETE);
}

void UserImageScreenHandler::HandleSelectImage(const std::string& image_url,
                                               const std::string& image_type,
                                               bool is_user_selection) {
  if (screen_)
    screen_->OnImageSelected(image_type, image_url, is_user_selection);
}

void UserImageScreenHandler::HandleImageAccepted() {
  if (screen_)
    screen_->OnImageAccepted();
}

void UserImageScreenHandler::HandleScreenShown() {
  DCHECK(!screen_show_time_.is_null());

  base::TimeDelta delta = base::Time::Now() - screen_show_time_;
  VLOG(1) << "Screen load time: " << delta.InSecondsF();
  UMA_HISTOGRAM_TIMES("UserImage.ScreenIsShownTime", delta);
}

void UserImageScreenHandler::SetCameraPresent(bool present) {
  CallJS("setCameraPresent", present);
}

void UserImageScreenHandler::HideCurtain() {
  CallJS("hideCurtain");
}

}  // namespace chromeos
