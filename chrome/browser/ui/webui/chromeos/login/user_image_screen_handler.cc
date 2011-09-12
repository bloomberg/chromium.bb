// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/user_image_screen_handler.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/widget/widget.h"

namespace {

// UserImage screen ID.
const char kUserImageScreen[] = "user-image";

}  // namespace

namespace chromeos {

UserImageScreenHandler::UserImageScreenHandler()
    : screen_(NULL),
      show_on_init_(false),
      selected_image_(UserManager::User::kInvalidImageIndex) {
}

UserImageScreenHandler::~UserImageScreenHandler() {
}

void UserImageScreenHandler::GetLocalizedStrings(
    DictionaryValue *localized_strings) {
  localized_strings->SetString("userImageScreenTitle",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TITLE));
  localized_strings->SetString("userImageScreenDescription",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT));
  localized_strings->SetString("takePhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO));
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

  if (selected_image_ != UserManager::User::kInvalidImageIndex)
    SelectImage(selected_image_);

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
  ShowScreen(kUserImageScreen, NULL);
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

bool UserImageScreenHandler::IsCapturing() const {
  return false;
}

void UserImageScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "takePhoto",
      NewCallback(this, &UserImageScreenHandler::HandleTakePhoto));
  web_ui_->RegisterMessageCallback(
      "selectImage",
      NewCallback(this, &UserImageScreenHandler::HandleSelectImage));
  web_ui_->RegisterMessageCallback(
      "onUserImageAccepted",
      NewCallback(this, &UserImageScreenHandler::HandleImageAccepted));
}

void UserImageScreenHandler::OnPhotoAccepted(const SkBitmap& photo) {
  user_photo_ = photo;
  selected_image_ = UserManager::User::kExternalImageIndex;
  StringValue data_url(web_ui_util::GetImageDataUrl(user_photo_));
  web_ui_->CallJavascriptFunction("oobe.UserImageScreen.setUserPhoto",
                                  data_url);
}

void UserImageScreenHandler::HandleTakePhoto(const base::ListValue* args) {
  DCHECK(args && args->empty());
  TakePhotoDialog* take_photo_dialog = new TakePhotoDialog(this);
  views::Widget* login_window = WebUILoginDisplay::GetLoginWindow();
  views::Widget* window = browser::CreateViewsWindow(
      login_window->GetNativeWindow(),
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
  int user_image_index = UserManager::User::kInvalidImageIndex;
  if (IsDefaultImageUrl(image_url, &user_image_index))
    selected_image_ = user_image_index;
  else
    selected_image_ = UserManager::User::kExternalImageIndex;
}

void UserImageScreenHandler::HandleImageAccepted(const base::ListValue* args) {
  DCHECK(args && args->empty());
  if (!screen_)
    return;
  if (selected_image_ == UserManager::User::kExternalImageIndex) {
    screen_->OnPhotoTaken(user_photo_);
  } else {
    DCHECK(selected_image_ >= 0);
    screen_->OnDefaultImageSelected(selected_image_);
  }
}

}  // namespace chromeos
