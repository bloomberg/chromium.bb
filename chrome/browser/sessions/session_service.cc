// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/threading/thread.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_command.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

#if defined(OS_MACOSX)
#include "chrome/browser/app_controller_mac.h"
#endif

using base::Time;
using content::NavigationEntry;
using content::WebContents;
using sessions::SerializedNavigationEntry;

// Identifier for commands written to file.
static const SessionCommand::id_type kCommandSetTabWindow = 0;
// OBSOLETE Superseded by kCommandSetWindowBounds3.
// static const SessionCommand::id_type kCommandSetWindowBounds = 1;
static const SessionCommand::id_type kCommandSetTabIndexInWindow = 2;
// Original kCommandTabClosed/kCommandWindowClosed. See comment in
// MigrateClosedPayload for details on why they were replaced.
static const SessionCommand::id_type kCommandTabClosedObsolete = 3;
static const SessionCommand::id_type kCommandWindowClosedObsolete = 4;
static const SessionCommand::id_type
    kCommandTabNavigationPathPrunedFromBack = 5;
static const SessionCommand::id_type kCommandUpdateTabNavigation = 6;
static const SessionCommand::id_type kCommandSetSelectedNavigationIndex = 7;
static const SessionCommand::id_type kCommandSetSelectedTabInIndex = 8;
static const SessionCommand::id_type kCommandSetWindowType = 9;
// OBSOLETE Superseded by kCommandSetWindowBounds3. Except for data migration.
// static const SessionCommand::id_type kCommandSetWindowBounds2 = 10;
static const SessionCommand::id_type
    kCommandTabNavigationPathPrunedFromFront = 11;
static const SessionCommand::id_type kCommandSetPinnedState = 12;
static const SessionCommand::id_type kCommandSetExtensionAppID = 13;
static const SessionCommand::id_type kCommandSetWindowBounds3 = 14;
static const SessionCommand::id_type kCommandSetWindowAppName = 15;
static const SessionCommand::id_type kCommandTabClosed = 16;
static const SessionCommand::id_type kCommandWindowClosed = 17;
static const SessionCommand::id_type kCommandSetTabUserAgentOverride = 18;
static const SessionCommand::id_type kCommandSessionStorageAssociated = 19;
static const SessionCommand::id_type kCommandSetActiveWindow = 20;

// Every kWritesPerReset commands triggers recreating the file.
static const int kWritesPerReset = 250;

namespace {

// Various payload structures.
struct ClosedPayload {
  SessionID::id_type id;
  int64 close_time;
};

struct WindowBoundsPayload2 {
  SessionID::id_type window_id;
  int32 x;
  int32 y;
  int32 w;
  int32 h;
  bool is_maximized;
};

struct WindowBoundsPayload3 {
  SessionID::id_type window_id;
  int32 x;
  int32 y;
  int32 w;
  int32 h;
  int32 show_state;
};

typedef SessionID::id_type ActiveWindowPayload;

struct IDAndIndexPayload {
  SessionID::id_type id;
  int32 index;
};

typedef IDAndIndexPayload TabIndexInWindowPayload;

typedef IDAndIndexPayload TabNavigationPathPrunedFromBackPayload;

typedef IDAndIndexPayload SelectedNavigationIndexPayload;

typedef IDAndIndexPayload SelectedTabInIndexPayload;

typedef IDAndIndexPayload WindowTypePayload;

typedef IDAndIndexPayload TabNavigationPathPrunedFromFrontPayload;

struct PinnedStatePayload {
  SessionID::id_type tab_id;
  bool pinned_state;
};

// Persisted versions of ui::WindowShowState that are written to disk and can
// never change.
enum PersistedWindowShowState {
  // SHOW_STATE_DEFAULT (0) never persisted.
  PERSISTED_SHOW_STATE_NORMAL = 1,
  PERSISTED_SHOW_STATE_MINIMIZED = 2,
  PERSISTED_SHOW_STATE_MAXIMIZED = 3,
  // SHOW_STATE_INACTIVE (4) never persisted.
  PERSISTED_SHOW_STATE_FULLSCREEN = 5,
  PERSISTED_SHOW_STATE_DETACHED_DEPRECATED = 6,
  PERSISTED_SHOW_STATE_END = 6
};

// Assert to ensure PersistedWindowShowState is updated if ui::WindowShowState
// is changed.
COMPILE_ASSERT(ui::SHOW_STATE_END ==
                   static_cast<ui::WindowShowState>(PERSISTED_SHOW_STATE_END),
               persisted_show_state_mismatch);

// Returns the show state to store to disk based |state|.
PersistedWindowShowState ShowStateToPersistedShowState(
    ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_NORMAL:
      return PERSISTED_SHOW_STATE_NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return PERSISTED_SHOW_STATE_MINIMIZED;
    case ui::SHOW_STATE_MAXIMIZED:
      return PERSISTED_SHOW_STATE_MAXIMIZED;
    case ui::SHOW_STATE_FULLSCREEN:
      return PERSISTED_SHOW_STATE_FULLSCREEN;

    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_INACTIVE:
      return PERSISTED_SHOW_STATE_NORMAL;

    case ui::SHOW_STATE_END:
      break;
  }
  NOTREACHED();
  return PERSISTED_SHOW_STATE_NORMAL;
}

// Lints show state values when read back from persited disk.
ui::WindowShowState PersistedShowStateToShowState(int state) {
  switch (state) {
    case PERSISTED_SHOW_STATE_NORMAL:
      return ui::SHOW_STATE_NORMAL;
    case PERSISTED_SHOW_STATE_MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case PERSISTED_SHOW_STATE_MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case PERSISTED_SHOW_STATE_FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case PERSISTED_SHOW_STATE_DETACHED_DEPRECATED:
      return ui::SHOW_STATE_NORMAL;
  }
  NOTREACHED();
  return ui::SHOW_STATE_NORMAL;
}

// Migrates a |ClosedPayload|, returning true on success (migration was
// necessary and happened), or false (migration was not necessary or was not
// successful).
bool MigrateClosedPayload(const SessionCommand& command,
                          ClosedPayload* payload) {
#if defined(OS_CHROMEOS)
  // Pre M17 versions of chromeos were 32bit. Post M17 is 64 bit. Apparently the
  // 32 bit versions of chrome on pre M17 resulted in a sizeof 12 for the
  // ClosedPayload, where as post M17 64-bit gives a sizeof 16 (presumably the
  // struct is padded).
  if ((command.id() == kCommandWindowClosedObsolete ||
       command.id() == kCommandTabClosedObsolete) &&
      command.size() == 12 && sizeof(payload->id) == 4 &&
      sizeof(payload->close_time) == 8) {
    memcpy(&payload->id, command.contents(), 4);
    memcpy(&payload->close_time, command.contents() + 4, 8);
    return true;
  } else {
    return false;
  }
#else
  return false;
#endif
}

}  // namespace

// SessionService -------------------------------------------------------------

SessionService::SessionService(Profile* profile)
    : BaseSessionService(SESSION_RESTORE, profile, base::FilePath()),
      has_open_trackable_browsers_(false),
      move_on_new_browser_(false),
      save_delay_in_millis_(base::TimeDelta::FromMilliseconds(2500)),
      save_delay_in_mins_(base::TimeDelta::FromMinutes(10)),
      save_delay_in_hrs_(base::TimeDelta::FromHours(8)),
      force_browser_not_alive_with_no_windows_(false) {
  Init();
}

SessionService::SessionService(const base::FilePath& save_path)
    : BaseSessionService(SESSION_RESTORE, NULL, save_path),
      has_open_trackable_browsers_(false),
      move_on_new_browser_(false),
      save_delay_in_millis_(base::TimeDelta::FromMilliseconds(2500)),
      save_delay_in_mins_(base::TimeDelta::FromMinutes(10)),
      save_delay_in_hrs_(base::TimeDelta::FromHours(8)),
      force_browser_not_alive_with_no_windows_(false)  {
  Init();
}

SessionService::~SessionService() {
  // The BrowserList should outlive the SessionService since it's static and
  // the SessionService is a KeyedService.
  BrowserList::RemoveObserver(this);
  Save();
}

bool SessionService::RestoreIfNecessary(const std::vector<GURL>& urls_to_open) {
  return RestoreIfNecessary(urls_to_open, NULL);
}

void SessionService::ResetFromCurrentBrowsers() {
  ScheduleReset();
}

void SessionService::MoveCurrentSessionToLastSession() {
  pending_tab_close_ids_.clear();
  window_closing_ids_.clear();
  pending_window_close_ids_.clear();

  Save();

  RunTaskOnBackendThread(
      FROM_HERE, base::Bind(&SessionBackend::MoveCurrentSessionToLastSession,
                            backend()));
}

void SessionService::SetTabWindow(const SessionID& window_id,
                                  const SessionID& tab_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetTabWindowCommand(window_id, tab_id));
}

void SessionService::SetWindowBounds(const SessionID& window_id,
                                     const gfx::Rect& bounds,
                                     ui::WindowShowState show_state) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetWindowBoundsCommand(window_id, bounds, show_state));
}

void SessionService::SetTabIndexInWindow(const SessionID& window_id,
                                         const SessionID& tab_id,
                                         int new_index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetTabIndexInWindowCommand(tab_id, new_index));
}

void SessionService::SetPinnedState(const SessionID& window_id,
                                    const SessionID& tab_id,
                                    bool is_pinned) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreatePinnedStateCommand(tab_id, is_pinned));
}

void SessionService::TabClosed(const SessionID& window_id,
                               const SessionID& tab_id,
                               bool closed_by_user_gesture) {
  if (!tab_id.id())
    return;  // Hapens when the tab is replaced.

  if (!ShouldTrackChangesToWindow(window_id))
    return;

  IdToRange::iterator i = tab_to_available_range_.find(tab_id.id());
  if (i != tab_to_available_range_.end())
    tab_to_available_range_.erase(i);

  if (find(pending_window_close_ids_.begin(), pending_window_close_ids_.end(),
           window_id.id()) != pending_window_close_ids_.end()) {
    // Tab is in last window. Don't commit it immediately, instead add it to the
    // list of tabs to close. If the user creates another window, the close is
    // committed.
    pending_tab_close_ids_.insert(tab_id.id());
  } else if (find(window_closing_ids_.begin(), window_closing_ids_.end(),
                  window_id.id()) != window_closing_ids_.end() ||
             !IsOnlyOneTabLeft() ||
             closed_by_user_gesture) {
    // Close is the result of one of the following:
    // . window close (and it isn't the last window).
    // . closing a tab and there are other windows/tabs open.
    // . closed by a user gesture.
    // In all cases we need to mark the tab as explicitly closed.
    ScheduleCommand(CreateTabClosedCommand(tab_id.id()));
  } else {
    // User closed the last tab in the last tabbed browser. Don't mark the
    // tab closed.
    pending_tab_close_ids_.insert(tab_id.id());
    has_open_trackable_browsers_ = false;
  }
}

void SessionService::WindowOpened(Browser* browser) {
  if (!ShouldTrackBrowser(browser))
    return;

  AppType app_type = browser->is_app() ? TYPE_APP : TYPE_NORMAL;
  RestoreIfNecessary(std::vector<GURL>(), browser);
  SetWindowType(browser->session_id(), browser->type(), app_type);
  SetWindowAppName(browser->session_id(), browser->app_name());
}

void SessionService::WindowClosing(const SessionID& window_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // The window is about to close. If there are other tabbed browsers with the
  // same original profile commit the close immediately.
  //
  // NOTE: if the user chooses the exit menu item session service is destroyed
  // and this code isn't hit.
  if (has_open_trackable_browsers_) {
    // Closing a window can never make has_open_trackable_browsers_ go from
    // false to true, so only update it if already true.
    has_open_trackable_browsers_ = HasOpenTrackableBrowsers(window_id);
  }
  bool use_pending_close = !has_open_trackable_browsers_;
  if (!use_pending_close) {
    // Somewhat outside of "normal behavior" is profile locking.  In this case
    // (when IsSiginRequired has already been set True), we're closing all
    // browser windows in turn but want them all to be restored when the user
    // unlocks.  To accomplish this, we do a "pending close" on all windows
    // instead of just the last one (which has no open_trackable_browsers).
    // http://crbug.com/356818
    //
    // Some editions (like iOS) don't have a profile_manager and some tests
    // don't supply one so be lenient.
    if (g_browser_process) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      if (profile_manager) {
        ProfileInfoCache& profile_info =
            profile_manager->GetProfileInfoCache();
        size_t profile_index = profile_info.GetIndexOfProfileWithPath(
            profile()->GetPath());
        use_pending_close = profile_index != std::string::npos &&
            profile_info.ProfileIsSigninRequiredAtIndex(profile_index);
      }
    }
  }
  if (use_pending_close)
    pending_window_close_ids_.insert(window_id.id());
  else
    window_closing_ids_.insert(window_id.id());
}

void SessionService::WindowClosed(const SessionID& window_id) {
  if (!ShouldTrackChangesToWindow(window_id)) {
    // The last window may be one that is not tracked.
    MaybeDeleteSessionOnlyData();
    return;
  }

  windows_tracking_.erase(window_id.id());

  if (window_closing_ids_.find(window_id.id()) != window_closing_ids_.end()) {
    window_closing_ids_.erase(window_id.id());
    ScheduleCommand(CreateWindowClosedCommand(window_id.id()));
  } else if (pending_window_close_ids_.find(window_id.id()) ==
             pending_window_close_ids_.end()) {
    // We'll hit this if user closed the last tab in a window.
    has_open_trackable_browsers_ = HasOpenTrackableBrowsers(window_id);
    if (!has_open_trackable_browsers_)
      pending_window_close_ids_.insert(window_id.id());
    else
      ScheduleCommand(CreateWindowClosedCommand(window_id.id()));
  }
  MaybeDeleteSessionOnlyData();
}

void SessionService::SetWindowType(const SessionID& window_id,
                                   Browser::Type type,
                                   AppType app_type) {
  if (!should_track_changes_for_browser_type(type, app_type))
    return;

  windows_tracking_.insert(window_id.id());

  // The user created a new tabbed browser with our profile. Commit any
  // pending closes.
  CommitPendingCloses();

  has_open_trackable_browsers_ = true;
  move_on_new_browser_ = true;

  ScheduleCommand(
      CreateSetWindowTypeCommand(window_id, WindowTypeForBrowserType(type)));
}

void SessionService::SetWindowAppName(
    const SessionID& window_id,
    const std::string& app_name) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetTabExtensionAppIDCommand(
                      kCommandSetWindowAppName,
                      window_id.id(),
                      app_name));
}

void SessionService::TabNavigationPathPrunedFromBack(const SessionID& window_id,
                                                     const SessionID& tab_id,
                                                     int count) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  TabNavigationPathPrunedFromBackPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = count;
  SessionCommand* command =
      new SessionCommand(kCommandTabNavigationPathPrunedFromBack,
                         sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  ScheduleCommand(command);
}

void SessionService::TabNavigationPathPrunedFromFront(
    const SessionID& window_id,
    const SessionID& tab_id,
    int count) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // Update the range of indices.
  if (tab_to_available_range_.find(tab_id.id()) !=
      tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id.id()];
    range.first = std::max(0, range.first - count);
    range.second = std::max(0, range.second - count);
  }

  TabNavigationPathPrunedFromFrontPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = count;
  SessionCommand* command =
      new SessionCommand(kCommandTabNavigationPathPrunedFromFront,
                         sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  ScheduleCommand(command);
}

void SessionService::UpdateTabNavigation(
    const SessionID& window_id,
    const SessionID& tab_id,
    const SerializedNavigationEntry& navigation) {
  if (!ShouldTrackEntry(navigation.virtual_url()) ||
      !ShouldTrackChangesToWindow(window_id)) {
    return;
  }

  if (tab_to_available_range_.find(tab_id.id()) !=
      tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id.id()];
    range.first = std::min(navigation.index(), range.first);
    range.second = std::max(navigation.index(), range.second);
  }
  ScheduleCommand(CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation,
                                                   tab_id.id(), navigation));
}

void SessionService::TabRestored(WebContents* tab, bool pinned) {
  SessionTabHelper* session_tab_helper = SessionTabHelper::FromWebContents(tab);
  if (!ShouldTrackChangesToWindow(session_tab_helper->window_id()))
    return;

  BuildCommandsForTab(session_tab_helper->window_id(), tab, -1,
                      pinned, &pending_commands(), NULL);
  StartSaveTimer();
}

void SessionService::SetSelectedNavigationIndex(const SessionID& window_id,
                                                const SessionID& tab_id,
                                                int index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  if (tab_to_available_range_.find(tab_id.id()) !=
      tab_to_available_range_.end()) {
    if (index < tab_to_available_range_[tab_id.id()].first ||
        index > tab_to_available_range_[tab_id.id()].second) {
      // The new index is outside the range of what we've archived, schedule
      // a reset.
      ResetFromCurrentBrowsers();
      return;
    }
  }
  ScheduleCommand(CreateSetSelectedNavigationIndexCommand(tab_id, index));
}

void SessionService::SetSelectedTabInWindow(const SessionID& window_id,
                                            int index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetSelectedTabInWindow(window_id, index));
}

void SessionService::SetTabUserAgentOverride(
    const SessionID& window_id,
    const SessionID& tab_id,
    const std::string& user_agent_override) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetTabUserAgentOverrideCommand(
      kCommandSetTabUserAgentOverride, tab_id.id(), user_agent_override));
}

base::CancelableTaskTracker::TaskId SessionService::GetLastSession(
    const SessionCallback& callback,
    base::CancelableTaskTracker* tracker) {
  // OnGotSessionCommands maps the SessionCommands to browser state, then run
  // the callback.
  return ScheduleGetLastSessionCommands(
      base::Bind(&SessionService::OnGotSessionCommands,
                 base::Unretained(this), callback),
      tracker);
}

void SessionService::Save() {
  bool had_commands = !pending_commands().empty();
  BaseSessionService::Save();
  if (had_commands) {
    RecordSessionUpdateHistogramData(chrome::NOTIFICATION_SESSION_SERVICE_SAVED,
                                     &last_updated_save_time_);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_SESSION_SERVICE_SAVED,
        content::Source<Profile>(profile()),
        content::NotificationService::NoDetails());
  }
}

void SessionService::Init() {
  // Register for the notifications we're interested in.
  registrar_.Add(this, content::NOTIFICATION_NAV_LIST_PRUNED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
  registrar_.Add(
      this, chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      content::NotificationService::AllSources());

  BrowserList::AddObserver(this);
}

bool SessionService::processed_any_commands() {
  return backend()->inited() || !pending_commands().empty();
}

bool SessionService::ShouldNewWindowStartSession() {
  // ChromeOS and OSX have different ideas of application lifetime than
  // the other platforms.
  // On ChromeOS opening a new window should never start a new session.
#if defined(OS_CHROMEOS)
  if (!force_browser_not_alive_with_no_windows_)
    return false;
#endif
  if (!has_open_trackable_browsers_ &&
      !StartupBrowserCreator::InSynchronousProfileLaunch() &&
      !SessionRestore::IsRestoring(profile())
#if defined(OS_MACOSX)
      // On OSX, a new window should not start a new session if it was opened
      // from the dock or the menubar.
      && !app_controller_mac::IsOpeningNewWindow()
#endif  // OS_MACOSX
      ) {
    return true;
  }
  return false;
}

bool SessionService::RestoreIfNecessary(const std::vector<GURL>& urls_to_open,
                                        Browser* browser) {
  if (ShouldNewWindowStartSession()) {
    // We're going from no tabbed browsers to a tabbed browser (and not in
    // process startup), restore the last session.
    if (move_on_new_browser_) {
      // Make the current session the last.
      MoveCurrentSessionToLastSession();
      move_on_new_browser_ = false;
    }
    SessionStartupPref pref = StartupBrowserCreator::GetSessionStartupPref(
        *CommandLine::ForCurrentProcess(), profile());
    if (pref.type == SessionStartupPref::LAST) {
      SessionRestore::RestoreSession(
          profile(), browser,
          browser ? browser->host_desktop_type() : chrome::GetActiveDesktop(),
          browser ? 0 : SessionRestore::ALWAYS_CREATE_TABBED_BROWSER,
          urls_to_open);
      return true;
    }
  }
  return false;
}

void SessionService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  // All of our messages have the NavigationController as the source.
  switch (type) {
    case content::NOTIFICATION_NAV_LIST_PRUNED: {
      WebContents* web_contents =
          content::Source<content::NavigationController>(source).ptr()->
              GetWebContents();
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (!session_tab_helper || web_contents->GetBrowserContext() != profile())
        return;
      content::Details<content::PrunedDetails> pruned_details(details);
      if (pruned_details->from_front) {
        TabNavigationPathPrunedFromFront(
            session_tab_helper->window_id(),
            session_tab_helper->session_id(),
            pruned_details->count);
      } else {
        TabNavigationPathPrunedFromBack(
            session_tab_helper->window_id(),
            session_tab_helper->session_id(),
            web_contents->GetController().GetEntryCount());
      }
      RecordSessionUpdateHistogramData(type,
                                       &last_updated_nav_list_pruned_time_);
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_CHANGED: {
      WebContents* web_contents =
          content::Source<content::NavigationController>(source).ptr()->
              GetWebContents();
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (!session_tab_helper || web_contents->GetBrowserContext() != profile())
        return;
      content::Details<content::EntryChangedDetails> changed(details);
      const SerializedNavigationEntry navigation =
          SerializedNavigationEntry::FromNavigationEntry(
              changed->index, *changed->changed_entry);
      UpdateTabNavigation(session_tab_helper->window_id(),
                          session_tab_helper->session_id(),
                          navigation);
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      WebContents* web_contents =
          content::Source<content::NavigationController>(source).ptr()->
              GetWebContents();
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (!session_tab_helper || web_contents->GetBrowserContext() != profile())
        return;
      int current_entry_index =
          web_contents->GetController().GetCurrentEntryIndex();
      SetSelectedNavigationIndex(
          session_tab_helper->window_id(),
          session_tab_helper->session_id(),
          current_entry_index);
      const SerializedNavigationEntry navigation =
          SerializedNavigationEntry::FromNavigationEntry(
              current_entry_index,
              *web_contents->GetController().GetEntryAtIndex(
                  current_entry_index));
      UpdateTabNavigation(
          session_tab_helper->window_id(),
          session_tab_helper->session_id(),
          navigation);
      content::Details<content::LoadCommittedDetails> changed(details);
      if (changed->type == content::NAVIGATION_TYPE_NEW_PAGE ||
        changed->type == content::NAVIGATION_TYPE_EXISTING_PAGE) {
        RecordSessionUpdateHistogramData(type,
                                         &last_updated_nav_entry_commit_time_);
      }
      break;
    }

    case chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED: {
      extensions::TabHelper* extension_tab_helper =
          content::Source<extensions::TabHelper>(source).ptr();
      if (extension_tab_helper->web_contents()->GetBrowserContext() !=
              profile()) {
        return;
      }
      if (extension_tab_helper->extension_app()) {
        SessionTabHelper* session_tab_helper =
            SessionTabHelper::FromWebContents(
                extension_tab_helper->web_contents());
        SetTabExtensionAppID(session_tab_helper->window_id(),
                             session_tab_helper->session_id(),
                             extension_tab_helper->extension_app()->id());
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

void SessionService::OnBrowserSetLastActive(Browser* browser) {
  if (ShouldTrackBrowser(browser))
    ScheduleCommand(CreateSetActiveWindowCommand(browser->session_id()));
}

void SessionService::SetTabExtensionAppID(
    const SessionID& window_id,
    const SessionID& tab_id,
    const std::string& extension_app_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(CreateSetTabExtensionAppIDCommand(kCommandSetExtensionAppID,
      tab_id.id(), extension_app_id));
}

SessionCommand* SessionService::CreateSetSelectedTabInWindow(
    const SessionID& window_id,
    int index) {
  SelectedTabInIndexPayload payload = { 0 };
  payload.id = window_id.id();
  payload.index = index;
  SessionCommand* command = new SessionCommand(kCommandSetSelectedTabInIndex,
                                 sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetTabWindowCommand(
    const SessionID& window_id,
    const SessionID& tab_id) {
  SessionID::id_type payload[] = { window_id.id(), tab_id.id() };
  SessionCommand* command =
      new SessionCommand(kCommandSetTabWindow, sizeof(payload));
  memcpy(command->contents(), payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetWindowBoundsCommand(
    const SessionID& window_id,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state) {
  WindowBoundsPayload3 payload = { 0 };
  payload.window_id = window_id.id();
  payload.x = bounds.x();
  payload.y = bounds.y();
  payload.w = bounds.width();
  payload.h = bounds.height();
  payload.show_state = ShowStateToPersistedShowState(show_state);
  SessionCommand* command = new SessionCommand(kCommandSetWindowBounds3,
                                               sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetTabIndexInWindowCommand(
    const SessionID& tab_id,
    int new_index) {
  TabIndexInWindowPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = new_index;
  SessionCommand* command =
      new SessionCommand(kCommandSetTabIndexInWindow, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateTabClosedCommand(
    const SessionID::id_type tab_id) {
  ClosedPayload payload;
  // Because of what appears to be a compiler bug setting payload to {0} doesn't
  // set the padding to 0, resulting in Purify reporting an UMR when we write
  // the structure to disk. To avoid this we explicitly memset the struct.
  memset(&payload, 0, sizeof(payload));
  payload.id = tab_id;
  payload.close_time = Time::Now().ToInternalValue();
  SessionCommand* command =
      new SessionCommand(kCommandTabClosed, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateWindowClosedCommand(
    const SessionID::id_type window_id) {
  ClosedPayload payload;
  // See comment in CreateTabClosedCommand as to why we do this.
  memset(&payload, 0, sizeof(payload));
  payload.id = window_id;
  payload.close_time = Time::Now().ToInternalValue();
  SessionCommand* command =
      new SessionCommand(kCommandWindowClosed, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetSelectedNavigationIndexCommand(
    const SessionID& tab_id,
    int index) {
  SelectedNavigationIndexPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = index;
  SessionCommand* command = new SessionCommand(
      kCommandSetSelectedNavigationIndex, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSetWindowTypeCommand(
    const SessionID& window_id,
    WindowType type) {
  WindowTypePayload payload = { 0 };
  payload.id = window_id.id();
  payload.index = static_cast<int32>(type);
  SessionCommand* command = new SessionCommand(
      kCommandSetWindowType, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreatePinnedStateCommand(
    const SessionID& tab_id,
    bool is_pinned) {
  PinnedStatePayload payload = { 0 };
  payload.tab_id = tab_id.id();
  payload.pinned_state = is_pinned;
  SessionCommand* command =
      new SessionCommand(kCommandSetPinnedState, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

SessionCommand* SessionService::CreateSessionStorageAssociatedCommand(
    const SessionID& tab_id,
    const std::string& session_storage_persistent_id) {
  Pickle pickle;
  pickle.WriteInt(tab_id.id());
  pickle.WriteString(session_storage_persistent_id);
  return new SessionCommand(kCommandSessionStorageAssociated, pickle);
}

SessionCommand* SessionService::CreateSetActiveWindowCommand(
    const SessionID& window_id) {
  ActiveWindowPayload payload = 0;
  payload = window_id.id();
  SessionCommand* command =
      new SessionCommand(kCommandSetActiveWindow, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

void SessionService::OnGotSessionCommands(
    const SessionCallback& callback,
    ScopedVector<SessionCommand> commands) {
  ScopedVector<SessionWindow> valid_windows;
  SessionID::id_type active_window_id = 0;

  RestoreSessionFromCommands(
      commands.get(), &valid_windows.get(), &active_window_id);
  callback.Run(valid_windows.Pass(), active_window_id);
}

void SessionService::RestoreSessionFromCommands(
    const std::vector<SessionCommand*>& commands,
    std::vector<SessionWindow*>* valid_windows,
    SessionID::id_type* active_window_id) {
  std::map<int, SessionTab*> tabs;
  std::map<int, SessionWindow*> windows;

  VLOG(1) << "RestoreSessionFromCommands " << commands.size();
  if (CreateTabsAndWindows(commands, &tabs, &windows, active_window_id)) {
    AddTabsToWindows(&tabs, &windows);
    SortTabsBasedOnVisualOrderAndPrune(&windows, valid_windows);
    UpdateSelectedTabIndex(valid_windows);
  }
  STLDeleteValues(&tabs);
  // Don't delete conents of windows, that is done by the caller as all
  // valid windows are added to valid_windows.
}

void SessionService::UpdateSelectedTabIndex(
    std::vector<SessionWindow*>* windows) {
  for (std::vector<SessionWindow*>::const_iterator i = windows->begin();
       i != windows->end(); ++i) {
    // See note in SessionWindow as to why we do this.
    int new_index = 0;
    for (std::vector<SessionTab*>::const_iterator j = (*i)->tabs.begin();
         j != (*i)->tabs.end(); ++j) {
      if ((*j)->tab_visual_index == (*i)->selected_tab_index) {
        new_index = static_cast<int>(j - (*i)->tabs.begin());
        break;
      }
    }
    (*i)->selected_tab_index = new_index;
  }
}

SessionWindow* SessionService::GetWindow(
    SessionID::id_type window_id,
    IdToSessionWindow* windows) {
  std::map<int, SessionWindow*>::iterator i = windows->find(window_id);
  if (i == windows->end()) {
    SessionWindow* window = new SessionWindow();
    window->window_id.set_id(window_id);
    (*windows)[window_id] = window;
    return window;
  }
  return i->second;
}

SessionTab* SessionService::GetTab(
    SessionID::id_type tab_id,
    IdToSessionTab* tabs) {
  DCHECK(tabs);
  std::map<int, SessionTab*>::iterator i = tabs->find(tab_id);
  if (i == tabs->end()) {
    SessionTab* tab = new SessionTab();
    tab->tab_id.set_id(tab_id);
    (*tabs)[tab_id] = tab;
    return tab;
  }
  return i->second;
}

std::vector<SerializedNavigationEntry>::iterator
  SessionService::FindClosestNavigationWithIndex(
    std::vector<SerializedNavigationEntry>* navigations,
    int index) {
  DCHECK(navigations);
  for (std::vector<SerializedNavigationEntry>::iterator
           i = navigations->begin(); i != navigations->end(); ++i) {
    if (i->index() >= index)
      return i;
  }
  return navigations->end();
}

// Function used in sorting windows. Sorting is done based on window id. As
// window ids increment for each new window, this effectively sorts by creation
// time.
static bool WindowOrderSortFunction(const SessionWindow* w1,
                                    const SessionWindow* w2) {
  return w1->window_id.id() < w2->window_id.id();
}

// Compares the two tabs based on visual index.
static bool TabVisualIndexSortFunction(const SessionTab* t1,
                                       const SessionTab* t2) {
  const int delta = t1->tab_visual_index - t2->tab_visual_index;
  return delta == 0 ? (t1->tab_id.id() < t2->tab_id.id()) : (delta < 0);
}

void SessionService::SortTabsBasedOnVisualOrderAndPrune(
    std::map<int, SessionWindow*>* windows,
    std::vector<SessionWindow*>* valid_windows) {
  std::map<int, SessionWindow*>::iterator i = windows->begin();
  while (i != windows->end()) {
    SessionWindow* window = i->second;
    AppType app_type = window->app_name.empty() ? TYPE_NORMAL : TYPE_APP;
    if (window->tabs.empty() || window->is_constrained ||
        !should_track_changes_for_browser_type(
            static_cast<Browser::Type>(window->type),
            app_type)) {
      delete window;
      windows->erase(i++);
    } else {
      // Valid window; sort the tabs and add it to the list of valid windows.
      std::sort(window->tabs.begin(), window->tabs.end(),
                &TabVisualIndexSortFunction);
      // Otherwise, add the window such that older windows appear first.
      if (valid_windows->empty()) {
        valid_windows->push_back(window);
      } else {
        valid_windows->insert(
            std::upper_bound(valid_windows->begin(), valid_windows->end(),
                             window, &WindowOrderSortFunction),
            window);
      }
      ++i;
    }
  }
}

void SessionService::AddTabsToWindows(std::map<int, SessionTab*>* tabs,
                                      std::map<int, SessionWindow*>* windows) {
  VLOG(1) << "AddTabsToWindws";
  VLOG(1) << "Tabs " << tabs->size() << ", windows " << windows->size();
  std::map<int, SessionTab*>::iterator i = tabs->begin();
  while (i != tabs->end()) {
    SessionTab* tab = i->second;
    if (tab->window_id.id() && !tab->navigations.empty()) {
      SessionWindow* window = GetWindow(tab->window_id.id(), windows);
      window->tabs.push_back(tab);
      tabs->erase(i++);

      // See note in SessionTab as to why we do this.
      std::vector<SerializedNavigationEntry>::iterator j =
          FindClosestNavigationWithIndex(&(tab->navigations),
                                         tab->current_navigation_index);
      if (j == tab->navigations.end()) {
        tab->current_navigation_index =
            static_cast<int>(tab->navigations.size() - 1);
      } else {
        tab->current_navigation_index =
            static_cast<int>(j - tab->navigations.begin());
      }
    } else {
      // Never got a set tab index in window, or tabs are empty, nothing
      // to do.
      ++i;
    }
  }
}

bool SessionService::CreateTabsAndWindows(
    const std::vector<SessionCommand*>& data,
    std::map<int, SessionTab*>* tabs,
    std::map<int, SessionWindow*>* windows,
    SessionID::id_type* active_window_id) {
  // If the file is corrupt (command with wrong size, or unknown command), we
  // still return true and attempt to restore what we we can.
  VLOG(1) << "CreateTabsAndWindows";

  startup_metric_utils::ScopedSlowStartupUMA
      scoped_timer("Startup.SlowStartupSessionServiceCreateTabsAndWindows");

  for (std::vector<SessionCommand*>::const_iterator i = data.begin();
       i != data.end(); ++i) {
    const SessionCommand::id_type kCommandSetWindowBounds2 = 10;
    const SessionCommand* command = *i;

    VLOG(1) << "Read command " << (int) command->id();
    switch (command->id()) {
      case kCommandSetTabWindow: {
        SessionID::id_type payload[2];
        if (!command->GetPayload(payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetTab(payload[1], tabs)->window_id.set_id(payload[0]);
        break;
      }

      // This is here for forward migration only.  New data is saved with
      // |kCommandSetWindowBounds3|.
      case kCommandSetWindowBounds2: {
        WindowBoundsPayload2 payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(payload.window_id, windows)->bounds.SetRect(payload.x,
                                                              payload.y,
                                                              payload.w,
                                                              payload.h);
        GetWindow(payload.window_id, windows)->show_state =
            payload.is_maximized ?
                ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL;
        break;
      }

      case kCommandSetWindowBounds3: {
        WindowBoundsPayload3 payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(payload.window_id, windows)->bounds.SetRect(payload.x,
                                                              payload.y,
                                                              payload.w,
                                                              payload.h);
        GetWindow(payload.window_id, windows)->show_state =
            PersistedShowStateToShowState(payload.show_state);
        break;
      }

      case kCommandSetTabIndexInWindow: {
        TabIndexInWindowPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetTab(payload.id, tabs)->tab_visual_index = payload.index;
        break;
      }

      case kCommandTabClosedObsolete:
      case kCommandWindowClosedObsolete:
      case kCommandTabClosed:
      case kCommandWindowClosed: {
        ClosedPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)) &&
            !MigrateClosedPayload(*command, &payload)) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        if (command->id() == kCommandTabClosed ||
            command->id() == kCommandTabClosedObsolete) {
          delete GetTab(payload.id, tabs);
          tabs->erase(payload.id);
        } else {
          delete GetWindow(payload.id, windows);
          windows->erase(payload.id);
        }
        break;
      }

      case kCommandTabNavigationPathPrunedFromBack: {
        TabNavigationPathPrunedFromBackPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        SessionTab* tab = GetTab(payload.id, tabs);
        tab->navigations.erase(
            FindClosestNavigationWithIndex(&(tab->navigations), payload.index),
            tab->navigations.end());
        break;
      }

      case kCommandTabNavigationPathPrunedFromFront: {
        TabNavigationPathPrunedFromFrontPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)) ||
            payload.index <= 0) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        SessionTab* tab = GetTab(payload.id, tabs);

        // Update the selected navigation index.
        tab->current_navigation_index =
            std::max(-1, tab->current_navigation_index - payload.index);

        // And update the index of existing navigations.
        for (std::vector<SerializedNavigationEntry>::iterator
                 i = tab->navigations.begin();
             i != tab->navigations.end();) {
          i->set_index(i->index() - payload.index);
          if (i->index() < 0)
            i = tab->navigations.erase(i);
          else
            ++i;
        }
        break;
      }

      case kCommandUpdateTabNavigation: {
        SerializedNavigationEntry navigation;
        SessionID::id_type tab_id;
        if (!RestoreUpdateTabNavigationCommand(
                *command, &navigation, &tab_id)) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        SessionTab* tab = GetTab(tab_id, tabs);
        std::vector<SerializedNavigationEntry>::iterator i =
            FindClosestNavigationWithIndex(&(tab->navigations),
                                           navigation.index());
        if (i != tab->navigations.end() && i->index() == navigation.index())
          *i = navigation;
        else
          tab->navigations.insert(i, navigation);
        break;
      }

      case kCommandSetSelectedNavigationIndex: {
        SelectedNavigationIndexPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetTab(payload.id, tabs)->current_navigation_index = payload.index;
        break;
      }

      case kCommandSetSelectedTabInIndex: {
        SelectedTabInIndexPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(payload.id, windows)->selected_tab_index = payload.index;
        break;
      }

      case kCommandSetWindowType: {
        WindowTypePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(payload.id, windows)->is_constrained = false;
        GetWindow(payload.id, windows)->type =
            BrowserTypeForWindowType(
                static_cast<WindowType>(payload.index));
        break;
      }

      case kCommandSetPinnedState: {
        PinnedStatePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetTab(payload.tab_id, tabs)->pinned = payload.pinned_state;
        break;
      }

      case kCommandSetWindowAppName: {
        SessionID::id_type window_id;
        std::string app_name;
        if (!RestoreSetWindowAppNameCommand(*command, &window_id, &app_name))
          return true;

        GetWindow(window_id, windows)->app_name.swap(app_name);
        break;
      }

      case kCommandSetExtensionAppID: {
        SessionID::id_type tab_id;
        std::string extension_app_id;
        if (!RestoreSetTabExtensionAppIDCommand(
                *command, &tab_id, &extension_app_id)) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }

        GetTab(tab_id, tabs)->extension_app_id.swap(extension_app_id);
        break;
      }

      case kCommandSetTabUserAgentOverride: {
        SessionID::id_type tab_id;
        std::string user_agent_override;
        if (!RestoreSetTabUserAgentOverrideCommand(
                *command, &tab_id, &user_agent_override)) {
          return true;
        }

        GetTab(tab_id, tabs)->user_agent_override.swap(user_agent_override);
        break;
      }

      case kCommandSessionStorageAssociated: {
        scoped_ptr<Pickle> command_pickle(command->PayloadAsPickle());
        SessionID::id_type command_tab_id;
        std::string session_storage_persistent_id;
        PickleIterator iter(*command_pickle.get());
        if (!command_pickle->ReadInt(&iter, &command_tab_id) ||
            !command_pickle->ReadString(&iter, &session_storage_persistent_id))
          return true;
        // Associate the session storage back.
        GetTab(command_tab_id, tabs)->session_storage_persistent_id =
            session_storage_persistent_id;
        break;
      }

      case kCommandSetActiveWindow: {
        ActiveWindowPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          VLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        *active_window_id = payload;
        break;
      }

      default:
        VLOG(1) << "Failed reading an unknown command " << command->id();
        return true;
    }
  }
  return true;
}

void SessionService::BuildCommandsForTab(const SessionID& window_id,
                                         WebContents* tab,
                                         int index_in_window,
                                         bool is_pinned,
                                         std::vector<SessionCommand*>* commands,
                                         IdToRange* tab_to_available_range) {
  DCHECK(tab && commands && window_id.id());
  SessionTabHelper* session_tab_helper = SessionTabHelper::FromWebContents(tab);
  const SessionID& session_id(session_tab_helper->session_id());
  commands->push_back(CreateSetTabWindowCommand(window_id, session_id));

  const int current_index = tab->GetController().GetCurrentEntryIndex();
  const int min_index = std::max(0,
                                 current_index - max_persist_navigation_count);
  const int max_index =
      std::min(current_index + max_persist_navigation_count,
               tab->GetController().GetEntryCount());
  const int pending_index = tab->GetController().GetPendingEntryIndex();
  if (tab_to_available_range) {
    (*tab_to_available_range)[session_id.id()] =
        std::pair<int, int>(min_index, max_index);
  }

  if (is_pinned) {
    commands->push_back(CreatePinnedStateCommand(session_id, true));
  }

  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(tab);
  if (extensions_tab_helper->extension_app()) {
    commands->push_back(
        CreateSetTabExtensionAppIDCommand(
            kCommandSetExtensionAppID, session_id.id(),
            extensions_tab_helper->extension_app()->id()));
  }

  const std::string& ua_override = tab->GetUserAgentOverride();
  if (!ua_override.empty()) {
    commands->push_back(
        CreateSetTabUserAgentOverrideCommand(
            kCommandSetTabUserAgentOverride, session_id.id(), ua_override));
  }

  for (int i = min_index; i < max_index; ++i) {
    const NavigationEntry* entry = (i == pending_index) ?
        tab->GetController().GetPendingEntry() :
        tab->GetController().GetEntryAtIndex(i);
    DCHECK(entry);
    if (ShouldTrackEntry(entry->GetVirtualURL())) {
      const SerializedNavigationEntry navigation =
          SerializedNavigationEntry::FromNavigationEntry(i, *entry);
      commands->push_back(
          CreateUpdateTabNavigationCommand(
              kCommandUpdateTabNavigation, session_id.id(), navigation));
    }
  }
  commands->push_back(
      CreateSetSelectedNavigationIndexCommand(session_id, current_index));

  if (index_in_window != -1) {
    commands->push_back(
        CreateSetTabIndexInWindowCommand(session_id, index_in_window));
  }

  // Record the association between the sessionStorage namespace and the tab.
  content::SessionStorageNamespace* session_storage_namespace =
      tab->GetController().GetDefaultSessionStorageNamespace();
  ScheduleCommand(CreateSessionStorageAssociatedCommand(
      session_tab_helper->session_id(),
      session_storage_namespace->persistent_id()));
}

void SessionService::BuildCommandsForBrowser(
    Browser* browser,
    std::vector<SessionCommand*>* commands,
    IdToRange* tab_to_available_range,
    std::set<SessionID::id_type>* windows_to_track) {
  DCHECK(browser && commands);
  DCHECK(browser->session_id().id());

  commands->push_back(
      CreateSetWindowBoundsCommand(browser->session_id(),
                                   browser->window()->GetRestoredBounds(),
                                   browser->window()->GetRestoredState()));

  commands->push_back(CreateSetWindowTypeCommand(
      browser->session_id(), WindowTypeForBrowserType(browser->type())));

  if (!browser->app_name().empty()) {
    commands->push_back(CreateSetWindowAppNameCommand(
        kCommandSetWindowAppName,
        browser->session_id().id(),
        browser->app_name()));
  }

  windows_to_track->insert(browser->session_id().id());
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    WebContents* tab = tab_strip->GetWebContentsAt(i);
    DCHECK(tab);
    BuildCommandsForTab(browser->session_id(), tab, i,
                        tab_strip->IsTabPinned(i),
                        commands, tab_to_available_range);
  }

  commands->push_back(
      CreateSetSelectedTabInWindow(browser->session_id(),
                                   browser->tab_strip_model()->active_index()));
}

void SessionService::BuildCommandsFromBrowsers(
    std::vector<SessionCommand*>* commands,
    IdToRange* tab_to_available_range,
    std::set<SessionID::id_type>* windows_to_track) {
  DCHECK(commands);
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    // Make sure the browser has tabs and a window. Browser's destructor
    // removes itself from the BrowserList. When a browser is closed the
    // destructor is not necessarily run immediately. This means it's possible
    // for us to get a handle to a browser that is about to be removed. If
    // the tab count is 0 or the window is NULL, the browser is about to be
    // deleted, so we ignore it.
    if (ShouldTrackBrowser(browser) && browser->tab_strip_model()->count() &&
        browser->window()) {
      BuildCommandsForBrowser(browser, commands, tab_to_available_range,
                              windows_to_track);
    }
  }
}

void SessionService::ScheduleReset() {
  set_pending_reset(true);
  STLDeleteElements(&pending_commands());
  tab_to_available_range_.clear();
  windows_tracking_.clear();
  BuildCommandsFromBrowsers(&pending_commands(), &tab_to_available_range_,
                            &windows_tracking_);
  if (!windows_tracking_.empty()) {
    // We're lazily created on startup and won't get an initial batch of
    // SetWindowType messages. Set these here to make sure our state is correct.
    has_open_trackable_browsers_ = true;
    move_on_new_browser_ = true;
  }
  StartSaveTimer();
}

bool SessionService::ReplacePendingCommand(SessionCommand* command) {
  // We optimize page navigations, which can happen quite frequently and
  // are expensive. And activation is like Highlander, there can only be one!
  if (command->id() != kCommandUpdateTabNavigation &&
      command->id() != kCommandSetActiveWindow) {
    return false;
  }
  for (std::vector<SessionCommand*>::reverse_iterator i =
       pending_commands().rbegin(); i != pending_commands().rend(); ++i) {
    SessionCommand* existing_command = *i;
    if (command->id() == kCommandUpdateTabNavigation &&
        existing_command->id() == kCommandUpdateTabNavigation) {
      scoped_ptr<Pickle> command_pickle(command->PayloadAsPickle());
      PickleIterator iterator(*command_pickle);
      SessionID::id_type command_tab_id;
      int command_nav_index;
      if (!command_pickle->ReadInt(&iterator, &command_tab_id) ||
          !command_pickle->ReadInt(&iterator, &command_nav_index)) {
        return false;
      }
      SessionID::id_type existing_tab_id;
      int existing_nav_index;
      {
        // Creating a pickle like this means the Pickle references the data from
        // the command. Make sure we delete the pickle before the command, else
        // the pickle references deleted memory.
        scoped_ptr<Pickle> existing_pickle(existing_command->PayloadAsPickle());
        iterator = PickleIterator(*existing_pickle);
        if (!existing_pickle->ReadInt(&iterator, &existing_tab_id) ||
            !existing_pickle->ReadInt(&iterator, &existing_nav_index)) {
          return false;
        }
      }
      if (existing_tab_id == command_tab_id &&
          existing_nav_index == command_nav_index) {
        // existing_command is an update for the same tab/index pair. Replace
        // it with the new one. We need to add to the end of the list just in
        // case there is a prune command after the update command.
        delete existing_command;
        pending_commands().erase(i.base() - 1);
        pending_commands().push_back(command);
        return true;
      }
      return false;
    }
    if (command->id() == kCommandSetActiveWindow &&
        existing_command->id() == kCommandSetActiveWindow) {
      *i = command;
      delete existing_command;
      return true;
    }
  }
  return false;
}

void SessionService::ScheduleCommand(SessionCommand* command) {
  DCHECK(command);
  if (ReplacePendingCommand(command))
    return;
  BaseSessionService::ScheduleCommand(command);
  // Don't schedule a reset on tab closed/window closed. Otherwise we may
  // lose tabs/windows we want to restore from if we exit right after this.
  if (!pending_reset() && pending_window_close_ids_.empty() &&
      commands_since_reset() >= kWritesPerReset &&
      (command->id() != kCommandTabClosed &&
       command->id() != kCommandWindowClosed)) {
    ScheduleReset();
  }
}

void SessionService::CommitPendingCloses() {
  for (PendingTabCloseIDs::iterator i = pending_tab_close_ids_.begin();
       i != pending_tab_close_ids_.end(); ++i) {
    ScheduleCommand(CreateTabClosedCommand(*i));
  }
  pending_tab_close_ids_.clear();

  for (PendingWindowCloseIDs::iterator i = pending_window_close_ids_.begin();
       i != pending_window_close_ids_.end(); ++i) {
    ScheduleCommand(CreateWindowClosedCommand(*i));
  }
  pending_window_close_ids_.clear();
}

bool SessionService::IsOnlyOneTabLeft() const {
  if (!profile() || profile()->AsTestingProfile()) {
    // We're testing, always return false.
    return false;
  }

  int window_count = 0;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    const SessionID::id_type window_id = browser->session_id().id();
    if (ShouldTrackBrowser(browser) &&
        window_closing_ids_.find(window_id) == window_closing_ids_.end()) {
      if (++window_count > 1)
        return false;
      // By the time this is invoked the tab has been removed. As such, we use
      // > 0 here rather than > 1.
      if (browser->tab_strip_model()->count() > 0)
        return false;
    }
  }
  return true;
}

bool SessionService::HasOpenTrackableBrowsers(
    const SessionID& window_id) const {
  if (!profile() || profile()->AsTestingProfile()) {
    // We're testing, always return true.
    return true;
  }

  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    const SessionID::id_type browser_id = browser->session_id().id();
    if (browser_id != window_id.id() &&
        window_closing_ids_.find(browser_id) == window_closing_ids_.end() &&
        ShouldTrackBrowser(browser)) {
      return true;
    }
  }
  return false;
}

bool SessionService::ShouldTrackChangesToWindow(
    const SessionID& window_id) const {
  return windows_tracking_.find(window_id.id()) != windows_tracking_.end();
}

bool SessionService::ShouldTrackBrowser(Browser* browser) const {
  if (browser->profile() != profile())
    return false;
  // Never track app popup windows that do not have a trusted source (i.e.
  // popup windows spawned by an app). If this logic changes, be sure to also
  // change SessionRestoreImpl::CreateRestoredBrowser().
  if (browser->is_app() && browser->is_type_popup() &&
      !browser->is_trusted_source()) {
    return false;
  }
  AppType app_type = browser->is_app() ? TYPE_APP : TYPE_NORMAL;
  return should_track_changes_for_browser_type(browser->type(), app_type);
}

bool SessionService::should_track_changes_for_browser_type(Browser::Type type,
                                                           AppType app_type) {
#if defined(OS_CHROMEOS)
  // Restore app popups for chromeos alone.
  if (type == Browser::TYPE_POPUP && app_type == TYPE_APP)
    return true;
#endif

  return type == Browser::TYPE_TABBED;
}

SessionService::WindowType SessionService::WindowTypeForBrowserType(
    Browser::Type type) {
  switch (type) {
    case Browser::TYPE_POPUP:
      return TYPE_POPUP;
    case Browser::TYPE_TABBED:
      return TYPE_TABBED;
    default:
      DCHECK(false);
      return TYPE_TABBED;
  }
}

Browser::Type SessionService::BrowserTypeForWindowType(WindowType type) {
  switch (type) {
    case TYPE_POPUP:
      return Browser::TYPE_POPUP;
    case TYPE_TABBED:
    default:
      return Browser::TYPE_TABBED;
  }
}

void SessionService::RecordSessionUpdateHistogramData(int type,
    base::TimeTicks* last_updated_time) {
  if (!last_updated_time->is_null()) {
    base::TimeDelta delta = base::TimeTicks::Now() - *last_updated_time;
    // We're interested in frequent updates periods longer than
    // 10 minutes.
    bool use_long_period = false;
    if (delta >= save_delay_in_mins_) {
      use_long_period = true;
    }
    switch (type) {
      case chrome::NOTIFICATION_SESSION_SERVICE_SAVED :
        RecordUpdatedSaveTime(delta, use_long_period);
        RecordUpdatedSessionNavigationOrTab(delta, use_long_period);
        break;
      case content::NOTIFICATION_WEB_CONTENTS_DESTROYED:
        RecordUpdatedTabClosed(delta, use_long_period);
        RecordUpdatedSessionNavigationOrTab(delta, use_long_period);
        break;
      case content::NOTIFICATION_NAV_LIST_PRUNED:
        RecordUpdatedNavListPruned(delta, use_long_period);
        RecordUpdatedSessionNavigationOrTab(delta, use_long_period);
        break;
      case content::NOTIFICATION_NAV_ENTRY_COMMITTED:
        RecordUpdatedNavEntryCommit(delta, use_long_period);
        RecordUpdatedSessionNavigationOrTab(delta, use_long_period);
        break;
      default:
        NOTREACHED() << "Bad type sent to RecordSessionUpdateHistogramData";
        break;
    }
  }
  (*last_updated_time) = base::TimeTicks::Now();
}

void SessionService::RecordUpdatedTabClosed(base::TimeDelta delta,
                                            bool use_long_period) {
  std::string name("SessionRestore.TabClosedPeriod");
  UMA_HISTOGRAM_CUSTOM_TIMES(name,
      delta,
      // 2500ms is the default save delay.
      save_delay_in_millis_,
      save_delay_in_mins_,
      50);
  if (use_long_period) {
    std::string long_name_("SessionRestore.TabClosedLongPeriod");
    UMA_HISTOGRAM_CUSTOM_TIMES(long_name_,
        delta,
        save_delay_in_mins_,
        save_delay_in_hrs_,
        50);
  }
}

void SessionService::RecordUpdatedNavListPruned(base::TimeDelta delta,
                                                bool use_long_period) {
  std::string name("SessionRestore.NavigationListPrunedPeriod");
  UMA_HISTOGRAM_CUSTOM_TIMES(name,
      delta,
      // 2500ms is the default save delay.
      save_delay_in_millis_,
      save_delay_in_mins_,
      50);
  if (use_long_period) {
    std::string long_name_("SessionRestore.NavigationListPrunedLongPeriod");
    UMA_HISTOGRAM_CUSTOM_TIMES(long_name_,
        delta,
        save_delay_in_mins_,
        save_delay_in_hrs_,
        50);
  }
}

void SessionService::RecordUpdatedNavEntryCommit(base::TimeDelta delta,
                                                 bool use_long_period) {
  std::string name("SessionRestore.NavEntryCommittedPeriod");
  UMA_HISTOGRAM_CUSTOM_TIMES(name,
      delta,
      // 2500ms is the default save delay.
      save_delay_in_millis_,
      save_delay_in_mins_,
      50);
  if (use_long_period) {
    std::string long_name_("SessionRestore.NavEntryCommittedLongPeriod");
    UMA_HISTOGRAM_CUSTOM_TIMES(long_name_,
        delta,
        save_delay_in_mins_,
        save_delay_in_hrs_,
        50);
  }
}

void SessionService::RecordUpdatedSessionNavigationOrTab(base::TimeDelta delta,
                                                         bool use_long_period) {
  std::string name("SessionRestore.NavOrTabUpdatePeriod");
  UMA_HISTOGRAM_CUSTOM_TIMES(name,
      delta,
      // 2500ms is the default save delay.
      save_delay_in_millis_,
      save_delay_in_mins_,
      50);
  if (use_long_period) {
    std::string long_name_("SessionRestore.NavOrTabUpdateLongPeriod");
    UMA_HISTOGRAM_CUSTOM_TIMES(long_name_,
        delta,
        save_delay_in_mins_,
        save_delay_in_hrs_,
        50);
  }
}

void SessionService::RecordUpdatedSaveTime(base::TimeDelta delta,
                                           bool use_long_period) {
  std::string name("SessionRestore.SavePeriod");
  UMA_HISTOGRAM_CUSTOM_TIMES(name,
      delta,
      // 2500ms is the default save delay.
      save_delay_in_millis_,
      save_delay_in_mins_,
      50);
  if (use_long_period) {
    std::string long_name_("SessionRestore.SaveLongPeriod");
    UMA_HISTOGRAM_CUSTOM_TIMES(long_name_,
        delta,
        save_delay_in_mins_,
        save_delay_in_hrs_,
        50);
  }
}

void SessionService::TabInserted(WebContents* contents) {
  SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(contents);
  if (!ShouldTrackChangesToWindow(session_tab_helper->window_id()))
    return;
  SetTabWindow(session_tab_helper->window_id(),
               session_tab_helper->session_id());
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(contents);
  if (extensions_tab_helper &&
      extensions_tab_helper->extension_app()) {
    SetTabExtensionAppID(
        session_tab_helper->window_id(),
        session_tab_helper->session_id(),
        extensions_tab_helper->extension_app()->id());
  }

  // Record the association between the SessionStorageNamespace and the
  // tab.
  //
  // TODO(ajwong): This should be processing the whole map rather than
  // just the default. This in particular will not work for tabs with only
  // isolated apps which won't have a default partition.
  content::SessionStorageNamespace* session_storage_namespace =
      contents->GetController().GetDefaultSessionStorageNamespace();
  ScheduleCommand(CreateSessionStorageAssociatedCommand(
      session_tab_helper->session_id(),
      session_storage_namespace->persistent_id()));
  session_storage_namespace->SetShouldPersist(true);
}

void SessionService::TabClosing(WebContents* contents) {
  // Allow the associated sessionStorage to get deleted; it won't be needed
  // in the session restore.
  content::SessionStorageNamespace* session_storage_namespace =
      contents->GetController().GetDefaultSessionStorageNamespace();
  session_storage_namespace->SetShouldPersist(false);
  SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(contents);
  TabClosed(session_tab_helper->window_id(),
            session_tab_helper->session_id(),
            contents->GetClosedByUserGesture());
  RecordSessionUpdateHistogramData(content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                                   &last_updated_tab_closed_time_);
}

void SessionService::MaybeDeleteSessionOnlyData() {
  // Don't try anything if we're testing.  The browser_process is not fully
  // created and DeleteSession will crash if we actually attempt it.
  if (!profile() || profile()->AsTestingProfile())
    return;

  // Clear session data if the last window for a profile has been closed and
  // closing the last window would normally close Chrome, unless background mode
  // is active.  Tests don't have a background_mode_manager.
  if (has_open_trackable_browsers_ ||
      browser_defaults::kBrowserAliveWithNoWindows ||
      g_browser_process->background_mode_manager()->IsBackgroundModeActive()) {
    return;
  }

  // Check for any open windows for the current profile that we aren't tracking.
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if ((*it)->profile() == profile())
      return;
  }
  DeleteSessionOnlyData(profile());
}
