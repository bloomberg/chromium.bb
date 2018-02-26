// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service_helper.h"

#include <inttypes.h>
#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "components/sessions/core/tab_restore_service_observer.h"

namespace sessions {

// TabRestoreServiceHelper::Observer -------------------------------------------

TabRestoreServiceHelper::Observer::~Observer() {}

void TabRestoreServiceHelper::Observer::OnClearEntries() {}

void TabRestoreServiceHelper::Observer::OnNavigationEntriesDeleted() {}

void TabRestoreServiceHelper::Observer::OnRestoreEntryById(
    SessionID::id_type id,
    Entries::const_iterator entry_iterator) {
}

void TabRestoreServiceHelper::Observer::OnAddEntry() {}

// TabRestoreServiceHelper -----------------------------------------------------

TabRestoreServiceHelper::TabRestoreServiceHelper(
    TabRestoreService* tab_restore_service,
    Observer* observer,
    TabRestoreServiceClient* client,
    TabRestoreService::TimeFactory* time_factory)
    : tab_restore_service_(tab_restore_service),
      observer_(observer),
      client_(client),
      restoring_(false),
      time_factory_(time_factory) {
  DCHECK(tab_restore_service_);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this,
      "TabRestoreServiceHelper",
      base::ThreadTaskRunnerHandle::Get());
}

TabRestoreServiceHelper::~TabRestoreServiceHelper() {
  for (auto& observer : observer_list_)
    observer.TabRestoreServiceDestroyed(tab_restore_service_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void TabRestoreServiceHelper::AddObserver(
    TabRestoreServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TabRestoreServiceHelper::RemoveObserver(
    TabRestoreServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TabRestoreServiceHelper::CreateHistoricalTab(LiveTab* live_tab,
                                                  int index) {
  if (restoring_)
    return;

  // If an entire window is being closed than all of the tabs have already
  // been persisted via "BrowserClosing". Ignore the subsequent tab closing
  // notifications.
  LiveTabContext* context = client_->FindLiveTabContextForTab(live_tab);
  if (closing_contexts_.find(context) != closing_contexts_.end())
    return;

  auto local_tab = std::make_unique<Tab>();
  PopulateTab(local_tab.get(), index, context, live_tab);
  if (local_tab->navigations.empty())
    return;

  AddEntry(std::move(local_tab), true, true);
}

void TabRestoreServiceHelper::BrowserClosing(LiveTabContext* context) {
  closing_contexts_.insert(context);

  auto window = std::make_unique<Window>();
  window->selected_tab_index = context->GetSelectedIndex();
  window->timestamp = TimeNow();
  window->app_name = context->GetAppName();
  window->bounds = context->GetRestoredBounds();
  window->show_state = context->GetRestoredState();
  window->workspace = context->GetWorkspace();

  for (int tab_index = 0; tab_index < context->GetTabCount(); ++tab_index) {
    auto tab = std::make_unique<Tab>();
    PopulateTab(tab.get(), tab_index, context,
                context->GetLiveTabAt(tab_index));
    if (!tab->navigations.empty()) {
      tab->browser_id = context->GetSessionID().id();
      window->tabs.push_back(std::move(tab));
    }
  }

  if (window->tabs.size() == 1 && window->app_name.empty()) {
    // Short-circuit creating a Window if only 1 tab was present. This fixes
    // http://crbug.com/56744.
    AddEntry(std::move(window->tabs[0]), true, true);
  } else if (!window->tabs.empty()) {
    window->selected_tab_index = std::min(
        static_cast<int>(window->tabs.size() - 1), window->selected_tab_index);
    AddEntry(std::move(window), true, true);
  }
}

void TabRestoreServiceHelper::BrowserClosed(LiveTabContext* context) {
  closing_contexts_.erase(context);
}

void TabRestoreServiceHelper::ClearEntries() {
  if (observer_)
    observer_->OnClearEntries();
  entries_.clear();
  NotifyTabsChanged();
}

bool TabRestoreServiceHelper::DeleteFromTab(const DeletionPredicate& predicate,
                                            Tab* tab) {
  std::vector<SerializedNavigationEntry> new_navigations;
  int deleted_navigations = 0;
  for (auto& navigation : tab->navigations) {
    if (predicate.Run(navigation)) {
      // If the current navigation is deleted, remove this tab.
      if (tab->current_navigation_index == navigation.index())
        return true;
      deleted_navigations++;
    } else {
      // Adjust indices according to number of deleted navigations.
      if (tab->current_navigation_index == navigation.index())
        tab->current_navigation_index -= deleted_navigations;
      navigation.set_index(navigation.index() - deleted_navigations);
      new_navigations.push_back(std::move(navigation));
    }
  }
  tab->navigations = std::move(new_navigations);
  DCHECK(tab->navigations.empty() || ValidateTab(*tab));
  return tab->navigations.empty();
}

bool TabRestoreServiceHelper::DeleteFromWindow(
    const DeletionPredicate& predicate,
    Window* window) {
  std::vector<std::unique_ptr<Tab>> new_tabs;
  int deleted_tabs = 0;
  for (auto& tab : window->tabs) {
    if (DeleteFromTab(predicate, tab.get())) {
      if (window->tabs[window->selected_tab_index] == tab)
        window->selected_tab_index = 0;
      deleted_tabs++;
    } else {
      // Adjust indices according to number of deleted tabs.
      if (window->tabs[window->selected_tab_index] == tab)
        window->selected_tab_index -= deleted_tabs;
      if (tab->tabstrip_index >= 0)
        tab->tabstrip_index -= deleted_tabs;
      new_tabs.push_back(std::move(tab));
    }
  }
  window->tabs = std::move(new_tabs);
  DCHECK(window->tabs.empty() || ValidateWindow(*window));
  return window->tabs.empty();
}

void TabRestoreServiceHelper::DeleteNavigationEntries(
    const DeletionPredicate& predicate) {
  Entries new_entries;
  for (std::unique_ptr<Entry>& entry : entries_) {
    switch (entry->type) {
      case TabRestoreService::TAB: {
        Tab* tab = static_cast<Tab*>(entry.get());
        if (!DeleteFromTab(predicate, tab))
          new_entries.push_back(std::move(entry));
        break;
      }
      case TabRestoreService::WINDOW: {
        Window* window = static_cast<Window*>(entry.get());
        if (!DeleteFromWindow(predicate, window)) {
          // If only a single tab is left, just keep the tab.
          if (window->tabs.size() == 1U)
            new_entries.push_back(std::move(window->tabs.front()));
          else
            new_entries.push_back(std::move(entry));
        }
        break;
      }
    }
  }
  entries_ = std::move(new_entries);
  if (observer_)
    observer_->OnNavigationEntriesDeleted();
  NotifyTabsChanged();
}

const TabRestoreService::Entries& TabRestoreServiceHelper::entries() const {
  return entries_;
}

std::vector<LiveTab*> TabRestoreServiceHelper::RestoreMostRecentEntry(
    LiveTabContext* context) {
  if (entries_.empty())
    return std::vector<LiveTab*>();
  return RestoreEntryById(context, entries_.front()->id,
                          WindowOpenDisposition::UNKNOWN);
}

std::unique_ptr<TabRestoreService::Tab>
TabRestoreServiceHelper::RemoveTabEntryById(SessionID::id_type id) {
  auto it = GetEntryIteratorById(id);
  if (it == entries_.end())
    return nullptr;

  if ((*it)->type != TabRestoreService::TAB)
    return nullptr;

  auto tab = std::unique_ptr<Tab>(static_cast<Tab*>(it->release()));
  entries_.erase(it);
  return tab;
}

std::vector<LiveTab*> TabRestoreServiceHelper::RestoreEntryById(
    LiveTabContext* context,
    SessionID::id_type id,
    WindowOpenDisposition disposition) {
  Entries::iterator entry_iterator = GetEntryIteratorById(id);
  if (entry_iterator == entries_.end()) {
    // Don't hoark here, we allow an invalid id.
    return std::vector<LiveTab*>();
  }

  if (observer_)
    observer_->OnRestoreEntryById(id, entry_iterator);
  restoring_ = true;
  auto& entry = **entry_iterator;

  // If the entry's ID does not match the ID that is being restored, then the
  // entry is a window from which a single tab will be restored.
  bool restoring_tab_in_window = entry.id != id;

  // |context| will be NULL in cases where one isn't already available (eg,
  // when invoked on Mac OS X with no windows open). In this case, create a
  // new browser into which we restore the tabs.
  std::vector<LiveTab*> live_tabs;
  switch (entry.type) {
    case TabRestoreService::TAB: {
      auto& tab = static_cast<const Tab&>(entry);
      LiveTab* restored_tab = nullptr;
      context = RestoreTab(tab, context, disposition, &restored_tab);
      live_tabs.push_back(restored_tab);
      context->ShowBrowserWindow();
      break;
    }
    case TabRestoreService::WINDOW: {
      LiveTabContext* current_context = context;
      auto& window = static_cast<Window&>(entry);

      // When restoring a window, either the entire window can be restored, or a
      // single tab within it. If the entry's ID matches the one to restore,
      // then the entire window will be restored.
      if (!restoring_tab_in_window) {
        context =
            client_->CreateLiveTabContext(window.app_name, window.bounds,
                                          window.show_state, window.workspace);
        for (size_t tab_i = 0; tab_i < window.tabs.size(); ++tab_i) {
          const Tab& tab = *window.tabs[tab_i];
          LiveTab* restored_tab = context->AddRestoredTab(
              tab.navigations, context->GetTabCount(),
              tab.current_navigation_index, tab.extension_app_id,
              static_cast<int>(tab_i) == window.selected_tab_index, tab.pinned,
              tab.from_last_session, tab.platform_data.get(),
              tab.user_agent_override);
          if (restored_tab) {
            client_->OnTabRestored(
                tab.navigations.at(tab.current_navigation_index).virtual_url());
            live_tabs.push_back(restored_tab);
          }
        }
        // All the window's tabs had the same former browser_id.
        if (auto browser_id = window.tabs[0]->browser_id) {
          UpdateTabBrowserIDs(browser_id, context->GetSessionID().id());
        }
      } else {
        // Restore a single tab from the window. Find the tab that matches the
        // ID in the window and restore it.
        for (auto tab_i = window.tabs.begin(); tab_i != window.tabs.end();
             ++tab_i) {
          SessionID::id_type restored_tab_browser_id;
          {
            const Tab& tab = **tab_i;
            if (tab.id != id)
              continue;

            restored_tab_browser_id = tab.browser_id;
            LiveTab* restored_tab = nullptr;
            context = RestoreTab(tab, context, disposition, &restored_tab);
            live_tabs.push_back(restored_tab);
            window.tabs.erase(tab_i);
          }
          // If restoring the tab leaves the window with nothing else, delete it
          // as well.
          if (window.tabs.empty()) {
            entries_.erase(entry_iterator);
          } else {
            // Update the browser ID of the rest of the tabs in the window so if
            // any one is restored, it goes into the same window as the tab
            // being restored now.
            UpdateTabBrowserIDs(restored_tab_browser_id,
                                context->GetSessionID().id());
            for (auto& tab_j : window.tabs)
              tab_j->browser_id = context->GetSessionID().id();
          }
          break;
        }
      }
      context->ShowBrowserWindow();

      if (disposition == WindowOpenDisposition::CURRENT_TAB &&
          current_context && current_context->GetActiveLiveTab()) {
        current_context->CloseTab();
      }
      break;
    }
  }

  if (!restoring_tab_in_window) {
    entries_.erase(entry_iterator);
  }

  restoring_ = false;
  NotifyTabsChanged();
  return live_tabs;
}

bool TabRestoreServiceHelper::IsRestoring() const {
  return restoring_;
}

void TabRestoreServiceHelper::NotifyTabsChanged() {
  for (auto& observer : observer_list_)
    observer.TabRestoreServiceChanged(tab_restore_service_);
}

void TabRestoreServiceHelper::NotifyLoaded() {
  for (auto& observer : observer_list_)
    observer.TabRestoreServiceLoaded(tab_restore_service_);
}

void TabRestoreServiceHelper::AddEntry(std::unique_ptr<Entry> entry,
                                       bool notify,
                                       bool to_front) {
  if (!FilterEntry(*entry) || (entries_.size() >= kMaxEntries && !to_front)) {
    return;
  }

  if (to_front)
    entries_.push_front(std::move(entry));
  else
    entries_.push_back(std::move(entry));

  PruneEntries();

  if (notify)
    NotifyTabsChanged();

  if (observer_)
    observer_->OnAddEntry();
}

void TabRestoreServiceHelper::PruneEntries() {
  Entries new_entries;

  for (auto& entry : entries_) {
    if (FilterEntry(*entry) && new_entries.size() < kMaxEntries) {
      new_entries.push_back(std::move(entry));
    }
  }

  entries_ = std::move(new_entries);
}

TabRestoreService::Entries::iterator
TabRestoreServiceHelper::GetEntryIteratorById(SessionID::id_type id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    if ((*i)->id == id)
      return i;

    // For Window entries, see if the ID matches a tab. If so, report the window
    // as the Entry.
    if ((*i)->type == TabRestoreService::WINDOW) {
      auto& window = static_cast<const Window&>(**i);
      for (const auto& tab : window.tabs) {
        if (tab->id == id) {
          return i;
        }
      }
    }
  }
  return entries_.end();
}

bool TabRestoreServiceHelper::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;

  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();

  if (entries_.empty()) {
    // Nothing to report
    return true;
  }

  std::string entries_dump_name = base::StringPrintf(
      "tab_restore/service_helper_0x%" PRIXPTR "/entries",
      reinterpret_cast<uintptr_t>(this));
  pmd->CreateAllocatorDump(entries_dump_name)
      ->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                  MemoryAllocatorDump::kUnitsObjects,
                  entries_.size());

  for (const auto& entry : entries_) {
    const char* type_string = "";
    switch (entry->type) {
      case TabRestoreService::WINDOW:
        type_string = "window";
        break;
      case TabRestoreService::TAB:
        type_string = "tab";
        break;
    }

    std::string entry_dump_name = base::StringPrintf(
        "%s/%s_0x%" PRIXPTR,
        entries_dump_name.c_str(),
        type_string,
        reinterpret_cast<uintptr_t>(entry.get()));
    auto* entry_dump = pmd->CreateAllocatorDump(entry_dump_name);

    entry_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes,
                          entry->EstimateMemoryUsage());

    auto age = base::Time::Now() - entry->timestamp;
    entry_dump->AddScalar("age",
                          MemoryAllocatorDump::kUnitsObjects,
                          age.InSeconds());

    if (system_allocator_name)
      pmd->AddSuballocation(entry_dump->guid(), system_allocator_name);
  }

  return true;
}

// static
bool TabRestoreServiceHelper::ValidateEntry(const Entry& entry) {
  switch (entry.type) {
    case TabRestoreService::TAB:
      return ValidateTab(static_cast<const Tab&>(entry));
    case TabRestoreService::WINDOW:
      return ValidateWindow(static_cast<const Window&>(entry));
  }
  NOTREACHED();
  return false;
}

void TabRestoreServiceHelper::PopulateTab(Tab* tab,
                                          int index,
                                          LiveTabContext* context,
                                          LiveTab* live_tab) {
  int entry_count =
      live_tab->IsInitialBlankNavigation() ? 0 : live_tab->GetEntryCount();
  tab->navigations.resize(static_cast<int>(entry_count));
  for (int i = 0; i < entry_count; ++i) {
    SerializedNavigationEntry entry = live_tab->GetEntryAtIndex(i);
    tab->navigations[i] = entry;
  }
  tab->timestamp = TimeNow();
  tab->current_navigation_index = live_tab->GetCurrentEntryIndex();
  tab->tabstrip_index = index;

  tab->extension_app_id = client_->GetExtensionAppIDForTab(live_tab);

  tab->user_agent_override = live_tab->GetUserAgentOverride();

  tab->platform_data = live_tab->GetPlatformSpecificTabData();

  // Delegate may be NULL during unit tests.
  if (context) {
    tab->browser_id = context->GetSessionID().id();
    tab->pinned = context->IsTabPinned(tab->tabstrip_index);
  }
}

LiveTabContext* TabRestoreServiceHelper::RestoreTab(
    const Tab& tab,
    LiveTabContext* context,
    WindowOpenDisposition disposition,
    LiveTab** live_tab) {
  LiveTab* restored_tab;
  if (disposition == WindowOpenDisposition::CURRENT_TAB && context) {
    restored_tab = context->ReplaceRestoredTab(
        tab.navigations, tab.current_navigation_index, tab.from_last_session,
        tab.extension_app_id, tab.platform_data.get(), tab.user_agent_override);
  } else {
    // We only respect the tab's original browser if there's no disposition.
    if (disposition == WindowOpenDisposition::UNKNOWN && tab.browser_id) {
      context = client_->FindLiveTabContextWithID(tab.browser_id);
    }

    int tab_index = -1;

    // |context| will be NULL in cases where one isn't already available (eg,
    // when invoked on Mac OS X with no windows open). In this case, create a
    // new browser into which we restore the tabs.
    if (context && disposition != WindowOpenDisposition::NEW_WINDOW) {
      tab_index = tab.tabstrip_index;
    } else {
      context = client_->CreateLiveTabContext(
          std::string(), gfx::Rect(), ui::SHOW_STATE_NORMAL, std::string());
      if (tab.browser_id)
        UpdateTabBrowserIDs(tab.browser_id, context->GetSessionID().id());
    }

    // Place the tab at the end if the tab index is no longer valid or
    // we were passed a specific disposition.
    if (tab_index < 0 || tab_index > context->GetTabCount() ||
        disposition != WindowOpenDisposition::UNKNOWN) {
      tab_index = context->GetTabCount();
    }

    restored_tab = context->AddRestoredTab(
        tab.navigations, tab_index, tab.current_navigation_index,
        tab.extension_app_id,
        disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB, tab.pinned,
        tab.from_last_session, tab.platform_data.get(),
        tab.user_agent_override);
  }
  client_->OnTabRestored(
      tab.navigations.at(tab.current_navigation_index).virtual_url());
  if (live_tab)
    *live_tab = restored_tab;

  return context;
}

bool TabRestoreServiceHelper::ValidateTab(const Tab& tab) {
  return !tab.navigations.empty() &&
         static_cast<size_t>(tab.current_navigation_index) <
             tab.navigations.size();
}

bool TabRestoreServiceHelper::ValidateWindow(const Window& window) {
  if (static_cast<size_t>(window.selected_tab_index) >= window.tabs.size())
    return false;

  for (const auto& tab : window.tabs) {
    if (!ValidateTab(*tab))
      return false;
  }

  return true;
}

bool TabRestoreServiceHelper::IsTabInteresting(const Tab& tab) {
  if (tab.navigations.empty())
    return false;

  if (tab.navigations.size() > 1)
    return true;

  return tab.pinned ||
         tab.navigations.at(0).virtual_url() != client_->GetNewTabURL();
}

bool TabRestoreServiceHelper::IsWindowInteresting(const Window& window) {
  if (window.tabs.empty())
    return false;

  if (window.tabs.size() > 1)
    return true;

  return IsTabInteresting(*window.tabs[0]);
}

bool TabRestoreServiceHelper::FilterEntry(const Entry& entry) {
  if (!ValidateEntry(entry))
    return false;

  switch (entry.type) {
    case TabRestoreService::TAB:
      return IsTabInteresting(static_cast<const Tab&>(entry));
    case TabRestoreService::WINDOW:
      return IsWindowInteresting(static_cast<const Window&>(entry));
  }
  NOTREACHED();
  return false;
}

void TabRestoreServiceHelper::UpdateTabBrowserIDs(SessionID::id_type old_id,
                                                  SessionID::id_type new_id) {
  for (const auto& entry : entries_) {
    if (entry->type == TabRestoreService::TAB) {
      auto& tab = static_cast<Tab&>(*entry);
      if (tab.browser_id == old_id)
        tab.browser_id = new_id;
    }
  }
}

base::Time TabRestoreServiceHelper::TimeNow() const {
  return time_factory_ ? time_factory_->TimeNow() : base::Time::Now();
}

}  // namespace sessions
