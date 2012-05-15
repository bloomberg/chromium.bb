// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/set_wallpaper_options_handler2.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/options2/chromeos/wallpaper_thumbnail_source2.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace options2 {

namespace {

// Returns info about extensions for files we support as wallpaper images.
SelectFileDialog::FileTypeInfo GetUserImageFileTypeInfo() {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(3);

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_type_info.extensions[1].push_back(FILE_PATH_LITERAL("jpeg"));

  file_type_info.extensions[2].push_back(FILE_PATH_LITERAL("png"));

  return file_type_info;
}

}  // namespace

SetWallpaperOptionsHandler::SetWallpaperOptionsHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

SetWallpaperOptionsHandler::~SetWallpaperOptionsHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void SetWallpaperOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("setWallpaperPage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SET_WALLPAPER_DIALOG_TITLE));
  localized_strings->SetString("setWallpaperPageDescription",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SET_WALLPAPER_DIALOG_TEXT));
  localized_strings->SetString("setWallpaperAuthor",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SET_WALLPAPER_AUTHOR_TEXT));
  localized_strings->SetString("randomCheckbox",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SET_WALLPAPER_RANDOM));
  localized_strings->SetString("customWallpaper",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CUSTOME_WALLPAPER));
}

void SetWallpaperOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("onSetWallpaperPageInitialized",
      base::Bind(&SetWallpaperOptionsHandler::HandlePageInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onSetWallpaperPageShown",
      base::Bind(&SetWallpaperOptionsHandler::HandlePageShown,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("selectDefaultWallpaper",
      base::Bind(&SetWallpaperOptionsHandler::HandleDefaultWallpaper,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("selectRandomWallpaper",
      base::Bind(&SetWallpaperOptionsHandler::HandleRandomWallpaper,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("chooseWallpaper",
      base::Bind(&SetWallpaperOptionsHandler::HandleChooseFile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("changeWallpaperLayout",
      base::Bind(&SetWallpaperOptionsHandler::HandleLayoutChanged,
                 base::Unretained(this)));
}

void SetWallpaperOptionsHandler::SetCustomWallpaperThumbnail() {
  web_ui()->CallJavascriptFunction("SetWallpaperOptions.setCustomImage");
}

void SetWallpaperOptionsHandler::FileSelected(const FilePath& path,
                                              int index,
                                              void* params) {
  UserManager* user_manager = UserManager::Get();

  // Default wallpaper layout is CENTER_CROPPED.
  user_manager->SaveUserWallpaperFromFile(
      user_manager->GetLoggedInUser().email(), path, ash::CENTER_CROPPED, this);
  web_ui()->CallJavascriptFunction("SetWallpaperOptions.didSelectFile");
}

void SetWallpaperOptionsHandler::SendDefaultImages() {
  ListValue images;
  DictionaryValue* image_detail;
  ash::WallpaperInfo image_info;

  for (int i = 0; i < ash::GetWallpaperCount(); ++i) {
    images.Append(image_detail = new DictionaryValue());
    image_info = ash::GetWallpaperInfo(i);
    image_detail->SetString("url", GetDefaultWallpaperThumbnailURL(i));
    image_detail->SetString("author", image_info.author);
    image_detail->SetString("website", image_info.website);
  }

  web_ui()->CallJavascriptFunction("SetWallpaperOptions.setDefaultImages",
                                   images);
}

void SetWallpaperOptionsHandler::SendLayoutOptions(
    ash::WallpaperLayout layout) {
  ListValue layouts;
  DictionaryValue* entry;

  layouts.Append(entry = new DictionaryValue());
  entry->SetString("name",
      l10n_util::GetStringUTF16(IDS_OPTIONS_WALLPAPER_CENTER_LAYOUT));
  entry->SetInteger("index", ash::CENTER);

  layouts.Append(entry = new DictionaryValue());
  entry->SetString("name",
      l10n_util::GetStringUTF16(IDS_OPTIONS_WALLPAPER_CENTER_CROPPED_LAYOUT));
  entry->SetInteger("index", ash::CENTER_CROPPED);

  layouts.Append(entry = new DictionaryValue());
  entry->SetString("name",
      l10n_util::GetStringUTF16(IDS_OPTIONS_WALLPAPER_STRETCH_LAYOUT));
  entry->SetInteger("index", ash::STRETCH);

  base::FundamentalValue selected_value(static_cast<int>(layout));
  web_ui()->CallJavascriptFunction(
      "SetWallpaperOptions.populateWallpaperLayouts", layouts, selected_value);
}

void SetWallpaperOptionsHandler::HandlePageInitialized(
    const base::ListValue* args) {
  DCHECK(args && args->empty());

  SendDefaultImages();
}

void SetWallpaperOptionsHandler::HandlePageShown(const base::ListValue* args) {
  DCHECK(args && args->empty());
  User::WallpaperType type;
  int index;
  UserManager::Get()->GetLoggedInUserWallpaperProperties(&type, &index);
  base::StringValue image_url(GetDefaultWallpaperThumbnailURL(index));
  base::FundamentalValue is_random(type == User::RANDOM);
  if (type == User::CUSTOMIZED) {
    ash::WallpaperLayout layout = static_cast<ash::WallpaperLayout>(index);
    SendLayoutOptions(layout);
    web_ui()->CallJavascriptFunction("SetWallpaperOptions.setCustomImage");
  } else {
    SendLayoutOptions(ash::CENTER_CROPPED);
    web_ui()->CallJavascriptFunction("SetWallpaperOptions.setSelectedImage",
                                     image_url, is_random);
  }
}

void SetWallpaperOptionsHandler::HandleChooseFile(const ListValue* args) {
  DCHECK(args && args->empty());
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  FilePath downloads_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path))
    NOTREACHED();

  // Static so we initialize it only once.
  CR_DEFINE_STATIC_LOCAL(SelectFileDialog::FileTypeInfo, file_type_info,
      (GetUserImageFileTypeInfo()));

  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                  l10n_util::GetStringUTF16(IDS_DOWNLOAD_TITLE),
                                  downloads_path, &file_type_info, 0,
                                  FILE_PATH_LITERAL(""),
                                  web_ui()->GetWebContents(),
                                  GetBrowserWindow(), NULL);
}

void SetWallpaperOptionsHandler::HandleLayoutChanged(const ListValue* args) {
  int selected_layout = ash::CENTER_CROPPED;
  if (!ExtractIntegerValue(args, &selected_layout))
    NOTREACHED() << "Could not read wallpaper layout from JSON argument";

  ash::WallpaperLayout layout =
      static_cast<ash::WallpaperLayout>(selected_layout);

  UserManager::Get()->SetLoggedInUserCustomWallpaperLayout(layout);
}

void SetWallpaperOptionsHandler::HandleDefaultWallpaper(const ListValue* args) {
  std::string image_url;
  if (!args ||
      args->GetSize() != 1 ||
      !args->GetString(0, &image_url))
    NOTREACHED();

  if (image_url.empty())
    return;

  int user_image_index;
  if (IsDefaultWallpaperURL(image_url, &user_image_index)) {
    UserManager::Get()->SaveLoggedInUserWallpaperProperties(User::DEFAULT,
                                                            user_image_index);
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetLoggedInUserWallpaper();
  }
}

void SetWallpaperOptionsHandler::HandleRandomWallpaper(const ListValue* args) {
  int index = ash::GetRandomWallpaperIndex();
  UserManager::Get()->SaveLoggedInUserWallpaperProperties(User::RANDOM, index);
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetDefaultWallpaper(index);
  base::StringValue image_url(GetDefaultWallpaperThumbnailURL(index));
  base::FundamentalValue is_random(true);
  web_ui()->CallJavascriptFunction("SetWallpaperOptions.setSelectedImage",
                                   image_url, is_random);
}

gfx::NativeWindow SetWallpaperOptionsHandler::GetBrowserWindow() const {
  Browser* browser =
      BrowserList::FindBrowserWithProfile(Profile::FromWebUI(web_ui()));
  if (!browser)
    return NULL;
  return browser->window()->GetNativeHandle();
}

}  // namespace options2
}  // namespace chromeos
