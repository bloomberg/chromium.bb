// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_helper.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/session_storage_namespace.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

void RecordAppLaunch(Profile* profile, const TabRestoreService::Tab& tab) {
#if !defined(OS_ANDROID)
  GURL url = tab.navigations.at(tab.current_navigation_index).virtual_url();
  DCHECK(profile->GetExtensionService());
  if (!profile->GetExtensionService()->IsInstalledApp(url))
    return;

  AppLauncherHandler::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_NTP_RECENTLY_CLOSED);
#endif  // !defined(OS_ANDROID)
}

}  // namespace

// TabRestoreServiceHelper::Observer -------------------------------------------

TabRestoreServiceHelper::Observer::~Observer() {}

void TabRestoreServiceHelper::Observer::OnClearEntries() {}

void TabRestoreServiceHelper::Observer::OnRestoreEntryById(
    SessionID::id_type id,
    Entries::const_iterator entry_iterator) {
}

void TabRestoreServiceHelper::Observer::OnAddEntry() {}

// TabRestoreServiceHelper -----------------------------------------------------

TabRestoreServiceHelper::TabRestoreServiceHelper(
    TabRestoreService* tab_restore_service,
    Observer* observer,
    Profile* profile,
    TabRestoreService::TimeFactory* time_factory)
    : tab_restore_service_(tab_restore_service),
      observer_(observer),
      profile_(profile),
      restoring_(false),
      time_factory_(time_factory) {
  DCHECK(tab_restore_service_);
}

TabRestoreServiceHelper::~TabRestoreServiceHelper() {
  FOR_EACH_OBSERVER(TabRestoreServiceObserver, observer_list_,
                    TabRestoreServiceDestroyed(tab_restore_service_));
  STLDeleteElements(&entries_);
}

void TabRestoreServiceHelper::AddObserver(
    TabRestoreServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TabRestoreServiceHelper::RemoveObserver(
    TabRestoreServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TabRestoreServiceHelper::CreateHistoricalTab(
    content::WebContents* contents,
    int index) {
  if (restoring_)
    return;

  TabRestoreServiceDelegate* delegate =
      TabRestoreServiceDelegate::FindDelegateForWebContents(contents);
  if (closing_delegates_.find(delegate) != closing_delegates_.end())
    return;

  scoped_ptr<Tab> local_tab(new Tab());
  PopulateTab(local_tab.get(), index, delegate, &contents->GetController());
  if (local_tab->navigations.empty())
    return;

  AddEntry(local_tab.release(), true, true);
}

void TabRestoreServiceHelper::BrowserClosing(
    TabRestoreServiceDelegate* delegate) {
  closing_delegates_.insert(delegate);

  scoped_ptr<Window> window(new Window());
  window->selected_tab_index = delegate->GetSelectedIndex();
  window->timestamp = TimeNow();
  window->app_name = delegate->GetAppName();

  // Don't use std::vector::resize() because it will push copies of an empty tab
  // into the vector, which will give all tabs in a window the same ID.
  for (int i = 0; i < delegate->GetTabCount(); ++i) {
    window->tabs.push_back(Tab());
  }
  size_t entry_index = 0;
  for (int tab_index = 0; tab_index < delegate->GetTabCount(); ++tab_index) {
    PopulateTab(&(window->tabs[entry_index]),
                tab_index,
                delegate,
                &delegate->GetWebContentsAt(tab_index)->GetController());
    if (window->tabs[entry_index].navigations.empty()) {
      window->tabs.erase(window->tabs.begin() + entry_index);
    } else {
      window->tabs[entry_index].browser_id = delegate->GetSessionID().id();
      entry_index++;
    }
  }
  if (window->tabs.size() == 1 && window->app_name.empty()) {
    // Short-circuit creating a Window if only 1 tab was present. This fixes
    // http://crbug.com/56744. Copy the Tab because it's owned by an object on
    // the stack.
    AddEntry(new Tab(window->tabs[0]), true, true);
  } else if (!window->tabs.empty()) {
    window->selected_tab_index =
        std::min(static_cast<int>(window->tabs.size() - 1),
                 window->selected_tab_index);
    AddEntry(window.release(), true, true);
  }
}

void TabRestoreServiceHelper::BrowserClosed(
    TabRestoreServiceDelegate* delegate) {
  closing_delegates_.erase(delegate);
}

void TabRestoreServiceHelper::ClearEntries() {
  if (observer_)
    observer_->OnClearEntries();
  STLDeleteElements(&entries_);
  NotifyTabsChanged();
}

const TabRestoreService::Entries& TabRestoreServiceHelper::entries() const {
  return entries_;
}

void TabRestoreServiceHelper::RestoreMostRecentEntry(
    TabRestoreServiceDelegate* delegate,
    chrome::HostDesktopType host_desktop_type) {
  if (entries_.empty())
    return;

  RestoreEntryById(delegate, entries_.front()->id, host_desktop_type, UNKNOWN);
}

TabRestoreService::Tab* TabRestoreServiceHelper::RemoveTabEntryById(
    SessionID::id_type id) {
  Entries::iterator i = GetEntryIteratorById(id);
  if (i == entries_.end())
    return NULL;

  Entry* entry = *i;
  if (entry->type != TabRestoreService::TAB)
    return NULL;

  Tab* tab = static_cast<Tab*>(entry);
  entries_.erase(i);
  return tab;
}

void TabRestoreServiceHelper::RestoreEntryById(
    TabRestoreServiceDelegate* delegate,
    SessionID::id_type id,
    chrome::HostDesktopType host_desktop_type,
    WindowOpenDisposition disposition) {
  Entries::iterator entry_iterator = GetEntryIteratorById(id);
  if (entry_iterator == entries_.end())
    // Don't hoark here, we allow an invalid id.
    return;

  if (observer_)
    observer_->OnRestoreEntryById(id, entry_iterator);
  restoring_ = true;
  Entry* entry = *entry_iterator;

  // If the entry's ID does not match the ID that is being restored, then the
  // entry is a window from which a single tab will be restored.
  bool restoring_tab_in_window = entry->id != id;

  if (!restoring_tab_in_window) {
    entries_.erase(entry_iterator);
    entry_iterator = entries_.end();
  }

  // |delegate| will be NULL in cases where one isn't already available (eg,
  // when invoked on Mac OS X with no windows open). In this case, create a
  // new browser into which we restore the tabs.
  if (entry->type == TabRestoreService::TAB) {
    Tab* tab = static_cast<Tab*>(entry);
    delegate = RestoreTab(*tab, delegate, host_desktop_type, disposition);
    delegate->ShowBrowserWindow();
  } else if (entry->type == TabRestoreService::WINDOW) {
    TabRestoreServiceDelegate* current_delegate = delegate;
    Window* window = static_cast<Window*>(entry);

    // When restoring a window, either the entire window can be restored, or a
    // single tab within it. If the entry's ID matches the one to restore, then
    // the entire window will be restored.
    if (!restoring_tab_in_window) {
      delegate = TabRestoreServiceDelegate::Create(profile_, host_desktop_type,
                                                   window->app_name);
      for (size_t tab_i = 0; tab_i < window->tabs.size(); ++tab_i) {
        const Tab& tab = window->tabs[tab_i];
        WebContents* restored_tab =
            delegate->AddRestoredTab(tab.navigations, delegate->GetTabCount(),
                                     tab.current_navigation_index,
                                     tab.extension_app_id,
                                     static_cast<int>(tab_i) ==
                                         window->selected_tab_index,
                                     tab.pinned, tab.from_last_session,
                                     tab.session_storage_namespace,
                                     tab.user_agent_override);
        if (restored_tab) {
          restored_tab->GetController().LoadIfNecessary();
          RecordAppLaunch(profile_, tab);
        }
      }
      // All the window's tabs had the same former browser_id.
      if (window->tabs[0].has_browser()) {
        UpdateTabBrowserIDs(window->tabs[0].browser_id,
                            delegate->GetSessionID().id());
      }
    } else {
      // Restore a single tab from the window. Find the tab that matches the ID
      // in the window and restore it.
      for (std::vector<Tab>::iterator tab_i = window->tabs.begin();
           tab_i != window->tabs.end(); ++tab_i) {
        const Tab& tab = *tab_i;
        if (tab.id == id) {
          delegate = RestoreTab(tab, delegate, host_desktop_type, disposition);
          window->tabs.erase(tab_i);
          // If restoring the tab leaves the window with nothing else, delete it
          // as well.
          if (!window->tabs.size()) {
            entries_.erase(entry_iterator);
            delete entry;
          } else {
            // Update the browser ID of the rest of the tabs in the window so if
            // any one is restored, it goes into the same window as the tab
            // being restored now.
            UpdateTabBrowserIDs(tab.browser_id,
                                delegate->GetSessionID().id());
            for (std::vector<Tab>::iterator tab_j = window->tabs.begin();
                 tab_j != window->tabs.end(); ++tab_j) {
              (*tab_j).browser_id = delegate->GetSessionID().id();
            }
          }
          break;
        }
      }
    }
    delegate->ShowBrowserWindow();

    if (disposition == CURRENT_TAB && current_delegate &&
        current_delegate->GetActiveWebContents()) {
      current_delegate->CloseTab();
    }
  } else {
    NOTREACHED();
  }

  if (!restoring_tab_in_window) {
    delete entry;
  }

  restoring_ = false;
  NotifyTabsChanged();
}

void TabRestoreServiceHelper::NotifyTabsChanged() {
  FOR_EACH_OBSERVER(TabRestoreServiceObserver, observer_list_,
                    TabRestoreServiceChanged(tab_restore_service_));
}

void TabRestoreServiceHelper::AddEntry(Entry* entry,
                                       bool notify,
                                       bool to_front) {
  if (!FilterEntry(entry) || (entries_.size() >= kMaxEntries && !to_front)) {
    delete entry;
    return;
  }

  if (to_front)
    entries_.push_front(entry);
  else
    entries_.push_back(entry);

  PruneEntries();

  if (notify)
    NotifyTabsChanged();

  if (observer_)
    observer_->OnAddEntry();
}

void TabRestoreServiceHelper::PruneEntries() {
  Entries new_entries;

  for (TabRestoreService::Entries::const_iterator iter = entries_.begin();
       iter != entries_.end(); ++iter) {
    TabRestoreService::Entry* entry = *iter;

    if (FilterEntry(entry) &&
        new_entries.size() < kMaxEntries) {
      new_entries.push_back(entry);
    } else {
      delete entry;
    }
  }

  entries_ = new_entries;
}

TabRestoreService::Entries::iterator
TabRestoreServiceHelper::GetEntryIteratorById(SessionID::id_type id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    if ((*i)->id == id)
      return i;

    // For Window entries, see if the ID matches a tab. If so, report the window
    // as the Entry.
    if ((*i)->type == TabRestoreService::WINDOW) {
      std::vector<Tab>& tabs = static_cast<Window*>(*i)->tabs;
      for (std::vector<Tab>::iterator j = tabs.begin();
           j != tabs.end(); ++j) {
        if ((*j).id == id) {
          return i;
        }
      }
    }
  }
  return entries_.end();
}

// static
bool TabRestoreServiceHelper::ValidateEntry(Entry* entry) {
  if (entry->type == TabRestoreService::TAB)
    return ValidateTab(static_cast<Tab*>(entry));

  if (entry->type == TabRestoreService::WINDOW)
    return ValidateWindow(static_cast<Window*>(entry));

  NOTREACHED();
  return false;
}

void TabRestoreServiceHelper::PopulateTab(
    Tab* tab,
    int index,
    TabRestoreServiceDelegate* delegate,
    NavigationController* controller) {
  const int pending_index = controller->GetPendingEntryIndex();
  int entry_count = controller->GetEntryCount();
  if (entry_count == 0 && pending_index == 0)
    entry_count++;
  tab->navigations.resize(static_cast<int>(entry_count));
  for (int i = 0; i < entry_count; ++i) {
    NavigationEntry* entry = (i == pending_index) ?
        controller->GetPendingEntry() : controller->GetEntryAtIndex(i);
    tab->navigations[i] =
        TabNavigation::FromNavigationEntry(i, *entry);
  }
  tab->timestamp = TimeNow();
  tab->current_navigation_index = controller->GetCurrentEntryIndex();
  if (tab->current_navigation_index == -1 && entry_count > 0)
    tab->current_navigation_index = 0;
  tab->tabstrip_index = index;

  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(controller->GetWebContents());
  // extensions_tab_helper is NULL in some browser tests.
  if (extensions_tab_helper) {
    const extensions::Extension* extension =
        extensions_tab_helper->extension_app();
    if (extension)
      tab->extension_app_id = extension->id();
  }

  tab->user_agent_override =
      controller->GetWebContents()->GetUserAgentOverride();

  // TODO(ajwong): This does not correctly handle storage for isolated apps.
  tab->session_storage_namespace =
      controller->GetDefaultSessionStorageNamespace();

  // Delegate may be NULL during unit tests.
  if (delegate) {
    tab->browser_id = delegate->GetSessionID().id();
    tab->pinned = delegate->IsTabPinned(tab->tabstrip_index);
  }
}

TabRestoreServiceDelegate* TabRestoreServiceHelper::RestoreTab(
    const Tab& tab,
    TabRestoreServiceDelegate* delegate,
    chrome::HostDesktopType host_desktop_type,
    WindowOpenDisposition disposition) {
  if (disposition == CURRENT_TAB && delegate) {
    delegate->ReplaceRestoredTab(tab.navigations,
                                 tab.current_navigation_index,
                                 tab.from_last_session,
                                 tab.extension_app_id,
                                 tab.session_storage_namespace,
                                 tab.user_agent_override);
  } else {
    // We only respsect the tab's original browser if there's no disposition.
    if (disposition == UNKNOWN && tab.has_browser()) {
      delegate = TabRestoreServiceDelegate::FindDelegateWithID(
                     tab.browser_id, host_desktop_type);
    }

    int tab_index = -1;

    // |delegate| will be NULL in cases where one isn't already available (eg,
    // when invoked on Mac OS X with no windows open). In this case, create a
    // new browser into which we restore the tabs.
    if (delegate && disposition != NEW_WINDOW) {
      tab_index = tab.tabstrip_index;
    } else {
      delegate = TabRestoreServiceDelegate::Create(profile_, host_desktop_type,
                                                   std::string());
      if (tab.has_browser())
        UpdateTabBrowserIDs(tab.browser_id, delegate->GetSessionID().id());
    }

    // Place the tab at the end if the tab index is no longer valid or
    // we were passed a specific disposition.
    if (tab_index < 0 || tab_index > delegate->GetTabCount() ||
        disposition != UNKNOWN) {
      tab_index = delegate->GetTabCount();
    }

    WebContents* web_contents = delegate->AddRestoredTab(
        tab.navigations,
        tab_index,
        tab.current_navigation_index,
        tab.extension_app_id,
        disposition != NEW_BACKGROUND_TAB,
        tab.pinned,
        tab.from_last_session,
        tab.session_storage_namespace,
        tab.user_agent_override);
    web_contents->GetController().LoadIfNecessary();
  }
  RecordAppLaunch(profile_, tab);
  return delegate;
}


bool TabRestoreServiceHelper::ValidateTab(Tab* tab) {
  if (tab->navigations.empty())
    return false;

  tab->current_navigation_index =
      std::max(0, std::min(tab->current_navigation_index,
                           static_cast<int>(tab->navigations.size()) - 1));

  return true;
}

bool TabRestoreServiceHelper::ValidateWindow(Window* window) {
  window->selected_tab_index =
      std::max(0, std::min(window->selected_tab_index,
                           static_cast<int>(window->tabs.size() - 1)));

  int i = 0;
  for (std::vector<Tab>::iterator tab_i = window->tabs.begin();
       tab_i != window->tabs.end();) {
    if (!ValidateTab(&(*tab_i))) {
      tab_i = window->tabs.erase(tab_i);
      if (i < window->selected_tab_index)
        window->selected_tab_index--;
      else if (i == window->selected_tab_index)
        window->selected_tab_index = 0;
    } else {
      ++tab_i;
      ++i;
    }
  }

  if (window->tabs.empty())
    return false;

  return true;
}

bool TabRestoreServiceHelper::IsTabInteresting(const Tab* tab) {
  if (tab->navigations.empty())
    return false;

  if (tab->navigations.size() > 1)
    return true;

  return tab->pinned ||
      tab->navigations.at(0).virtual_url() !=
          GURL(chrome::kChromeUINewTabURL);
}

bool TabRestoreServiceHelper::IsWindowInteresting(const Window* window) {
  if (window->tabs.empty())
    return false;

  if (window->tabs.size() > 1)
    return true;

  return IsTabInteresting(&window->tabs[0]);
}

bool TabRestoreServiceHelper::FilterEntry(Entry* entry) {
  if (!ValidateEntry(entry))
    return false;

  if (entry->type == TabRestoreService::TAB)
    return IsTabInteresting(static_cast<Tab*>(entry));
  else if (entry->type == TabRestoreService::WINDOW)
    return IsWindowInteresting(static_cast<Window*>(entry));

  NOTREACHED();
  return false;
}

void TabRestoreServiceHelper::UpdateTabBrowserIDs(SessionID::id_type old_id,
                                                  SessionID::id_type new_id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    Entry* entry = *i;
    if (entry->type == TabRestoreService::TAB) {
      Tab* tab = static_cast<Tab*>(entry);
      if (tab->browser_id == old_id)
        tab->browser_id = new_id;
    }
  }
}

base::Time TabRestoreServiceHelper::TimeNow() const {
  return time_factory_ ? time_factory_->TimeNow() : base::Time::Now();
}
