// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/android/context_menu_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/page_transition_types.h"

ContextMenuHandler::ContextMenuHandler()
    : weak_ptr_factory_(this) {
}

ContextMenuHandler::~ContextMenuHandler() {
}

void ContextMenuHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("showContextMenu",
      base::Bind(&ContextMenuHandler::HandleShowContextMenu,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openInNewTab",
      base::Bind(&ContextMenuHandler::HandleOpenInNewTab,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openInIncognitoTab",
      base::Bind(&ContextMenuHandler::HandleOpenInIncognitoTab,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getIncognitoDisabled",
      base::Bind(&ContextMenuHandler::GetIncognitoDisabled,
                 base::Unretained(this)));
}

void ContextMenuHandler::OnItemSelected(int item_id) {
  base::FundamentalValue value(item_id);
  web_ui()->CallJavascriptFunction("ntp.onCustomMenuSelected", value);
}

void ContextMenuHandler::GetIncognitoDisabled(
    const ListValue* args) {
  DictionaryValue value;

  const PrefService* pref = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext())->GetPrefs();

  // Tell to ntp_android.js whether "open in new incognito tab" is allowed or
  // not in different context menus. This property can be disabled by
  // preferences (e.g. via policies).
  int pref_value = pref->GetInteger(prefs::kIncognitoModeAvailability);
  bool incognito_enabled = pref_value != IncognitoModePrefs::DISABLED;
  value.SetBoolean("incognitoEnabled", incognito_enabled);
  web_ui()->CallJavascriptFunction("ntp.setIncognitoEnabled", value);
}

void ContextMenuHandler::HandleShowContextMenu(
    const ListValue* menu_list_values) {
  if (menu_list_values->empty()) {
    LOG(WARNING) << "Ignoring request for empty context menu.";
    return;
  }

  // We expect menu_list_values to be of the form:
  // [ [ 1, "title1" ], [ 2, "title2" ], ...]
  // Where the first value in the sub-array is the item id and the second its
  // title.
  content::ContextMenuParams menu;
  for (size_t i = 0; i < menu_list_values->GetSize(); ++i) {
    ListValue* item_list_value = NULL;
    bool valid_value = menu_list_values->GetList(
        i, const_cast<const ListValue**>(&item_list_value));
    if (!valid_value) {
      LOG(ERROR) << "Invalid context menu request: menu item info " << i <<
          " is not a list.";
      return;
    }

    int id;
    if (!ExtractIntegerValue(item_list_value, &id)) {
      Value* value = NULL;
      item_list_value->Get(0, &value);
      LOG(ERROR) << "Invalid context menu request:  menu item " << i <<
          " expected int value for first parameter (got " <<
          value->GetType() << ").";
      return;
    }

    content::MenuItem menu_item;
    menu_item.action = id;
    if (!item_list_value->GetString(1, &(menu_item.label))) {
      Value* value = NULL;
      item_list_value->Get(1, &value);
      LOG(ERROR) << "Invalid context menu request:  menu item " << i <<
          " expected string value for second parameter (got " <<
          value->GetType() << ").";
      return;
    }
    menu.custom_items.push_back(menu_item);
  }

  TabAndroid* tab = TabAndroid::FromWebContents(web_ui()->GetWebContents());
  if (tab) {
    tab->ShowCustomContextMenu(
        menu,
        base::Bind(&ContextMenuHandler::OnItemSelected,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ContextMenuHandler::HandleOpenInNewTab(const ListValue* args) {
  OpenUrl(args, NEW_FOREGROUND_TAB);
}

void ContextMenuHandler::HandleOpenInIncognitoTab(const ListValue* args) {
  OpenUrl(args, OFF_THE_RECORD);
}

void ContextMenuHandler::OpenUrl(const ListValue* args,
                                 WindowOpenDisposition disposition) {
  string16 url = ExtractStringValue(args);
  if (!url.empty()) {
    web_ui()->GetWebContents()->OpenURL(content::OpenURLParams(
        GURL(url), content::Referrer(), disposition,
        content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  }
}
