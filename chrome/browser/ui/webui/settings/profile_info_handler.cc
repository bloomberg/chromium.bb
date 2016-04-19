// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/profile_info_handler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/ui/user_manager.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#else
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif

namespace settings {

// static
const char ProfileInfoHandler::kProfileInfoChangedEventName[] =
    "profile-info-changed";

ProfileInfoHandler::ProfileInfoHandler(Profile* profile)
    : profile_(profile), observers_registered_(false) {}

void ProfileInfoHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfileInfo", base::Bind(&ProfileInfoHandler::HandleGetProfileInfo,
                                   base::Unretained(this)));
}

void ProfileInfoHandler::RenderViewReused() {
  if (!observers_registered_)
    return;

  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .RemoveObserver(this);

#if defined(OS_CHROMEOS)
  registrar_.RemoveAll();
#endif

  observers_registered_ = false;
}

#if defined(OS_CHROMEOS)
void ProfileInfoHandler::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED:
      PushProfileInfo();
      break;
    default:
      NOTREACHED();
  }
}
#endif

void ProfileInfoHandler::OnProfileNameChanged(
    const base::FilePath& /* profile_path */,
    const base::string16& /* old_profile_name */) {
  PushProfileInfo();
}

void ProfileInfoHandler::OnProfileAvatarChanged(
    const base::FilePath& /* profile_path */) {
  PushProfileInfo();
}

void ProfileInfoHandler::HandleGetProfileInfo(const base::ListValue* args) {
  if (!observers_registered_) {
    observers_registered_ = true;

    g_browser_process->profile_manager()
        ->GetProfileAttributesStorage()
        .AddObserver(this);

#if defined(OS_CHROMEOS)
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                   content::NotificationService::AllSources());
#endif
  }

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(*callback_id, *GetAccountNameAndIcon());
}

void ProfileInfoHandler::PushProfileInfo() {
  web_ui()->CallJavascriptFunction(
      "cr.webUIListenerCallback",
      base::StringValue(kProfileInfoChangedEventName),
      *GetAccountNameAndIcon());
}

std::unique_ptr<base::DictionaryValue>
ProfileInfoHandler::GetAccountNameAndIcon() const {
  std::string name;
  std::string icon_url;

#if defined(OS_CHROMEOS)
  name = profile_->GetProfileUserName();
  if (name.empty()) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user && (user->GetType() != user_manager::USER_TYPE_GUEST))
      name = user->email();
  }
  if (!name.empty())
    name = gaia::SanitizeEmail(gaia::CanonicalizeEmail(name));

  // Get image as data URL instead of using chrome://userimage source to avoid
  // issues with caching.
  const AccountId account_id(AccountId::FromUserEmail(name));
  scoped_refptr<base::RefCountedMemory> image =
      chromeos::options::UserImageSource::GetUserImage(account_id);
  icon_url = webui::GetPngDataUrl(image->front(), image->size());
#else   // !defined(OS_CHROMEOS)
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    name = base::UTF16ToUTF8(entry->GetName());

    if (entry->IsUsingGAIAPicture() && entry->GetGAIAPicture()) {
      gfx::Image icon =
          profiles::GetAvatarIconForWebUI(entry->GetAvatarIcon(), true);
      icon_url = webui::GetBitmapDataUrl(icon.AsBitmap());
    } else {
      icon_url = profiles::GetDefaultAvatarIconUrl(entry->GetAvatarIconIndex());
    }
  }
#endif  // defined(OS_CHROMEOS)

  base::DictionaryValue* response = new base::DictionaryValue();
  response->SetString("name", name);
  response->SetString("iconUrl", icon_url);
  return base::WrapUnique(response);
}

}  // namespace settings
