// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"

#include "base/callback.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_paths.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/window/window.h"

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
      NewCallback(this, &ChangePictureOptionsHandler::ChooseFile));
  web_ui_->RegisterMessageCallback(
      "takePhoto",
      NewCallback(this, &ChangePictureOptionsHandler::TakePhoto));
  web_ui_->RegisterMessageCallback(
      "getAvailableImages",
      NewCallback(this, &ChangePictureOptionsHandler::GetAvailableImages));
  web_ui_->RegisterMessageCallback(
      "selectImage",
      NewCallback(this, &ChangePictureOptionsHandler::SelectImage));
}

void ChangePictureOptionsHandler::ChooseFile(const ListValue* args) {
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

void ChangePictureOptionsHandler::TakePhoto(const ListValue* args) {
  DCHECK(args && args->empty());
  views::Window* window = browser::CreateViewsWindow(
      GetBrowserWindow(),
      gfx::Rect(),
      new TakePhotoDialog());
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void ChangePictureOptionsHandler::GetAvailableImages(const ListValue* args) {
  DCHECK(args && args->empty());
  ListValue image_urls;
  for (int i = 0; i < kDefaultImagesCount; ++i) {
    image_urls.Append(new StringValue(GetDefaultImageUrl(i)));
  }
  web_ui_->CallJavascriptFunction("ChangePictureOptions.addUserImages",
                                  image_urls);
}

void ChangePictureOptionsHandler::SelectImage(const ListValue* args) {
  std::string image_url;
  if (!args ||
      args->GetSize() != 1 ||
      !args->GetString(0, &image_url)) {
    NOTREACHED();
    return;
  }
  int user_image_index = -1;
  if (!IsDefaultImageUrl(image_url, &user_image_index))
    return;

  const SkBitmap* image = ResourceBundle::GetSharedInstance().GetBitmapNamed(
          kDefaultImageResources[user_image_index]);
  UserManager* user_manager = UserManager::Get();
  user_manager->SetLoggedInUserImage(*image);
  user_manager->SaveUserImagePath(
      user_manager->logged_in_user().email(),
      GetDefaultImagePath(user_image_index));
}

void ChangePictureOptionsHandler::FileSelected(const FilePath& path,
                                               int index,
                                               void* params) {
  UserManager::Get()->LoadLoggedInUserImage(path);
}

gfx::NativeWindow ChangePictureOptionsHandler::GetBrowserWindow() const {
  Browser* browser = BrowserList::FindBrowserWithProfile(web_ui_->GetProfile());
  if (!browser)
    return NULL;
  return browser->window()->GetNativeHandle();
}

}  // namespace chromeos
