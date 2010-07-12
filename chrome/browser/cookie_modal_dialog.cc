// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookie_modal_dialog.h"

#include "app/message_box_flags.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"

// Cookies
CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const std::string& cookie_line,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      host_content_settings_map_(host_content_settings_map),
      dialog_type_(DIALOG_TYPE_COOKIE),
      origin_(origin),
      cookie_line_(cookie_line),
      delegate_(delegate) {
}

// LocalStorage
CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const string16& key,
    const string16& value,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      host_content_settings_map_(host_content_settings_map),
      dialog_type_(DIALOG_TYPE_LOCAL_STORAGE),
      origin_(origin),
      local_storage_key_(key),
      local_storage_value_(value),
      delegate_(delegate) {
}

// Database
CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const string16& database_name,
    const string16& display_name,
    unsigned long estimated_size,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      host_content_settings_map_(host_content_settings_map),
      dialog_type_(DIALOG_TYPE_DATABASE),
      origin_(origin),
      database_name_(database_name),
      display_name_(display_name),
      estimated_size_(estimated_size),
      delegate_(delegate) {
}

// AppCache
CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& appcache_manifest_url,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      host_content_settings_map_(host_content_settings_map),
      dialog_type_(DIALOG_TYPE_APPCACHE),
      origin_(appcache_manifest_url.GetOrigin()),
      appcache_manifest_url_(appcache_manifest_url),
      delegate_(delegate) {
}

CookiePromptModalDialog::~CookiePromptModalDialog() {
}

bool CookiePromptModalDialog::IsValid() {
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          origin_, CONTENT_SETTINGS_TYPE_COOKIES);
  if (content_setting != CONTENT_SETTING_ASK) {
    if (content_setting == CONTENT_SETTING_ALLOW) {
      AllowSiteData(false, false);
    } else if (content_setting == CONTENT_SETTING_SESSION_ONLY) {
      AllowSiteData(false, true);
    } else {
      DCHECK(content_setting == CONTENT_SETTING_BLOCK);
      BlockSiteData(false);
    }
    return false;
  }
  return !skip_this_dialog_;
}

void CookiePromptModalDialog::AllowSiteData(bool remember,
                                            bool session_expire) {
  if (remember) {
    // Make sure there is no entry that would override the pattern we are about
    // to insert for exactly this URL.
    host_content_settings_map_->SetContentSetting(
        HostContentSettingsMap::Pattern::FromURLNoWildcard(origin_),
        CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_DEFAULT);
    host_content_settings_map_->SetContentSetting(
        HostContentSettingsMap::Pattern::FromURL(origin_),
        CONTENT_SETTINGS_TYPE_COOKIES,
        session_expire ? CONTENT_SETTING_SESSION_ONLY : CONTENT_SETTING_ALLOW);
  }

  if (delegate_) {
    delegate_->AllowSiteData(session_expire);
    delegate_ = NULL;  // It can be deleted at any point now.
  }
}

void CookiePromptModalDialog::BlockSiteData(bool remember) {
  if (remember) {
    // Make sure there is no entry that would override the pattern we are about
    // to insert for exactly this URL.
    host_content_settings_map_->SetContentSetting(
        HostContentSettingsMap::Pattern::FromURLNoWildcard(origin_),
        CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_DEFAULT);
    host_content_settings_map_->SetContentSetting(
        HostContentSettingsMap::Pattern::FromURL(origin_),
        CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  }

  if (delegate_) {
    delegate_->BlockSiteData();
    delegate_ = NULL;  // It can be deleted at any point now.
  }
}

// static
void CookiePromptModalDialog::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kCookiePromptExpanded, false);
}

int CookiePromptModalDialog::GetDialogButtons() {
  // Enable the automation interface to accept/dismiss this dialog.
  return MessageBoxFlags::DIALOGBUTTON_OK |
         MessageBoxFlags::DIALOGBUTTON_CANCEL;
}
