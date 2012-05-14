// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/set_wallpaper_options_handler2.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_resources.h"
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
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace options2 {

SetWallpaperOptionsHandler::SetWallpaperOptionsHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

SetWallpaperOptionsHandler::~SetWallpaperOptionsHandler() {
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
  web_ui()->CallJavascriptFunction("SetWallpaperOptions.setSelectedImage",
                                   image_url, is_random);
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
