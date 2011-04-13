// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"

#include "base/callback.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_paths.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
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
  web_ui_->RegisterMessageCallback(
      "chooseFile",
      NewCallback(this, &ChangePictureOptionsHandler::ChooseFile));
  web_ui_->RegisterMessageCallback(
      "takePhoto",
      NewCallback(this, &ChangePictureOptionsHandler::TakePhoto));
}

void ChangePictureOptionsHandler::ChooseFile(const ListValue* args) {
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
      NULL,
      NULL);
}

void ChangePictureOptionsHandler::TakePhoto(const ListValue* args) {
  Browser* browser = BrowserList::FindBrowserWithProfile(web_ui_->GetProfile());
  views::Window* window = browser::CreateViewsWindow(
      browser->window()->GetNativeHandle(),
      gfx::Rect(),
      new TakePhotoDialog());
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void ChangePictureOptionsHandler::FileSelected(const FilePath& path,
                                               int index,
                                               void* params) {
  UserManager::Get()->LoadLoggedInUserImage(path);
}

}  // namespace chromeos
