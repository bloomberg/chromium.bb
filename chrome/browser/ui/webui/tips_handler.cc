// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tips_handler.h"

#include <string>

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

WebUIMessageHandler* TipsHandler::Attach(WebUI* web_ui) {
  web_ui_ = web_ui;
  tips_cache_ = web_ui_->GetProfile()->GetPrefs()->
      GetMutableDictionary(prefs::kNTPPromoResourceCache);
  return WebUIMessageHandler::Attach(web_ui);
}

void TipsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getTips",
      NewCallback(this, &TipsHandler::HandleGetTips));
}

void TipsHandler::HandleGetTips(const ListValue* args) {
  // List containing the tips to be displayed.
  ListValue list_value;

  // Holds the web resource data found in the preferences cache.
  ListValue* wr_list;

  // These values hold the data for each web resource item.
  int current_tip_index;
  std::string current_tip;

  // If tips are not correct for our language, do not send.  Wait for update.
  // We need to check here because the new tab page calls for tips before
  // the tip service starts up.
  PrefService* current_prefs = web_ui_->GetProfile()->GetPrefs();
  if (current_prefs->HasPrefPath(prefs::kNTPTipsResourceServer)) {
    std::string server = current_prefs->GetString(
        prefs::kNTPTipsResourceServer);
    std::string locale = g_browser_process->GetApplicationLocale();
    if (!EndsWith(server, locale, false)) {
      web_ui_->CallJavascriptFunction("tips", list_value);
      return;
    }
  }

  if (tips_cache_ != NULL && !tips_cache_->empty()) {
    if (tips_cache_->GetInteger(
        PromoResourceService::kCurrentTipPrefName, &current_tip_index) &&
        tips_cache_->GetList(
        PromoResourceService::kTipCachePrefName, &wr_list) &&
        wr_list && wr_list->GetSize() > 0) {
      if (wr_list->GetSize() <= static_cast<size_t>(current_tip_index)) {
        // Check to see whether the home page is set to NTP; if not, add tip
        // to set home page before resetting tip index to 0.
        current_tip_index = 0;
        const PrefService::Preference* pref =
            web_ui_->GetProfile()->GetPrefs()->FindPreference(
                prefs::kHomePageIsNewTabPage);
        bool value;
        if (pref && !pref->IsManaged() &&
            pref->GetValue()->GetAsBoolean(&value) && !value) {
          SendTip(l10n_util::GetStringUTF8(IDS_NEW_TAB_MAKE_THIS_HOMEPAGE),
                  "set_homepage_tip", current_tip_index);
          return;
        }
      }
      if (wr_list->GetString(current_tip_index, &current_tip)) {
        SendTip(current_tip, "tip_html_text", current_tip_index + 1);
      }
    }
  }
}

void TipsHandler::SendTip(const std::string& tip, const std::string& tip_type,
                          int tip_index) {
  // List containing the tips to be displayed.
  ListValue list_value;
  DictionaryValue* tip_dict = new DictionaryValue();
  tip_dict->SetString(tip_type, tip);
  list_value.Append(tip_dict);
  tips_cache_->SetInteger(PromoResourceService::kCurrentTipPrefName,
                          tip_index);
  // Send list of web resource items back out to the DOM.
  web_ui_->CallJavascriptFunction("tips", list_value);
}

// static
void TipsHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kNTPPromoResourceCache);
  prefs->RegisterStringPref(prefs::kNTPPromoResourceServer,
                            PromoResourceService::kDefaultPromoResourceServer);
}

bool TipsHandler::IsValidURL(const std::wstring& url_string) {
  GURL url(WideToUTF8(url_string));
  return !url.is_empty() && (url.SchemeIs(chrome::kHttpScheme) ||
                             url.SchemeIs(chrome::kHttpsScheme));
}
