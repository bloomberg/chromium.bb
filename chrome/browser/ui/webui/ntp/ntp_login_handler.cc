// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_login_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/sync_promo_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

NTPLoginHandler::NTPLoginHandler() {
}

NTPLoginHandler::~NTPLoginHandler() {
}

WebUIMessageHandler* NTPLoginHandler::Attach(WebUI* web_ui) {
  PrefService* pref_service = Profile::FromWebUI(web_ui)->GetPrefs();
  username_pref_.Init(prefs::kGoogleServicesUsername, pref_service, this);

  return WebUIMessageHandler::Attach(web_ui);
}

void NTPLoginHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("initializeSyncLogin",
      base::Bind(&NTPLoginHandler::HandleInitializeSyncLogin,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("showSyncLoginUI",
      base::Bind(&NTPLoginHandler::HandleShowSyncLoginUI,
                 base::Unretained(this)));
}

void NTPLoginHandler::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_PREF_CHANGED);
  std::string* name = Details<std::string>(details).ptr();
  if (prefs::kGoogleServicesUsername == *name)
    UpdateLogin();
}

void NTPLoginHandler::HandleInitializeSyncLogin(const ListValue* args) {
  UpdateLogin();
}

void NTPLoginHandler::HandleShowSyncLoginUI(const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  std::string username = profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);

  if (username.empty()) {
    // The user isn't signed in, show the sync promo.
    if (SyncPromoUI::ShouldShowSyncPromo(profile)) {
        web_ui_->tab_contents()->OpenURL(GURL(chrome::kChromeUISyncPromoURL),
                                         GURL(), CURRENT_TAB,
                                         content::PAGE_TRANSITION_LINK);
    }
  } else if (args->GetSize() == 4) {
    // The user is signed in, show the profiles menu.
    Browser* browser =
        BrowserList::FindBrowserWithTabContents(web_ui_->tab_contents());
    if (!browser)
      return;
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    bool success = args->GetDouble(0, &x);
    DCHECK(success);
    success = args->GetDouble(1, &y);
    DCHECK(success);
    success = args->GetDouble(2, &width);
    DCHECK(success);
    success = args->GetDouble(3, &height);
    DCHECK(success);
    gfx::Rect rect(x, y, width, height);
    browser->window()->ShowAvatarBubble(web_ui_->tab_contents(), rect);
  }
}

void NTPLoginHandler::UpdateLogin() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  std::string username = profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  string16 header, sub_header;
  if (!username.empty()) {
    header = UTF8ToUTF16(username);
  } else if (SyncPromoUI::ShouldShowSyncPromo(profile)) {
    string16 signed_in_link = l10n_util::GetStringUTF16(
        IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_LINK);
    signed_in_link = ASCIIToUTF16("<span class='link-span'>") + signed_in_link +
                     ASCIIToUTF16("</span>");
    header = l10n_util::GetStringFUTF16(
        IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_HEADER, signed_in_link,
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
    sub_header = l10n_util::GetStringUTF16(
        IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_SUB_HEADER);
  }

  StringValue header_value(header);
  StringValue sub_header_value(sub_header);
  web_ui_->CallJavascriptFunction(
      "updateLogin", header_value, sub_header_value);
}

bool NTPLoginHandler::ShouldShow(Profile* profile) {
#if defined(OS_CHROMEOS)
  // For now we don't care about showing sync status on Chrome OS. The promo
  // UI and the avatar menu don't exist on that platform.
  return false;
#else
  if (profile->IsOffTheRecord())
    return false;

  return profile->GetOriginalProfile()->IsSyncAccessible();
#endif
}
