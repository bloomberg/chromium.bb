// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/persistent_tab_restore_service.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/sessions/core/base_session_service.h"
#include "components/sessions/core/base_session_service_commands.h"
#include "components/sessions/core/base_session_service_delegate.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"

namespace sessions {

namespace {

// Only written if the tab is pinned.
typedef bool PinnedStatePayload;

typedef int32_t RestoredEntryPayload;

// Payload used for the start of a tab close. This is the old struct that is
// used for backwards compat when it comes to reading the session files.
struct SelectedNavigationInTabPayload {
  SessionID::id_type id;
  int32_t index;
};

// Payload used for the start of a window close. This is the old struct that is
// used for backwards compat when it comes to reading the session files. This
// struct must be POD, because we memset the contents.
struct WindowPayload {
  SessionID::id_type window_id;
  int32_t selected_tab_index;
  int32_t num_tabs;
};

// Payload used for the start of a window close.  This struct must be POD,
// because we memset the contents.
struct WindowPayload2 : WindowPayload {
  int64_t timestamp;
};

// Payload used for the start of a tab close.
struct SelectedNavigationInTabPayload2 : SelectedNavigationInTabPayload {
  int64_t timestamp;
};

// Used to indicate what has loaded.
enum LoadState {
  // Indicates we haven't loaded anything.
  NOT_LOADED           = 1 << 0,

  // Indicates we've asked for the last sessions and tabs but haven't gotten the
  // result back yet.
  LOADING              = 1 << 2,

  // Indicates we finished loading the last tabs (but not necessarily the last
  // session).
  LOADED_LAST_TABS     = 1 << 3,

  // Indicates we finished loading the last session (but not necessarily the
  // last tabs).
  LOADED_LAST_SESSION  = 1 << 4
};

// Identifier for commands written to file. The ordering in the file is as
// follows:
// . When the user closes a tab a command of type
//   kCommandSelectedNavigationInTab is written identifying the tab and
//   the selected index, then a kCommandPinnedState command if the tab was
//   pinned and kCommandSetExtensionAppID if the tab has an app id and
//   the user agent override if it was using one.  This is
//   followed by any number of kCommandUpdateTabNavigation commands (1 per
//   navigation entry).
// . When the user closes a window a kCommandSelectedNavigationInTab command
//   is written out and followed by n tab closed sequences (as previoulsy
//   described).
// . When the user restores an entry a command of type kCommandRestoredEntry
//   is written.
const SessionCommand::id_type kCommandUpdateTabNavigation = 1;
const SessionCommand::id_type kCommandRestoredEntry = 2;
const SessionCommand::id_type kCommandWindow = 3;
const SessionCommand::id_type kCommandSelectedNavigationInTab = 4;
const SessionCommand::id_type kCommandPinnedState = 5;
const SessionCommand::id_type kCommandSetExtensionAppID = 6;
const SessionCommand::id_type kCommandSetWindowAppName = 7;
const SessionCommand::id_type kCommandSetTabUserAgentOverride = 8;

// Number of entries (not commands) before we clobber the file and write
// everything.
const int kEntriesPerReset = 40;

const size_t kMaxEntries = TabRestoreServiceHelper::kMaxEntries;

void RemoveEntryByID(
    SessionID::id_type id,
    std::vector<std::unique_ptr<TabRestoreService::Entry>>* entries) {
  // Look for the entry in the top-level collection.
  for (auto it = entries->begin(); it != entries->end(); ++it) {
    TabRestoreService::Entry& entry = **it;
    // Erase it if it's our target.
    if (entry.id == id) {
      entries->erase(it);
      return;
    }
    // If this entry is a window, look through its tabs.
    if (entry.type == TabRestoreService::WINDOW) {
      auto& window = static_cast<TabRestoreService::Window&>(entry);
      for (auto it = window.tabs.begin(); it != window.tabs.end(); ++it) {
        const TabRestoreService::Tab& tab = **it;
        // Erase it if it's our target.
        if (tab.id == id) {
          window.tabs.erase(it);
          return;
        }
      }
    }
  }
}

}  // namespace

// PersistentTabRestoreService::Delegate ---------------------------------------

// This restore service will create and own a BaseSessionService and implement
// the required BaseSessionServiceDelegate.
class PersistentTabRestoreService::Delegate
    : public BaseSessionServiceDelegate,
      public TabRestoreServiceHelper::Observer {
 public:
  explicit Delegate(TabRestoreServiceClient* client);

  ~Delegate() override;

  // BaseSessionServiceDelegate:
  bool ShouldUseDelayedSave() override;
  void OnWillSaveCommands() override;

  // TabRestoreServiceHelper::Observer:
  void OnClearEntries() override;
  void OnRestoreEntryById(SessionID::id_type id,
                          Entries::const_iterator entry_iterator) override;
  void OnAddEntry() override;

  void set_tab_restore_service_helper(
      TabRestoreServiceHelper* tab_restore_service_helper) {
    tab_restore_service_helper_ = tab_restore_service_helper;
  }

  void LoadTabsFromLastSession();

  void DeleteLastSession();

  bool IsLoaded() const;

  // Creates and add entries to |entries| for each of the windows in |windows|.
  static void CreateEntriesFromWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      std::vector<std::unique_ptr<Entry>>* entries);

  void Shutdown();

  // Schedules the commands for a window close.
  void ScheduleCommandsForWindow(const Window& window);

  // Schedules the commands for a tab close. |selected_index| gives the index of
  // the selected navigation.
  void ScheduleCommandsForTab(const Tab& tab, int selected_index);

  // Creates a window close command.
  static std::unique_ptr<SessionCommand> CreateWindowCommand(
      SessionID::id_type id,
      int selected_tab_index,
      int num_tabs,
      base::Time timestamp);

  // Creates a tab close command.
  static std::unique_ptr<SessionCommand> CreateSelectedNavigationInTabCommand(
      SessionID::id_type tab_id,
      int32_t index,
      base::Time timestamp);

  // Creates a restore command.
  static std::unique_ptr<SessionCommand> CreateRestoredEntryCommand(
      SessionID::id_type entry_id);

  // Returns the index to persist as the selected index. This is the same as
  // |tab.current_navigation_index| unless the entry at
  // |tab.current_navigation_index| shouldn't be persisted. Returns -1 if no
  // valid navigation to persist.
  int GetSelectedNavigationIndexToPersist(const Tab& tab);

  // Invoked when we've loaded the session commands that identify the previously
  // closed tabs. This creates entries, adds them to staging_entries_, and
  // invokes LoadState.
  void OnGotLastSessionCommands(
      std::vector<std::unique_ptr<SessionCommand>> commands);

  // Populates |loaded_entries| with Entries from |commands|.
  void CreateEntriesFromCommands(
      const std::vector<std::unique_ptr<SessionCommand>>& commands,
      std::vector<std::unique_ptr<Entry>>* loaded_entries);

  // Validates all entries in |entries|, deleting any with no navigations. This
  // also deletes any entries beyond the max number of entries we can hold.
  static void ValidateAndDeleteEmptyEntries(
      std::vector<std::unique_ptr<Entry>>* entries);

  // Callback from BaseSessionService when we've received the windows from the
  // previous session. This creates and add entries to |staging_entries_| and
  // invokes LoadStateChanged. |ignored_active_window| is ignored because we
  // don't need to restore activation.
  void OnGotPreviousSession(std::vector<std::unique_ptr<SessionWindow>> windows,
                            SessionID::id_type ignored_active_window);

  // Converts a SessionWindow into a Window, returning true on success. We use 0
  // as the timestamp here since we do not know when the window/tab was closed.
  static bool ConvertSessionWindowToWindow(SessionWindow* session_window,
                                           Window* window);

  // Invoked when previous tabs or session is loaded. If both have finished
  // loading the entries in |staging_entries_| are added to entries and
  // observers are notified.
  void LoadStateChanged();

 private:
  // The associated client.
  TabRestoreServiceClient* client_;

  std::unique_ptr<BaseSessionService> base_session_service_;

  TabRestoreServiceHelper* tab_restore_service_helper_;

  // The number of entries to write.
  int entries_to_write_;

  // Number of entries we've written.
  int entries_written_;

  // Whether we've loaded the last session.
  int load_state_;

  // Results from previously closed tabs/sessions is first added here. When the
  // results from both us and the session restore service have finished loading
  // LoadStateChanged is invoked, which adds these entries to entries_.
  std::vector<std::unique_ptr<Entry>> staging_entries_;

  // Used when loading previous tabs/session and open tabs/session.
  base::CancelableTaskTracker cancelable_task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PersistentTabRestoreService::Delegate::Delegate(TabRestoreServiceClient* client)
    : client_(client),
      base_session_service_(
          new BaseSessionService(BaseSessionService::TAB_RESTORE,
                                 client_->GetPathToSaveTo(),
                                 this)),
      tab_restore_service_helper_(NULL),
      entries_to_write_(0),
      entries_written_(0),
      load_state_(NOT_LOADED) {}

PersistentTabRestoreService::Delegate::~Delegate() {}

bool PersistentTabRestoreService::Delegate::ShouldUseDelayedSave() {
  return true;
}

void PersistentTabRestoreService::Delegate::OnWillSaveCommands() {
  const Entries& entries = tab_restore_service_helper_->entries();
  int to_write_count = std::min(entries_to_write_,
                                static_cast<int>(entries.size()));
  entries_to_write_ = 0;
  if (entries_written_ + to_write_count > kEntriesPerReset) {
    to_write_count = entries.size();
    base_session_service_->set_pending_reset(true);
  }
  if (to_write_count) {
    // Write the to_write_count most recently added entries out. The most
    // recently added entry is at the front, so we use a reverse iterator to
    // write in the order the entries were added.
    Entries::const_reverse_iterator i = entries.rbegin();
    DCHECK(static_cast<size_t>(to_write_count) <= entries.size());
    std::advance(i, entries.size() - static_cast<int>(to_write_count));
    for (; i != entries.rend(); ++i) {
      Entry& entry = **i;
      switch (entry.type) {
        case TAB: {
          Tab& tab = static_cast<Tab&>(entry);
          int selected_index = GetSelectedNavigationIndexToPersist(tab);
          if (selected_index != -1)
            ScheduleCommandsForTab(tab, selected_index);
          break;
        }
        case WINDOW:
          ScheduleCommandsForWindow(static_cast<Window&>(entry));
          break;
      }
      entries_written_++;
    }
  }
  if (base_session_service_->pending_reset())
    entries_written_ = 0;
}

void PersistentTabRestoreService::Delegate::OnClearEntries() {
  // Mark all the tabs as closed so that we don't attempt to restore them.
  const Entries& entries = tab_restore_service_helper_->entries();
  for (Entries::const_iterator i = entries.begin(); i != entries.end(); ++i)
    base_session_service_->ScheduleCommand(
        CreateRestoredEntryCommand((*i)->id));

  entries_to_write_ = 0;

  // Schedule a pending reset so that we nuke the file on next write.
  base_session_service_->set_pending_reset(true);

  // Schedule a command, otherwise if there are no pending commands Save does
  // nothing.
  base_session_service_->ScheduleCommand(CreateRestoredEntryCommand(1));
}

void PersistentTabRestoreService::Delegate::OnRestoreEntryById(
    SessionID::id_type id,
    Entries::const_iterator entry_iterator) {
  size_t index = 0;
  const Entries& entries = tab_restore_service_helper_->entries();
  for (Entries::const_iterator j = entries.begin();
       j != entry_iterator && j != entries.end();
       ++j, ++index) {}
  if (static_cast<int>(index) < entries_to_write_)
    entries_to_write_--;

  base_session_service_->ScheduleCommand(CreateRestoredEntryCommand(id));
}

void PersistentTabRestoreService::Delegate::OnAddEntry() {
  // Start the save timer, when it fires we'll generate the commands.
  base_session_service_->StartSaveTimer();
  entries_to_write_++;
}

void PersistentTabRestoreService::Delegate::LoadTabsFromLastSession() {
  if (load_state_ != NOT_LOADED)
    return;

  if (tab_restore_service_helper_->entries().size() == kMaxEntries) {
    // We already have the max number of entries we can take. There is no point
    // in attempting to load since we'll just drop the results. Skip to loaded.
    load_state_ = (LOADING | LOADED_LAST_SESSION | LOADED_LAST_TABS);
    LoadStateChanged();
    return;
  }

  load_state_ = LOADING;
  if (client_->HasLastSession()) {
    client_->GetLastSession(
        base::Bind(&Delegate::OnGotPreviousSession, base::Unretained(this)),
        &cancelable_task_tracker_);
  } else {
    load_state_ |= LOADED_LAST_SESSION;
  }

  // Request the tabs closed in the last session. If the last session crashed,
  // this won't contain the tabs/window that were open at the point of the
  // crash (the call to GetLastSession above requests those).
  base_session_service_->ScheduleGetLastSessionCommands(
      base::Bind(&Delegate::OnGotLastSessionCommands, base::Unretained(this)),
      &cancelable_task_tracker_);
}

void PersistentTabRestoreService::Delegate::DeleteLastSession() {
  base_session_service_->DeleteLastSession();
}

bool PersistentTabRestoreService::Delegate::IsLoaded() const {
  return !(load_state_ & (NOT_LOADED | LOADING));
}

// static
void PersistentTabRestoreService::Delegate::CreateEntriesFromWindows(
    std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
    std::vector<std::unique_ptr<Entry>>* entries) {
  for (const auto& session_window : *windows) {
    std::unique_ptr<Window> window = base::MakeUnique<Window>();
    if (ConvertSessionWindowToWindow(session_window.get(), window.get()))
      entries->push_back(std::move(window));
  }
}

void PersistentTabRestoreService::Delegate::Shutdown() {
  base_session_service_->Save();
}

void PersistentTabRestoreService::Delegate::ScheduleCommandsForWindow(
    const Window& window) {
  DCHECK(!window.tabs.empty());
  int selected_tab = window.selected_tab_index;
  int valid_tab_count = 0;
  int real_selected_tab = selected_tab;
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    if (GetSelectedNavigationIndexToPersist(*window.tabs[i]) != -1) {
      valid_tab_count++;
    } else if (static_cast<int>(i) < selected_tab) {
      real_selected_tab--;
    }
  }
  if (valid_tab_count == 0)
    return;  // No tabs to persist.

  base_session_service_->ScheduleCommand(CreateWindowCommand(
      window.id, std::min(real_selected_tab, valid_tab_count - 1),
      valid_tab_count, window.timestamp));

  if (!window.app_name.empty()) {
    base_session_service_->ScheduleCommand(CreateSetWindowAppNameCommand(
        kCommandSetWindowAppName, window.id, window.app_name));
  }

  for (size_t i = 0; i < window.tabs.size(); ++i) {
    int selected_index = GetSelectedNavigationIndexToPersist(*window.tabs[i]);
    if (selected_index != -1)
      ScheduleCommandsForTab(*window.tabs[i], selected_index);
  }
}

void PersistentTabRestoreService::Delegate::ScheduleCommandsForTab(
    const Tab& tab,
    int selected_index) {
  const std::vector<SerializedNavigationEntry>& navigations = tab.navigations;
  int max_index = static_cast<int>(navigations.size());

  // Determine the first navigation we'll persist.
  int valid_count_before_selected = 0;
  int first_index_to_persist = selected_index;
  for (int i = selected_index - 1;
       i >= 0 && valid_count_before_selected < gMaxPersistNavigationCount;
       --i) {
    if (client_->ShouldTrackURLForRestore(navigations[i].virtual_url())) {
      first_index_to_persist = i;
      valid_count_before_selected++;
    }
  }

  // Write the command that identifies the selected tab.
  base_session_service_->ScheduleCommand(CreateSelectedNavigationInTabCommand(
      tab.id, valid_count_before_selected, tab.timestamp));

  if (tab.pinned) {
    PinnedStatePayload payload = true;
    std::unique_ptr<SessionCommand> command(
        new SessionCommand(kCommandPinnedState, sizeof(payload)));
    memcpy(command->contents(), &payload, sizeof(payload));
    base_session_service_->ScheduleCommand(std::move(command));
  }

  if (!tab.extension_app_id.empty()) {
    base_session_service_->ScheduleCommand(CreateSetTabExtensionAppIDCommand(
        kCommandSetExtensionAppID, tab.id, tab.extension_app_id));
  }

  if (!tab.user_agent_override.empty()) {
    base_session_service_->ScheduleCommand(CreateSetTabUserAgentOverrideCommand(
        kCommandSetTabUserAgentOverride, tab.id, tab.user_agent_override));
  }

  // Then write the navigations.
  for (int i = first_index_to_persist, wrote_count = 0;
       wrote_count < 2 * gMaxPersistNavigationCount && i < max_index; ++i) {
    if (client_->ShouldTrackURLForRestore(navigations[i].virtual_url())) {
      base_session_service_->ScheduleCommand(
          CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation,
                                           tab.id,
                                           navigations[i]));
    }
  }
}

// static
std::unique_ptr<SessionCommand>
PersistentTabRestoreService::Delegate::CreateWindowCommand(
    SessionID::id_type id,
    int selected_tab_index,
    int num_tabs,
    base::Time timestamp) {
  WindowPayload2 payload;
  // |timestamp| is aligned on a 16 byte boundary, leaving 4 bytes of
  // uninitialized memory in the struct.
  memset(&payload, 0, sizeof(payload));
  payload.window_id = id;
  payload.selected_tab_index = selected_tab_index;
  payload.num_tabs = num_tabs;
  payload.timestamp = timestamp.ToInternalValue();

  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandWindow, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

// static
std::unique_ptr<SessionCommand>
PersistentTabRestoreService::Delegate::CreateSelectedNavigationInTabCommand(
    SessionID::id_type tab_id,
    int32_t index,
    base::Time timestamp) {
  SelectedNavigationInTabPayload2 payload;
  payload.id = tab_id;
  payload.index = index;
  payload.timestamp = timestamp.ToInternalValue();
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSelectedNavigationInTab, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

// static
std::unique_ptr<SessionCommand>
PersistentTabRestoreService::Delegate::CreateRestoredEntryCommand(
    SessionID::id_type entry_id) {
  RestoredEntryPayload payload = entry_id;
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandRestoredEntry, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

int PersistentTabRestoreService::Delegate::GetSelectedNavigationIndexToPersist(
    const Tab& tab) {
  const std::vector<SerializedNavigationEntry>& navigations = tab.navigations;
  int selected_index = tab.current_navigation_index;
  int max_index = static_cast<int>(navigations.size());

  // Find the first navigation to persist. We won't persist the selected
  // navigation if client_->ShouldTrackURLForRestore returns false.
  while (selected_index >= 0 &&
         !client_->ShouldTrackURLForRestore(
             navigations[selected_index].virtual_url())) {
    selected_index--;
  }

  if (selected_index != -1)
    return selected_index;

  // Couldn't find a navigation to persist going back, go forward.
  selected_index = tab.current_navigation_index + 1;
  while (selected_index < max_index &&
         !client_->ShouldTrackURLForRestore(
             navigations[selected_index].virtual_url())) {
    selected_index++;
  }

  return (selected_index == max_index) ? -1 : selected_index;
}

void PersistentTabRestoreService::Delegate::OnGotLastSessionCommands(
    std::vector<std::unique_ptr<SessionCommand>> commands) {
  std::vector<std::unique_ptr<TabRestoreService::Entry>> entries;
  CreateEntriesFromCommands(commands, &entries);
  // Closed tabs always go to the end.
  staging_entries_.insert(staging_entries_.end(),
                          make_move_iterator(entries.begin()),
                          make_move_iterator(entries.end()));
  load_state_ |= LOADED_LAST_TABS;
  LoadStateChanged();
}

void PersistentTabRestoreService::Delegate::CreateEntriesFromCommands(
    const std::vector<std::unique_ptr<SessionCommand>>& commands,
    std::vector<std::unique_ptr<Entry>>* loaded_entries) {
  if (tab_restore_service_helper_->entries().size() == kMaxEntries)
    return;

  // Iterate through the commands, populating |entries|.
  std::vector<std::unique_ptr<Entry>> entries;
  // If non-null we're processing the navigations of this tab.
  Tab* current_tab = nullptr;
  // If non-null we're processing the tabs of this window.
  Window* current_window = nullptr;
  // If > 0, we've gotten a window command but not all the tabs yet.
  int pending_window_tabs = 0;
  for (auto i = commands.begin(); i != commands.end(); ++i) {
    const SessionCommand& command = *(*i);
    switch (command.id()) {
      case kCommandRestoredEntry: {
        if (pending_window_tabs > 0) {
          // Should never receive a restored command while waiting for all the
          // tabs in a window.
          return;
        }

        current_tab = nullptr;
        current_window = nullptr;

        RestoredEntryPayload payload;
        if (!command.GetPayload(&payload, sizeof(payload)))
          return;
        RemoveEntryByID(payload, &entries);
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

        RemoveEntryByID(payload.window_id, &entries);

        entries.push_back(base::MakeUnique<Window>());
        current_window = static_cast<Window*>(entries.back().get());
        current_window->selected_tab_index = payload.selected_tab_index;
        current_window->timestamp =
            base::Time::FromInternalValue(payload.timestamp);
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
          current_window->tabs.push_back(base::MakeUnique<Tab>());
          current_tab = current_window->tabs.back().get();
          if (--pending_window_tabs == 0)
            current_window = nullptr;
        } else {
          RemoveEntryByID(payload.id, &entries);
          entries.push_back(base::MakeUnique<Tab>());
          current_tab = static_cast<Tab*>(entries.back().get());
          current_tab->timestamp =
              base::Time::FromInternalValue(payload.timestamp);
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
        if (!RestoreUpdateTabNavigationCommand(command,
                                               &current_tab->navigations.back(),
                                               &tab_id)) {
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

      case kCommandSetWindowAppName: {
        if (!current_window) {
          // We should have created a window already.
          NOTREACHED();
          return;
        }

        SessionID::id_type window_id;
        std::string app_name;
        if (!RestoreSetWindowAppNameCommand(command, &window_id, &app_name))
          return;

        current_window->app_name.swap(app_name);
        break;
      }

      case kCommandSetExtensionAppID: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        SessionID::id_type tab_id;
        std::string extension_app_id;
        if (!RestoreSetTabExtensionAppIDCommand(command,
                                                &tab_id,
                                                &extension_app_id)) {
          return;
        }
        current_tab->extension_app_id.swap(extension_app_id);
        break;
      }

      case kCommandSetTabUserAgentOverride: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        SessionID::id_type tab_id;
        std::string user_agent_override;
        if (!RestoreSetTabUserAgentOverrideCommand(command,
                                                   &tab_id,
                                                   &user_agent_override)) {
          return;
        }
        current_tab->user_agent_override.swap(user_agent_override);
        break;
      }

      default:
        // Unknown type, usually indicates corruption of file. Ignore it.
        return;
    }
  }

  // If there was corruption some of the entries won't be valid.
  ValidateAndDeleteEmptyEntries(&entries);

  loaded_entries->swap(entries);
}

// static
void PersistentTabRestoreService::Delegate::ValidateAndDeleteEmptyEntries(
    std::vector<std::unique_ptr<Entry>>* entries) {
  std::vector<std::unique_ptr<Entry>> valid_entries;

  // Iterate from the back so that we keep the most recently closed entries.
  for (auto i = entries->rbegin(); i != entries->rend(); ++i) {
    if (TabRestoreServiceHelper::ValidateEntry(**i))
      valid_entries.push_back(std::move(*i));
  }
  // NOTE: at this point the entries are ordered with newest at the front.
  entries->swap(valid_entries);
}

void PersistentTabRestoreService::Delegate::OnGotPreviousSession(
    std::vector<std::unique_ptr<SessionWindow>> windows,
    SessionID::id_type ignored_active_window) {
  std::vector<std::unique_ptr<Entry>> entries;
  CreateEntriesFromWindows(&windows, &entries);
  // Previous session tabs go first.
  staging_entries_.insert(staging_entries_.begin(),
                          make_move_iterator(entries.begin()),
                          make_move_iterator(entries.end()));
  load_state_ |= LOADED_LAST_SESSION;
  LoadStateChanged();
}

bool PersistentTabRestoreService::Delegate::ConvertSessionWindowToWindow(
    SessionWindow* session_window,
    Window* window) {
  for (size_t i = 0; i < session_window->tabs.size(); ++i) {
    if (!session_window->tabs[i]->navigations.empty()) {
      window->tabs.push_back(base::MakeUnique<Tab>());
      Tab& tab = *window->tabs.back();
      tab.pinned = session_window->tabs[i]->pinned;
      tab.navigations.swap(session_window->tabs[i]->navigations);
      tab.current_navigation_index =
          session_window->tabs[i]->current_navigation_index;
      tab.extension_app_id = session_window->tabs[i]->extension_app_id;
      tab.timestamp = base::Time();
    }
  }
  if (window->tabs.empty())
    return false;

  window->selected_tab_index =
      std::min(session_window->selected_tab_index,
               static_cast<int>(window->tabs.size() - 1));
  window->timestamp = base::Time();
  return true;
}

void PersistentTabRestoreService::Delegate::LoadStateChanged() {
  if ((load_state_ & (LOADED_LAST_TABS | LOADED_LAST_SESSION)) !=
      (LOADED_LAST_TABS | LOADED_LAST_SESSION)) {
    // Still waiting on previous session or previous tabs.
    return;
  }

  // We're done loading.
  load_state_ ^= LOADING;

  const Entries& entries = tab_restore_service_helper_->entries();
  if (staging_entries_.empty() || entries.size() >= kMaxEntries) {
    staging_entries_.clear();
    tab_restore_service_helper_->NotifyLoaded();
    return;
  }

  if (staging_entries_.size() + entries.size() > kMaxEntries) {
    // If we add all the staged entries we'll end up with more than
    // kMaxEntries. Delete entries such that we only end up with at most
    // kMaxEntries.
    int surplus = kMaxEntries - entries.size();
    CHECK_LE(0, surplus);
    CHECK_GE(static_cast<int>(staging_entries_.size()), surplus);
    staging_entries_.erase(
        staging_entries_.begin() + (kMaxEntries - entries.size()),
        staging_entries_.end());
  }

  // And add them.
  for (auto& staging_entry : staging_entries_) {
    staging_entry->from_last_session = true;
    tab_restore_service_helper_->AddEntry(std::move(staging_entry), false,
                                          false);
  }

  staging_entries_.clear();
  entries_to_write_ = 0;

  tab_restore_service_helper_->PruneEntries();
  tab_restore_service_helper_->NotifyTabsChanged();

  tab_restore_service_helper_->NotifyLoaded();
}

// PersistentTabRestoreService -------------------------------------------------

PersistentTabRestoreService::PersistentTabRestoreService(
    std::unique_ptr<TabRestoreServiceClient> client,
    TimeFactory* time_factory)
    : client_(std::move(client)),
      delegate_(new Delegate(client_.get())),
      helper_(this, delegate_.get(), client_.get(), time_factory) {
  delegate_->set_tab_restore_service_helper(&helper_);
}

PersistentTabRestoreService::~PersistentTabRestoreService() {}

void PersistentTabRestoreService::AddObserver(
    TabRestoreServiceObserver* observer) {
  helper_.AddObserver(observer);
}

void PersistentTabRestoreService::RemoveObserver(
    TabRestoreServiceObserver* observer) {
  helper_.RemoveObserver(observer);
}

void PersistentTabRestoreService::CreateHistoricalTab(LiveTab* live_tab,
                                                      int index) {
  helper_.CreateHistoricalTab(live_tab, index);
}

void PersistentTabRestoreService::BrowserClosing(LiveTabContext* context) {
  helper_.BrowserClosing(context);
}

void PersistentTabRestoreService::BrowserClosed(LiveTabContext* context) {
  helper_.BrowserClosed(context);
}

void PersistentTabRestoreService::ClearEntries() {
  helper_.ClearEntries();
}

const TabRestoreService::Entries& PersistentTabRestoreService::entries() const {
  return helper_.entries();
}

std::vector<LiveTab*> PersistentTabRestoreService::RestoreMostRecentEntry(
    LiveTabContext* context) {
  return helper_.RestoreMostRecentEntry(context);
}

std::unique_ptr<TabRestoreService::Tab>
PersistentTabRestoreService::RemoveTabEntryById(SessionID::id_type id) {
  return helper_.RemoveTabEntryById(id);
}

std::vector<LiveTab*> PersistentTabRestoreService::RestoreEntryById(
    LiveTabContext* context,
    SessionID::id_type id,
    WindowOpenDisposition disposition) {
  return helper_.RestoreEntryById(context, id, disposition);
}

bool PersistentTabRestoreService::IsLoaded() const {
  return delegate_->IsLoaded();
}

void PersistentTabRestoreService::DeleteLastSession() {
  return delegate_->DeleteLastSession();
}

bool PersistentTabRestoreService::IsRestoring() const {
  return helper_.IsRestoring();
}

void PersistentTabRestoreService::Shutdown() {
  return delegate_->Shutdown();
}

void PersistentTabRestoreService::LoadTabsFromLastSession() {
  delegate_->LoadTabsFromLastSession();
}

TabRestoreService::Entries* PersistentTabRestoreService::mutable_entries() {
  return &helper_.entries_;
}

void PersistentTabRestoreService::PruneEntries() {
  helper_.PruneEntries();
}

}  // namespace sessions
