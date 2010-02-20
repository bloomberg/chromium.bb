// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookie_modal_dialog.h"

#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/cookie_prompt_view.h"
#include "chrome/common/pref_names.h"

// Cookies
CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    const GURL& origin,
    const std::string& cookie_line,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      dialog_type_(DIALOG_TYPE_COOKIE),
      origin_(origin),
      cookie_line_(cookie_line),
      delegate_(delegate) {
}

// LocalStorage
CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    const GURL& origin,
    const string16& key,
    const string16& value,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      dialog_type_(DIALOG_TYPE_LOCAL_STORAGE),
      origin_(origin),
      local_storage_key_(key),
      local_storage_value_(value),
      delegate_(delegate) {
}

// Database
CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    const GURL& origin,
    const string16& database_name,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      dialog_type_(DIALOG_TYPE_DATABASE),
      origin_(origin),
      database_name_(database_name),
      delegate_(delegate) {
}

bool CookiePromptModalDialog::IsValid() {
  HostContentSettingsMap* host_content_settings_map =
      tab_contents()->profile()->GetHostContentSettingsMap();
  ContentSetting content_setting =
      host_content_settings_map->GetContentSetting(
          origin(),
          CONTENT_SETTINGS_TYPE_COOKIES);
  if (content_setting != CONTENT_SETTING_ASK) {
    if (content_setting == CONTENT_SETTING_ALLOW) {
      AllowSiteData(false, false);
    } else {
      DCHECK(content_setting == CONTENT_SETTING_BLOCK);
      BlockSiteData(false);
    }
    return false;
  }
  return true;
}

void CookiePromptModalDialog::AllowSiteData(bool remember,
                                            bool session_expire) {
  if (delegate_) {
    delegate_->AllowSiteData(remember, session_expire);
    delegate_ = NULL;  // It can be deleted at any point now.
  }
}

void CookiePromptModalDialog::BlockSiteData(bool remember) {
  if (delegate_) {
    delegate_->BlockSiteData(remember);
    delegate_ = NULL;  // It can be deleted at any point now.
  }
}

// static
void CookiePromptModalDialog::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kCookiePromptExpanded, false);
}
