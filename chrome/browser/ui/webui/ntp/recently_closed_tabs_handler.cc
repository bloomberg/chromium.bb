// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/recently_closed_tabs_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

void TabToValue(const TabRestoreService::Tab& tab,
                base::DictionaryValue* dictionary) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  NewTabUI::SetUrlTitleAndDirection(dictionary, current_navigation.title(),
                                    current_navigation.virtual_url());
  dictionary->SetString("type", "tab");
  dictionary->SetDouble("timestamp", tab.timestamp.ToDoubleT());
}

void WindowToValue(const TabRestoreService::Window& window,
                   base::DictionaryValue* dictionary) {
  DCHECK(!window.tabs.empty());

  scoped_ptr<base::ListValue> tab_values(new base::ListValue());
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    base::DictionaryValue* tab_value = new base::DictionaryValue();
    TabToValue(window.tabs[i], tab_value);
    tab_values->Append(tab_value);
  }

  dictionary->SetString("type", "window");
  dictionary->SetDouble("timestamp", window.timestamp.ToDoubleT());
  dictionary->Set("tabs", tab_values.release());
}

}  // namespace

void RecentlyClosedTabsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getRecentlyClosedTabs",
      base::Bind(&RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("reopenTab",
      base::Bind(&RecentlyClosedTabsHandler::HandleReopenTab,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("clearRecentlyClosed",
      base::Bind(&RecentlyClosedTabsHandler::HandleClearRecentlyClosed,
                 base::Unretained(this)));
}

RecentlyClosedTabsHandler::~RecentlyClosedTabsHandler() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void RecentlyClosedTabsHandler::HandleReopenTab(const base::ListValue* args) {
  if (!tab_restore_service_)
    return;

  double session_to_restore = 0.0;
  CHECK(args->GetDouble(0, &session_to_restore));

  double index = -1.0;
  CHECK(args->GetDouble(1, &index));

  // There are actually less than 20 restore tab items displayed in the UI.
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.SessionRestore",
                            static_cast<int>(index), 20);

  TabRestoreServiceDelegate* delegate =
      TabRestoreServiceDelegate::FindDelegateForWebContents(
          web_ui()->GetWebContents());
  if (!delegate)
    return;
  chrome::HostDesktopType host_desktop_type =
      chrome::GetHostDesktopTypeForNativeView(
          web_ui()->GetWebContents()->GetNativeView());
  WindowOpenDisposition disposition = webui::GetDispositionFromClick(args, 2);
  tab_restore_service_->RestoreEntryById(delegate,
                                         static_cast<int>(session_to_restore),
                                         host_desktop_type,
                                         disposition);
  // The current tab has been nuked at this point; don't touch any member
  // variables.
}

void RecentlyClosedTabsHandler::HandleClearRecentlyClosed(
    const base::ListValue* args) {
  EnsureTabRestoreService();
  if (tab_restore_service_)
    tab_restore_service_->ClearEntries();
}

void RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs(
    const base::ListValue* args) {
  EnsureTabRestoreService();
  if (tab_restore_service_)
    TabRestoreServiceChanged(tab_restore_service_);
}

void RecentlyClosedTabsHandler::TabRestoreServiceChanged(
    TabRestoreService* service) {
  base::ListValue list_value;
  const int max_count = 10;
  int added_count = 0;
  // We filter the list of recently closed to only show 'interesting' entries,
  // where an interesting entry is either a closed window or a closed tab
  // whose selected navigation is not the new tab ui.
  for (TabRestoreService::Entries::const_iterator it =
           service->entries().begin();
       it != service->entries().end() && added_count < max_count; ++it) {
    TabRestoreService::Entry* entry = *it;
    scoped_ptr<base::DictionaryValue> entry_dict(new base::DictionaryValue());
    if (entry->type == TabRestoreService::TAB) {
      TabToValue(*static_cast<TabRestoreService::Tab*>(entry),
                 entry_dict.get());
    } else  {
      DCHECK_EQ(entry->type, TabRestoreService::WINDOW);
      WindowToValue(*static_cast<TabRestoreService::Window*>(entry),
                    entry_dict.get());
    }

    entry_dict->SetInteger("sessionId", entry->id);
    list_value.Append(entry_dict.release());
    ++added_count;
  }

  web_ui()->CallJavascriptFunction("ntp.setRecentlyClosedTabs", list_value);
}

void RecentlyClosedTabsHandler::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

void RecentlyClosedTabsHandler::EnsureTabRestoreService() {
  if (tab_restore_service_)
    return;

  tab_restore_service_ =
      TabRestoreServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));

  // TabRestoreServiceFactory::GetForProfile() can return NULL (i.e., when in
  // Off the Record mode)
  if (tab_restore_service_) {
    // This does nothing if the tabs have already been loaded or they
    // shouldn't be loaded.
    tab_restore_service_->LoadTabsFromLastSession();
    tab_restore_service_->AddObserver(this);
  }
}
