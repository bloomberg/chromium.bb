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
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_ANDROID)
#include "chrome/browser/sessions/session_restore.h"
#endif

namespace {

void TabToValue(const TabRestoreService::Tab& tab,
                DictionaryValue* dictionary) {
  const TabNavigation& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  NewTabUI::SetUrlTitleAndDirection(dictionary, current_navigation.title(),
                                    current_navigation.virtual_url());
  dictionary->SetString("type", "tab");
  dictionary->SetDouble("timestamp", tab.timestamp.ToDoubleT());
}

void WindowToValue(const TabRestoreService::Window& window,
                   DictionaryValue* dictionary) {
  DCHECK(!window.tabs.empty());

  scoped_ptr<ListValue> tab_values(new ListValue());
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    DictionaryValue* tab_value = new DictionaryValue();
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

void RecentlyClosedTabsHandler::HandleReopenTab(const ListValue* args) {
  if (!tab_restore_service_)
    return;

  double session_to_restore = 0.0;
  CHECK(args->GetDouble(0, &session_to_restore));

#if defined(OS_ANDROID)
  // Find and remove the corresponding tab entry from TabRestoreService.
  // We take ownership of the returned tab.
  scoped_ptr<TabRestoreService::Tab> tab_entry(
      tab_restore_service_->RemoveTabEntryById(static_cast<int>(
          session_to_restore)));
  if (tab_entry.get() == NULL)
    return;

  // RestoreForeignSessionTab needs a SessionTab.
  SessionTab session_tab;
  session_tab.current_navigation_index = tab_entry->current_navigation_index;
  session_tab.navigations = tab_entry->navigations;

  SessionRestore::RestoreForeignSessionTab(web_ui()->GetWebContents(),
                                           session_tab, NEW_FOREGROUND_TAB);
#else
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
  WindowOpenDisposition disposition =
      web_ui_util::GetDispositionFromClick(args, 2);
  tab_restore_service_->RestoreEntryById(delegate,
                                         static_cast<int>(session_to_restore),
                                         host_desktop_type,
                                         disposition);
  // The current tab has been nuked at this point; don't touch any member
  // variables.
#endif
}

void RecentlyClosedTabsHandler::HandleClearRecentlyClosed(
    const ListValue* args) {
  EnsureTabRestoreService();
  if (tab_restore_service_)
    tab_restore_service_->ClearEntries();
}

void RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs(
    const ListValue* args) {
  EnsureTabRestoreService();
  if (tab_restore_service_)
    TabRestoreServiceChanged(tab_restore_service_);
}

void RecentlyClosedTabsHandler::TabRestoreServiceChanged(
    TabRestoreService* service) {
  ListValue list_value;
  TabRestoreService::Entries entries = service->entries();
  CreateRecentlyClosedValues(entries, &list_value);

  web_ui()->CallJavascriptFunction("ntp.setRecentlyClosedTabs", list_value);
}

void RecentlyClosedTabsHandler::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

// static
void RecentlyClosedTabsHandler::CreateRecentlyClosedValues(
    const TabRestoreService::Entries& entries, ListValue* entry_list_value) {
  const int max_count = 10;
  int added_count = 0;
  // We filter the list of recently closed to only show 'interesting' entries,
  // where an interesting entry is either a closed window or a closed tab
  // whose selected navigation is not the new tab ui.
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end() && added_count < max_count; ++it) {
    TabRestoreService::Entry* entry = *it;
    scoped_ptr<DictionaryValue> entry_dict(new DictionaryValue());
    if (entry->type == TabRestoreService::TAB) {
      TabToValue(*static_cast<TabRestoreService::Tab*>(entry),
                 entry_dict.get());
    } else  {
      DCHECK_EQ(entry->type, TabRestoreService::WINDOW);
      WindowToValue(*static_cast<TabRestoreService::Window*>(entry),
                    entry_dict.get());
    }

    entry_dict->SetInteger("sessionId", entry->id);
    entry_list_value->Append(entry_dict.release());
    added_count++;
  }
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
