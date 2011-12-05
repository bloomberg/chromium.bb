// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service.h"

#include <algorithm>
#include <iterator>
#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_command.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"

using base::Time;

// TimeFactory-----------------------------------------------------------------

TabRestoreService::TimeFactory::~TimeFactory() {}

// Entry ----------------------------------------------------------------------

// ID of the next Entry.
static SessionID::id_type next_entry_id = 1;

TabRestoreService::Entry::Entry()
    : id(next_entry_id++),
      type(TAB),
      from_last_session(false) {}

TabRestoreService::Entry::Entry(Type type)
    : id(next_entry_id++),
      type(type),
      from_last_session(false) {}

TabRestoreService::Entry::~Entry() {}

// TabRestoreService ----------------------------------------------------------

// static
const size_t TabRestoreService::kMaxEntries = 25;

// Identifier for commands written to file.
// The ordering in the file is as follows:
// . When the user closes a tab a command of type
//   kCommandSelectedNavigationInTab is written identifying the tab and
//   the selected index, then a kCommandPinnedState command if the tab was
//   pinned and kCommandSetExtensionAppID if the tab has an app id. This is
//   followed by any number of kCommandUpdateTabNavigation commands (1 per
//   navigation entry).
// . When the user closes a window a kCommandSelectedNavigationInTab command
//   is written out and followed by n tab closed sequences (as previoulsy
//   described).
// . When the user restores an entry a command of type kCommandRestoredEntry
//   is written.
static const SessionCommand::id_type kCommandUpdateTabNavigation = 1;
static const SessionCommand::id_type kCommandRestoredEntry = 2;
static const SessionCommand::id_type kCommandWindow = 3;
static const SessionCommand::id_type kCommandSelectedNavigationInTab = 4;
static const SessionCommand::id_type kCommandPinnedState = 5;
static const SessionCommand::id_type kCommandSetExtensionAppID = 6;

// Number of entries (not commands) before we clobber the file and write
// everything.
static const int kEntriesPerReset = 40;

namespace {

// Payload structures.

typedef int32 RestoredEntryPayload;

// Payload used for the start of a window close. This is the old struct that is
// used for backwards compat when it comes to reading the session files. This
// struct must be POD, because we memset the contents.
struct WindowPayload {
  SessionID::id_type window_id;
  int32 selected_tab_index;
  int32 num_tabs;
};

// Payload used for the start of a tab close. This is the old struct that is
// used for backwards compat when it comes to reading the session files.
struct SelectedNavigationInTabPayload {
  SessionID::id_type id;
  int32 index;
};

// Payload used for the start of a window close.  This struct must be POD,
// because we memset the contents.
struct WindowPayload2 : WindowPayload {
  int64 timestamp;
};

// Payload used for the start of a tab close.
struct SelectedNavigationInTabPayload2 : SelectedNavigationInTabPayload {
  int64 timestamp;
};

// Only written if the tab is pinned.
typedef bool PinnedStatePayload;

typedef std::map<SessionID::id_type, TabRestoreService::Entry*> IDToEntry;

// If |id_to_entry| contains an entry for |id| the corresponding entry is
// deleted and removed from both |id_to_entry| and |entries|. This is used
// when creating entries from the backend file.
void RemoveEntryByID(SessionID::id_type id,
                     IDToEntry* id_to_entry,
                     std::vector<TabRestoreService::Entry*>* entries) {
  // Look for the entry in the map. If it is present, erase it from both
  // collections and return.
  IDToEntry::iterator i = id_to_entry->find(id);
  if (i != id_to_entry->end()) {
    entries->erase(std::find(entries->begin(), entries->end(), i->second));
    delete i->second;
    id_to_entry->erase(i);
    return;
  }

  // Otherwise, loop over all items in the map and see if any of the Windows
  // have Tabs with the |id|.
  for (IDToEntry::iterator i = id_to_entry->begin(); i != id_to_entry->end();
       ++i) {
    if (i->second->type == TabRestoreService::WINDOW) {
      TabRestoreService::Window* window =
          static_cast<TabRestoreService::Window*>(i->second);
      std::vector<TabRestoreService::Tab>::iterator j = window->tabs.begin();
      for ( ; j != window->tabs.end(); ++j) {
        // If the ID matches one of this window's tabs, remove it from the list.
        if ((*j).id == id) {
          window->tabs.erase(j);
          return;
        }
      }
    }
  }
}

void RecordAppLaunch(Profile* profile, const TabRestoreService::Tab& tab) {
  GURL url = tab.navigations.at(tab.current_navigation_index).virtual_url();
  DCHECK(profile->GetExtensionService());
  if (!profile->GetExtensionService()->IsInstalledApp(url))
    return;

  UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppLaunchHistogram,
                            extension_misc::APP_LAUNCH_NTP_RECENTLY_CLOSED,
                            extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
}

}  // namespace

TabRestoreService::Tab::Tab()
    : Entry(TAB),
      current_navigation_index(-1),
      browser_id(0),
      tabstrip_index(-1),
      pinned(false) {
}

TabRestoreService::Tab::~Tab() {
}

TabRestoreService::Window::Window() : Entry(WINDOW), selected_tab_index(-1) {
}

TabRestoreService::Window::~Window() {
}

TabRestoreService::TabRestoreService(Profile* profile,
    TabRestoreService::TimeFactory* time_factory)
    : BaseSessionService(BaseSessionService::TAB_RESTORE, profile,
                         FilePath()),
      load_state_(NOT_LOADED),
      restoring_(false),
      entries_to_write_(0),
      entries_written_(0),
      time_factory_(time_factory) {
}

TabRestoreService::~TabRestoreService() {
  if (backend())
    Save();

  FOR_EACH_OBSERVER(TabRestoreServiceObserver, observer_list_,
                    TabRestoreServiceDestroyed(this));
  STLDeleteElements(&entries_);
  STLDeleteElements(&staging_entries_);
  time_factory_ = NULL;
}

void TabRestoreService::AddObserver(TabRestoreServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TabRestoreService::RemoveObserver(TabRestoreServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TabRestoreService::CreateHistoricalTab(NavigationController* tab,
                                            int index) {
  if (restoring_)
    return;

  TabRestoreServiceDelegate* delegate =
      TabRestoreServiceDelegate::FindDelegateForController(tab, NULL);
  if (closing_delegates_.find(delegate) != closing_delegates_.end())
    return;

  scoped_ptr<Tab> local_tab(new Tab());
  PopulateTab(local_tab.get(), index, delegate, tab);
  if (local_tab->navigations.empty())
    return;

  AddEntry(local_tab.release(), true, true);
}

void TabRestoreService::BrowserClosing(TabRestoreServiceDelegate* delegate) {
  closing_delegates_.insert(delegate);

  scoped_ptr<Window> window(new Window());
  window->selected_tab_index = delegate->GetSelectedIndex();
  window->timestamp = TimeNow();
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
                &delegate->GetTabContentsAt(tab_index)->controller());
    if (window->tabs[entry_index].navigations.empty()) {
      window->tabs.erase(window->tabs.begin() + entry_index);
    } else {
      window->tabs[entry_index].browser_id = delegate->GetSessionID().id();
      entry_index++;
    }
  }
  if (window->tabs.size() == 1) {
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

void TabRestoreService::BrowserClosed(TabRestoreServiceDelegate* delegate) {
  closing_delegates_.erase(delegate);
}

void TabRestoreService::ClearEntries() {
  // Mark all the tabs as closed so that we don't attempt to restore them.
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i)
    ScheduleCommand(CreateRestoredEntryCommand((*i)->id));

  entries_to_write_ = 0;

  // Schedule a pending reset so that we nuke the file on next write.
  set_pending_reset(true);

  // Schedule a command, otherwise if there are no pending commands Save does
  // nothing.
  ScheduleCommand(CreateRestoredEntryCommand(1));

  STLDeleteElements(&entries_);
  NotifyTabsChanged();
}

const TabRestoreService::Entries& TabRestoreService::entries() const {
  return entries_;
}

void TabRestoreService::RestoreMostRecentEntry(
    TabRestoreServiceDelegate* delegate) {
  if (entries_.empty())
    return;

  RestoreEntryById(delegate, entries_.front()->id, UNKNOWN);
}

void TabRestoreService::RestoreEntryById(TabRestoreServiceDelegate* delegate,
                                         SessionID::id_type id,
                                         WindowOpenDisposition disposition) {
  Entries::iterator i = GetEntryIteratorById(id);
  if (i == entries_.end()) {
    // Don't hoark here, we allow an invalid id.
    return;
  }

  size_t index = 0;
  for (Entries::iterator j = entries_.begin(); j != i && j != entries_.end();
       ++j, ++index) {}
  if (static_cast<int>(index) < entries_to_write_)
    entries_to_write_--;

  ScheduleCommand(CreateRestoredEntryCommand(id));

  restoring_ = true;
  Entry* entry = *i;

  // If the entry's ID does not match the ID that is being restored, then the
  // entry is a window from which a single tab will be restored.
  bool restoring_tab_in_window = entry->id != id;

  if (!restoring_tab_in_window) {
    entries_.erase(i);
    i = entries_.end();
  }

  // |delegate| will be NULL in cases where one isn't already available (eg,
  // when invoked on Mac OS X with no windows open). In this case, create a
  // new browser into which we restore the tabs.
  if (entry->type == TAB) {
    Tab* tab = static_cast<Tab*>(entry);
    delegate = RestoreTab(*tab, delegate, disposition);
    delegate->ShowBrowserWindow();
  } else if (entry->type == WINDOW) {
    TabRestoreServiceDelegate* current_delegate = delegate;
    Window* window = static_cast<Window*>(entry);

    // When restoring a window, either the entire window can be restored, or a
    // single tab within it. If the entry's ID matches the one to restore, then
    // the entire window will be restored.
    if (!restoring_tab_in_window) {
      delegate = TabRestoreServiceDelegate::Create(profile());
      for (size_t tab_i = 0; tab_i < window->tabs.size(); ++tab_i) {
        const Tab& tab = window->tabs[tab_i];
        TabContents* restored_tab =
            delegate->AddRestoredTab(tab.navigations, delegate->GetTabCount(),
                                     tab.current_navigation_index,
                                     tab.extension_app_id,
                                     static_cast<int>(tab_i) ==
                                         window->selected_tab_index,
                                     tab.pinned, tab.from_last_session,
                                     tab.session_storage_namespace);
        if (restored_tab) {
          restored_tab->controller().LoadIfNecessary();
          RecordAppLaunch(profile(), tab);
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
          delegate = RestoreTab(tab, delegate, disposition);
          window->tabs.erase(tab_i);
          // If restoring the tab leaves the window with nothing else, delete it
          // as well.
          if (!window->tabs.size()) {
            entries_.erase(i);
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
        current_delegate->GetSelectedTabContents()) {
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

void TabRestoreService::LoadTabsFromLastSession() {
  if (load_state_ != NOT_LOADED || entries_.size() == kMaxEntries)
    return;

  load_state_ = LOADING;

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  if (!profile()->restored_last_session() &&
      !profile()->DidLastSessionExitCleanly() &&
      session_service) {
    // The previous session crashed and wasn't restored. Load the tabs/windows
    // that were open at the point of crash from the session service.
    session_service->GetLastSession(
        &crash_consumer_,
        base::Bind(&TabRestoreService::OnGotPreviousSession,
                   base::Unretained(this)));
  } else {
    load_state_ |= LOADED_LAST_SESSION;
  }

  // Request the tabs closed in the last session. If the last session crashed,
  // this won't contain the tabs/window that were open at the point of the
  // crash (the call to GetLastSession above requests those).
  ScheduleGetLastSessionCommands(
      new InternalGetCommandsRequest(
          base::Bind(&TabRestoreService::OnGotLastSessionCommands,
                     base::Unretained(this))),
      &load_consumer_);
}

void TabRestoreService::Save() {
  int to_write_count = std::min(entries_to_write_,
                                static_cast<int>(entries_.size()));
  entries_to_write_ = 0;
  if (entries_written_ + to_write_count > kEntriesPerReset) {
    to_write_count = entries_.size();
    set_pending_reset(true);
  }
  if (to_write_count) {
    // Write the to_write_count most recently added entries out. The most
    // recently added entry is at the front, so we use a reverse iterator to
    // write in the order the entries were added.
    Entries::reverse_iterator i = entries_.rbegin();
    DCHECK(static_cast<size_t>(to_write_count) <= entries_.size());
    std::advance(i, entries_.size() - static_cast<int>(to_write_count));
    for (; i != entries_.rend(); ++i) {
      Entry* entry = *i;
      if (entry->type == TAB) {
        Tab* tab = static_cast<Tab*>(entry);
        int selected_index = GetSelectedNavigationIndexToPersist(*tab);
        if (selected_index != -1)
          ScheduleCommandsForTab(*tab, selected_index);
      } else {
        ScheduleCommandsForWindow(*static_cast<Window*>(entry));
      }
      entries_written_++;
    }
  }
  if (pending_reset())
    entries_written_ = 0;
  BaseSessionService::Save();
}

void TabRestoreService::PopulateTab(Tab* tab,
                                    int index,
                                    TabRestoreServiceDelegate* delegate,
                                    NavigationController* controller) {
  const int pending_index = controller->pending_entry_index();
  int entry_count = controller->entry_count();
  if (entry_count == 0 && pending_index == 0)
    entry_count++;
  tab->navigations.resize(static_cast<int>(entry_count));
  for (int i = 0; i < entry_count; ++i) {
    NavigationEntry* entry = (i == pending_index) ?
        controller->pending_entry() : controller->GetEntryAtIndex(i);
    tab->navigations[i].SetFromNavigationEntry(*entry);
  }
  tab->timestamp = TimeNow();
  tab->current_navigation_index = controller->GetCurrentEntryIndex();
  if (tab->current_navigation_index == -1 && entry_count > 0)
    tab->current_navigation_index = 0;
  tab->tabstrip_index = index;

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(
          controller->tab_contents());
  // wrapper is NULL in some browser tests.
  if (wrapper) {
    const Extension* extension =
        wrapper->extension_tab_helper()->extension_app();
    if (extension)
      tab->extension_app_id = extension->id();
  }

  tab->session_storage_namespace = controller->session_storage_namespace();

  // Delegate may be NULL during unit tests.
  if (delegate) {
    tab->browser_id = delegate->GetSessionID().id();
    tab->pinned = delegate->IsTabPinned(tab->tabstrip_index);
  }
}

void TabRestoreService::NotifyTabsChanged() {
  FOR_EACH_OBSERVER(TabRestoreServiceObserver, observer_list_,
                    TabRestoreServiceChanged(this));
}

void TabRestoreService::AddEntry(Entry* entry, bool notify, bool to_front) {
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

  // Start the save timer, when it fires we'll generate the commands.
  StartSaveTimer();
  entries_to_write_++;
}

void TabRestoreService::PruneEntries() {
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

TabRestoreService::Entries::iterator TabRestoreService::GetEntryIteratorById(
    SessionID::id_type id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    if ((*i)->id == id)
      return i;

    // For Window entries, see if the ID matches a tab. If so, report the window
    // as the Entry.
    if ((*i)->type == WINDOW) {
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

void TabRestoreService::ScheduleCommandsForWindow(const Window& window) {
  DCHECK(!window.tabs.empty());
  int selected_tab = window.selected_tab_index;
  int valid_tab_count = 0;
  int real_selected_tab = selected_tab;
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    if (GetSelectedNavigationIndexToPersist(window.tabs[i]) != -1) {
      valid_tab_count++;
    } else if (static_cast<int>(i) < selected_tab) {
      real_selected_tab--;
    }
  }
  if (valid_tab_count == 0)
    return;  // No tabs to persist.

  ScheduleCommand(
      CreateWindowCommand(window.id,
                          std::min(real_selected_tab, valid_tab_count - 1),
                          valid_tab_count,
                          window.timestamp));

  for (size_t i = 0; i < window.tabs.size(); ++i) {
    int selected_index = GetSelectedNavigationIndexToPersist(window.tabs[i]);
    if (selected_index != -1)
      ScheduleCommandsForTab(window.tabs[i], selected_index);
  }
}

void TabRestoreService::ScheduleCommandsForTab(const Tab& tab,
                                               int selected_index) {
  const std::vector<TabNavigation>& navigations = tab.navigations;
  int max_index = static_cast<int>(navigations.size());

  // Determine the first navigation we'll persist.
  int valid_count_before_selected = 0;
  int first_index_to_persist = selected_index;
  for (int i = selected_index - 1; i >= 0 &&
       valid_count_before_selected < max_persist_navigation_count; --i) {
    if (ShouldTrackEntry(navigations[i].virtual_url())) {
      first_index_to_persist = i;
      valid_count_before_selected++;
    }
  }

  // Write the command that identifies the selected tab.
  ScheduleCommand(
      CreateSelectedNavigationInTabCommand(tab.id,
                                           valid_count_before_selected,
                                           tab.timestamp));

  if (tab.pinned) {
    PinnedStatePayload payload = true;
    SessionCommand* command =
        new SessionCommand(kCommandPinnedState, sizeof(payload));
    memcpy(command->contents(), &payload, sizeof(payload));
    ScheduleCommand(command);
  }

  if (!tab.extension_app_id.empty()) {
    ScheduleCommand(
        CreateSetTabExtensionAppIDCommand(kCommandSetExtensionAppID, tab.id,
                                          tab.extension_app_id));
  }

  // Then write the navigations.
  for (int i = first_index_to_persist, wrote_count = 0;
       i < max_index && wrote_count < 2 * max_persist_navigation_count; ++i) {
    if (ShouldTrackEntry(navigations[i].virtual_url())) {
      // Creating a NavigationEntry isn't the most efficient way to go about
      // this, but it simplifies the code and makes it less error prone as we
      // add new data to NavigationEntry.
      scoped_ptr<NavigationEntry> entry(
          navigations[i].ToNavigationEntry(wrote_count, profile()));
      ScheduleCommand(
          CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation, tab.id,
                                           wrote_count++, *entry));
    }
  }
}

SessionCommand* TabRestoreService::CreateWindowCommand(SessionID::id_type id,
                                                       int selected_tab_index,
                                                       int num_tabs,
                                                       Time timestamp) {
  WindowPayload2 payload;
  // |timestamp| is aligned on a 16 byte boundary, leaving 4 bytes of
  // uninitialized memory in the struct.
  memset(&payload, 0, sizeof(payload));
  payload.window_id = id;
  payload.selected_tab_index = selected_tab_index;
  payload.num_tabs = num_tabs;
  payload.timestamp = timestamp.ToInternalValue();

  SessionCommand* command =
      new SessionCommand(kCommandWindow, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* TabRestoreService::CreateSelectedNavigationInTabCommand(
    SessionID::id_type tab_id,
    int32 index,
    Time timestamp) {
  SelectedNavigationInTabPayload2 payload;
  payload.id = tab_id;
  payload.index = index;
  payload.timestamp = timestamp.ToInternalValue();
  SessionCommand* command =
      new SessionCommand(kCommandSelectedNavigationInTab, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* TabRestoreService::CreateRestoredEntryCommand(
    SessionID::id_type entry_id) {
  RestoredEntryPayload payload = entry_id;
  SessionCommand* command =
      new SessionCommand(kCommandRestoredEntry, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

int TabRestoreService::GetSelectedNavigationIndexToPersist(const Tab& tab) {
  const std::vector<TabNavigation>& navigations = tab.navigations;
  int selected_index = tab.current_navigation_index;
  int max_index = static_cast<int>(navigations.size());

  // Find the first navigation to persist. We won't persist the selected
  // navigation if ShouldTrackEntry returns false.
  while (selected_index >= 0 &&
         !ShouldTrackEntry(navigations[selected_index].virtual_url())) {
    selected_index--;
  }

  if (selected_index != -1)
    return selected_index;

  // Couldn't find a navigation to persist going back, go forward.
  selected_index = tab.current_navigation_index + 1;
  while (selected_index < max_index &&
         !ShouldTrackEntry(navigations[selected_index].virtual_url())) {
    selected_index++;
  }

  return (selected_index == max_index) ? -1 : selected_index;
}

void TabRestoreService::OnGotLastSessionCommands(
    Handle handle,
    scoped_refptr<InternalGetCommandsRequest> request) {
  std::vector<Entry*> entries;
  CreateEntriesFromCommands(request, &entries);
  // Closed tabs always go to the end.
  staging_entries_.insert(staging_entries_.end(), entries.begin(),
                          entries.end());
  load_state_ |= LOADED_LAST_TABS;
  LoadStateChanged();
}

void TabRestoreService::CreateEntriesFromCommands(
    scoped_refptr<InternalGetCommandsRequest> request,
    std::vector<Entry*>* loaded_entries) {
  if (request->canceled() || entries_.size() == kMaxEntries)
    return;

  std::vector<SessionCommand*>& commands = request->commands;
  // Iterate through the commands populating entries and id_to_entry.
  ScopedVector<Entry> entries;
  IDToEntry id_to_entry;
  // If non-null we're processing the navigations of this tab.
  Tab* current_tab = NULL;
  // If non-null we're processing the tabs of this window.
  Window* current_window = NULL;
  // If > 0, we've gotten a window command but not all the tabs yet.
  int pending_window_tabs = 0;
  for (std::vector<SessionCommand*>::const_iterator i = commands.begin();
       i != commands.end(); ++i) {
    const SessionCommand& command = *(*i);
    switch (command.id()) {
      case kCommandRestoredEntry: {
        if (pending_window_tabs > 0) {
          // Should never receive a restored command while waiting for all the
          // tabs in a window.
          return;
        }

        current_tab = NULL;
        current_window = NULL;

        RestoredEntryPayload payload;
        if (!command.GetPayload(&payload, sizeof(payload)))
          return;
        RemoveEntryByID(payload, &id_to_entry, &(entries.get()));
        break;
      }

      case kCommandWindow: {
        WindowPayload2 payload;
        if (pending_window_tabs > 0) {
          // Should never receive a window command while waiting for all the
          // tabs in a window.
          return;
        }

        // Try the new payload first
        if (!command.GetPayload(&payload, sizeof(payload))) {
          // then the old payload
          WindowPayload old_payload;
          if (!command.GetPayload(&old_payload, sizeof(old_payload)))
            return;

          // Copy the old payload data to the new payload.
          payload.window_id = old_payload.window_id;
          payload.selected_tab_index = old_payload.selected_tab_index;
          payload.num_tabs = old_payload.num_tabs;
          // Since we don't have a time use time 0 which is used to mark as an
          // unknown timestamp.
          payload.timestamp = 0;
        }

        pending_window_tabs = payload.num_tabs;
        if (pending_window_tabs <= 0) {
          // Should always have at least 1 tab. Likely indicates corruption.
          return;
        }

        RemoveEntryByID(payload.window_id, &id_to_entry, &(entries.get()));

        current_window = new Window();
        current_window->selected_tab_index = payload.selected_tab_index;
        current_window->timestamp = Time::FromInternalValue(payload.timestamp);
        entries->push_back(current_window);
        id_to_entry[payload.window_id] = current_window;
        break;
      }

      case kCommandSelectedNavigationInTab: {
        SelectedNavigationInTabPayload2 payload;
        if (!command.GetPayload(&payload, sizeof(payload))) {
          SelectedNavigationInTabPayload old_payload;
          if (!command.GetPayload(&old_payload, sizeof(old_payload)))
            return;
          payload.id = old_payload.id;
          payload.index = old_payload.index;
          // Since we don't have a time use time 0 which is used to mark as an
          // unknown timestamp.
          payload.timestamp = 0;
        }

        if (pending_window_tabs > 0) {
          if (!current_window) {
            // We should have created a window already.
            NOTREACHED();
            return;
          }
          current_window->tabs.resize(current_window->tabs.size() + 1);
          current_tab = &(current_window->tabs.back());
          if (--pending_window_tabs == 0)
            current_window = NULL;
        } else {
          RemoveEntryByID(payload.id, &id_to_entry, &(entries.get()));
          current_tab = new Tab();
          id_to_entry[payload.id] = current_tab;
          current_tab->timestamp = Time::FromInternalValue(payload.timestamp);
          entries->push_back(current_tab);
        }
        current_tab->current_navigation_index = payload.index;
        break;
      }

      case kCommandUpdateTabNavigation: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        current_tab->navigations.resize(current_tab->navigations.size() + 1);
        SessionID::id_type tab_id;
        if (!RestoreUpdateTabNavigationCommand(
            command, &current_tab->navigations.back(), &tab_id)) {
          return;
        }
        break;
      }

      case kCommandPinnedState: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        // NOTE: payload doesn't matter. kCommandPinnedState is only written if
        // tab is pinned.
        current_tab->pinned = true;
        break;
      }

      case kCommandSetExtensionAppID: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        SessionID::id_type tab_id;
        std::string extension_app_id;
        if (!RestoreSetTabExtensionAppIDCommand(command, &tab_id,
                                                &extension_app_id)) {
          return;
        }
        current_tab->extension_app_id.swap(extension_app_id);
        break;
      }

      default:
        // Unknown type, usually indicates corruption of file. Ignore it.
        return;
    }
  }

  // If there was corruption some of the entries won't be valid.
  ValidateAndDeleteEmptyEntries(&(entries.get()));

  loaded_entries->swap(entries.get());
}

TabRestoreServiceDelegate* TabRestoreService::RestoreTab(
    const Tab& tab,
    TabRestoreServiceDelegate* delegate,
    WindowOpenDisposition disposition) {
  if (disposition == CURRENT_TAB && delegate) {
    delegate->ReplaceRestoredTab(tab.navigations,
                                 tab.current_navigation_index,
                                 tab.from_last_session,
                                 tab.extension_app_id,
                                 tab.session_storage_namespace);
  } else {
    // We only respsect the tab's original browser if there's no disposition.
    if (disposition == UNKNOWN && tab.has_browser())
      delegate = TabRestoreServiceDelegate::FindDelegateWithID(tab.browser_id);

    int tab_index = -1;

    // |delegate| will be NULL in cases where one isn't already available (eg,
    // when invoked on Mac OS X with no windows open). In this case, create a
    // new browser into which we restore the tabs.
    if (delegate && disposition != NEW_WINDOW) {
      tab_index = tab.tabstrip_index;
    } else {
      delegate = TabRestoreServiceDelegate::Create(profile());
      if (tab.has_browser())
        UpdateTabBrowserIDs(tab.browser_id, delegate->GetSessionID().id());
    }

    // Place the tab at the end if the tab index is no longer valid or
    // we were passed a specific disposition.
    if (tab_index < 0 || tab_index > delegate->GetTabCount() ||
        disposition != UNKNOWN) {
      tab_index = delegate->GetTabCount();
    }

    delegate->AddRestoredTab(tab.navigations,
                             tab_index,
                             tab.current_navigation_index,
                             tab.extension_app_id,
                             disposition != NEW_BACKGROUND_TAB,
                             tab.pinned,
                             tab.from_last_session,
                             tab.session_storage_namespace);
  }
  RecordAppLaunch(profile(), tab);
  return delegate;
}


bool TabRestoreService::ValidateTab(Tab* tab) {
  if (tab->navigations.empty())
    return false;

  tab->current_navigation_index =
      std::max(0, std::min(tab->current_navigation_index,
                           static_cast<int>(tab->navigations.size()) - 1));

  return true;
}

bool TabRestoreService::ValidateWindow(Window* window) {
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

bool TabRestoreService::ValidateEntry(Entry* entry) {
  if (entry->type == TAB)
    return ValidateTab(static_cast<Tab*>(entry));

  if (entry->type == WINDOW)
    return ValidateWindow(static_cast<Window*>(entry));

  NOTREACHED();
  return false;
}

bool TabRestoreService::IsTabInteresting(const Tab* tab) {
  if (tab->navigations.empty())
    return false;

  if (tab->navigations.size() > 1)
    return true;

  return tab->pinned ||
      tab->navigations.at(0).virtual_url() !=
          GURL(chrome::kChromeUINewTabURL);
}

bool TabRestoreService::IsWindowInteresting(const Window* window) {
  if (window->tabs.empty())
    return false;

  if (window->tabs.size() > 1)
    return true;

  return IsTabInteresting(&window->tabs[0]);
}

bool TabRestoreService::FilterEntry(Entry* entry) {
  if (!ValidateEntry(entry))
    return false;

  if (entry->type == TAB)
    return IsTabInteresting(static_cast<Tab*>(entry));
  else if (entry->type == WINDOW)
    return IsWindowInteresting(static_cast<Window*>(entry));

  NOTREACHED();
  return false;
}

void TabRestoreService::ValidateAndDeleteEmptyEntries(
    std::vector<Entry*>* entries) {
  std::vector<Entry*> valid_entries;
  std::vector<Entry*> invalid_entries;

  // Iterate from the back so that we keep the most recently closed entries.
  for (std::vector<Entry*>::reverse_iterator i = entries->rbegin();
       i != entries->rend(); ++i) {
    if (ValidateEntry(*i))
      valid_entries.push_back(*i);
    else
      invalid_entries.push_back(*i);
  }
  // NOTE: at this point the entries are ordered with newest at the front.
  entries->swap(valid_entries);

  // Delete the remaining entries.
  STLDeleteElements(&invalid_entries);
}

void TabRestoreService::UpdateTabBrowserIDs(SessionID::id_type old_id,
                                            SessionID::id_type new_id) {
  for (Entries::iterator i = entries_.begin(); i != entries_.end(); ++i) {
    Entry* entry = *i;
    if (entry->type == TAB) {
      Tab* tab = static_cast<Tab*>(entry);
      if (tab->browser_id == old_id)
        tab->browser_id = new_id;
    }
  }
}

void TabRestoreService::OnGotPreviousSession(
    Handle handle,
    std::vector<SessionWindow*>* windows) {
  std::vector<Entry*> entries;
  CreateEntriesFromWindows(windows, &entries);
  // Previous session tabs go first.
  staging_entries_.insert(staging_entries_.begin(), entries.begin(),
                          entries.end());
  load_state_ |= LOADED_LAST_SESSION;
  LoadStateChanged();
}

void TabRestoreService::CreateEntriesFromWindows(
    std::vector<SessionWindow*>* windows,
    std::vector<Entry*>* entries) {
  for (size_t i = 0; i < windows->size(); ++i) {
    scoped_ptr<Window> window(new Window());
    if (ConvertSessionWindowToWindow((*windows)[i], window.get()))
      entries->push_back(window.release());
  }
}

bool TabRestoreService::ConvertSessionWindowToWindow(
    SessionWindow* session_window,
    Window* window) {
  for (size_t i = 0; i < session_window->tabs.size(); ++i) {
    if (!session_window->tabs[i]->navigations.empty()) {
      window->tabs.resize(window->tabs.size() + 1);
      Tab& tab = window->tabs.back();
      tab.pinned = session_window->tabs[i]->pinned;
      tab.navigations.swap(session_window->tabs[i]->navigations);
      tab.current_navigation_index =
          session_window->tabs[i]->current_navigation_index;
      tab.extension_app_id = session_window->tabs[i]->extension_app_id;
      tab.timestamp = Time();
    }
  }
  if (window->tabs.empty())
    return false;

  window->selected_tab_index =
      std::min(session_window->selected_tab_index,
               static_cast<int>(window->tabs.size() - 1));
  window->timestamp = Time();
  return true;
}

void TabRestoreService::LoadStateChanged() {
  if ((load_state_ & (LOADED_LAST_TABS | LOADED_LAST_SESSION)) !=
      (LOADED_LAST_TABS | LOADED_LAST_SESSION)) {
    // Still waiting on previous session or previous tabs.
    return;
  }

  // We're done loading.
  load_state_ ^= LOADING;

  if (staging_entries_.empty() || entries_.size() >= kMaxEntries) {
    STLDeleteElements(&staging_entries_);
    return;
  }

  if (staging_entries_.size() + entries_.size() > kMaxEntries) {
    // If we add all the staged entries we'll end up with more than
    // kMaxEntries. Delete entries such that we only end up with
    // at most kMaxEntries.
    int surplus = kMaxEntries - entries_.size();
    CHECK_LE(0, surplus);
    CHECK_GE(static_cast<int>(staging_entries_.size()), surplus);
    STLDeleteContainerPointers(
        staging_entries_.begin() + (kMaxEntries - entries_.size()),
        staging_entries_.end());
    staging_entries_.erase(
        staging_entries_.begin() + (kMaxEntries - entries_.size()),
        staging_entries_.end());
  }

  // And add them.
  for (size_t i = 0; i < staging_entries_.size(); ++i) {
    staging_entries_[i]->from_last_session = true;
    AddEntry(staging_entries_[i], false, false);
  }

  // AddEntry takes ownership of the entry, need to clear out entries so that
  // it doesn't delete them.
  staging_entries_.clear();

  // Make it so we rewrite all the tabs. We need to do this otherwise we won't
  // correctly write out the entries when Save is invoked (Save starts from
  // the front, not the end and we just added the entries to the end).
  entries_to_write_ = staging_entries_.size();

  PruneEntries();
  NotifyTabsChanged();
}

Time TabRestoreService::TimeNow() const {
  return time_factory_ ? time_factory_->TimeNow() : Time::Now();
}
