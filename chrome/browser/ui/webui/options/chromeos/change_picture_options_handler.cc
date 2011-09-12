// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/widget/widget.h"

namespace chromeos {

namespace {

// Returns info about extensions for files we support as user images.
SelectFileDialog::FileTypeInfo GetUserImageFileTypeInfo() {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(5);

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("bmp"));

  file_type_info.extensions[1].push_back(FILE_PATH_LITERAL("gif"));

  file_type_info.extensions[2].push_back(FILE_PATH_LITERAL("jpg"));
  file_type_info.extensions[2].push_back(FILE_PATH_LITERAL("jpeg"));

  file_type_info.extensions[3].push_back(FILE_PATH_LITERAL("png"));

  file_type_info.extensions[4].push_back(FILE_PATH_LITERAL("tif"));
  file_type_info.extensions[4].push_back(FILE_PATH_LITERAL("tiff"));

  return file_type_info;
}

}  // namespace

ChangePictureOptionsHandler::ChangePictureOptionsHandler() {
}

ChangePictureOptionsHandler::~ChangePictureOptionsHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ChangePictureOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("changePicturePage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TITLE));
  localized_strings->SetString("changePicturePageDescription",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT));
  localized_strings->SetString("takePhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO));
  localized_strings->SetString("chooseFile",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_CHOOSE_FILE));
}

void ChangePictureOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback(
      "chooseFile",
      NewCallback(this, &ChangePictureOptionsHandler::HandleChooseFile));
  web_ui_->RegisterMessageCallback(
      "takePhoto",
      NewCallback(this, &ChangePictureOptionsHandler::HandleTakePhoto));
  web_ui_->RegisterMessageCallback(
      "getAvailableImages",
      NewCallback(this,
                  &ChangePictureOptionsHandler::HandleGetAvailableImages));
  web_ui_->RegisterMessageCallback(
      "getSelectedImage",
      NewCallback(this,
                  &ChangePictureOptionsHandler::HandleGetSelectedImage));
  web_ui_->RegisterMessageCallback(
      "selectImage",
      NewCallback(this, &ChangePictureOptionsHandler::HandleSelectImage));
}

void ChangePictureOptionsHandler::HandleChooseFile(const ListValue* args) {
  DCHECK(args && args->empty());
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  FilePath downloads_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path)) {
    NOTREACHED();
    return;
  }

  // Static so we initialize it only once.
  static SelectFileDialog::FileTypeInfo file_type_info =
      GetUserImageFileTypeInfo();

  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TITLE),
      downloads_path,
      &file_type_info,
      0,
      FILE_PATH_LITERAL(""),
      web_ui_->tab_contents(),
      GetBrowserWindow(),
      NULL);
}

void ChangePictureOptionsHandler::HandleTakePhoto(const ListValue* args) {
  DCHECK(args && args->empty());
  views::Widget* window = browser::CreateViewsWindow(
      GetBrowserWindow(),
      new TakePhotoDialog(this));
  window->SetAlwaysOnTop(true);
  window->Show();
}

void ChangePictureOptionsHandler::HandleGetAvailableImages(
    const ListValue* args) {
  DCHECK(args && args->empty());
  ListValue image_urls;
  for (int i = 0; i < kDefaultImagesCount; ++i) {
    image_urls.Append(new StringValue(GetDefaultImageUrl(i)));
  }
  web_ui_->CallJavascriptFunction("ChangePictureOptions.setUserImages",
                                  image_urls);
}

void ChangePictureOptionsHandler::HandleGetSelectedImage(
    const base::ListValue* args) {
  DCHECK(args && args->empty());

  const UserManager::User& user = UserManager::Get()->logged_in_user();
  DCHECK(!user.email().empty());

  int image_index = user.default_image_index();
  if (image_index == UserManager::User::kExternalImageIndex) {
    // User has image from camera/file, copy it and add to the list of images.
    previous_image_ = user.image();
    web_ui_->CallJavascriptFunction("ChangePictureOptions.addOldImage");
  } else {
    base::StringValue image_url(GetDefaultImageUrl(image_index));
    web_ui_->CallJavascriptFunction("ChangePictureOptions.setSelectedImage",
                                    image_url);
  }
}

void ChangePictureOptionsHandler::HandleSelectImage(const ListValue* args) {
  std::string image_url;
  if (!args ||
      args->GetSize() != 1 ||
      !args->GetString(0, &image_url)) {
    NOTREACHED();
    return;
  }

  UserManager* user_manager = UserManager::Get();
  const UserManager::User& user = user_manager->logged_in_user();
  int user_image_index = UserManager::User::kExternalImageIndex;

  if (StartsWithASCII(image_url, chrome::kChromeUIUserImageURL, false)) {
    DCHECK(!previous_image_.empty());

    user_manager->SetLoggedInUserImage(previous_image_,
                                       UserManager::User::kExternalImageIndex);
    user_manager->SaveUserImage(user.email(), previous_image_);
    UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                              kHistogramImageOld,
                              kHistogramImagesCount);

    VLOG(1) << "Selected old user image";
  } else if (IsDefaultImageUrl(image_url, &user_image_index)) {
    const SkBitmap* image = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        kDefaultImageResources[user_image_index]);

    user_manager->SetLoggedInUserImage(*image, user_image_index);
    user_manager->SaveUserImagePath(
        user_manager->logged_in_user().email(),
        GetDefaultImagePath(user_image_index));
    UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                              user_image_index,
                              kHistogramImagesCount);

    VLOG(1) << "Selected default user image: " << user_image_index;
  }
}

void ChangePictureOptionsHandler::FileSelected(const FilePath& path,
                                               int index,
                                               void* params) {
  UserManager::Get()->LoadLoggedInUserImage(path);
  UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                            kHistogramImageFromFile,
                            kHistogramImagesCount);
}

void ChangePictureOptionsHandler::OnPhotoAccepted(const SkBitmap& photo) {
  UserManager* user_manager = UserManager::Get();
  DCHECK(user_manager);

  const UserManager::User& user = user_manager->logged_in_user();
  DCHECK(!user.email().empty());

  user_manager->SetLoggedInUserImage(photo,
                                     UserManager::User::kExternalImageIndex);
  user_manager->SaveUserImage(user.email(), photo);
  UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                            kHistogramImageFromCamera,
                            kHistogramImagesCount);
}

gfx::NativeWindow ChangePictureOptionsHandler::GetBrowserWindow() const {
  Browser* browser =
      BrowserList::FindBrowserWithProfile(Profile::FromWebUI(web_ui_));
  if (!browser)
    return NULL;
  return browser->window()->GetNativeHandle();
}

}  // namespace chromeos
