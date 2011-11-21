// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_
#pragma once

#include <list>
#include <set>
#include <vector>

#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/sessions/base_session_service.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_types.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"

class NavigationController;
class Profile;
class TabRestoreServiceDelegate;
class TabRestoreServiceObserver;
struct SessionWindow;

// TabRestoreService is responsible for maintaining the most recently closed
// tabs and windows. When a tab is closed
// TabRestoreService::CreateHistoricalTab is invoked and a Tab is created to
// represent the tab. Similarly, when a browser is closed, BrowserClosing is
// invoked and a Window is created to represent the window.
//
// To restore a tab/window from the TabRestoreService invoke RestoreEntryById
// or RestoreMostRecentEntry.
//
// To listen for changes to the set of entries managed by the TabRestoreService
// add an observer.
class TabRestoreService : public BaseSessionService {
 public:
  // Interface used to allow the test to provide a custom time.
  class TimeFactory {
   public:
    virtual ~TimeFactory();
    virtual base::Time TimeNow() = 0;
  };

  // The type of entry.
  enum Type {
    TAB,
    WINDOW
  };

  struct Entry {
    Entry();
    explicit Entry(Type type);
    virtual ~Entry();

    // Unique id for this entry. The id is guaranteed to be unique for a
    // session.
    SessionID::id_type id;

    // The type of the entry.
    Type type;

    // The time when the window or tab was closed.
    base::Time timestamp;

    // Is this entry from the last session? This is set to true for entries that
    // were closed during the last session, and false for entries that were
    // closed during this session.
    bool from_last_session;
  };

  // Represents a previously open tab.
  struct Tab : public Entry {
    Tab();
    virtual ~Tab();

    bool has_browser() const { return browser_id > 0; }

    // The navigations.
    std::vector<TabNavigation> navigations;

    // Index of the selected navigation in navigations.
    int current_navigation_index;

    // The ID of the browser to which this tab belonged, so it can be restored
    // there. May be 0 (an invalid SessionID) when restoring an entire session.
    SessionID::id_type browser_id;

    // Index within the tab strip. May be -1 for an unknown index.
    int tabstrip_index;

    // True if the tab was pinned.
    bool pinned;

    // If non-empty gives the id of the extension for the tab.
    std::string extension_app_id;

    // The associated session storage namespace (if any).
    scoped_refptr<SessionStorageNamespace> session_storage_namespace;
  };

  // Represents a previously open window.
  struct Window : public Entry {
    Window();
    virtual ~Window();

    // The tabs that comprised the window, in order.
    std::vector<Tab> tabs;

    // Index of the selected tab.
    int selected_tab_index;
  };

  typedef std::list<Entry*> Entries;

  // Creates a new TabRestoreService and provides an object that provides the
  // current time. The TabRestoreService does not take ownership of the
  // |time_factory_|.
  TabRestoreService(Profile* profile, TimeFactory* time_factory_ = NULL);

  virtual ~TabRestoreService();

  // Adds/removes an observer. TabRestoreService does not take ownership of
  // the observer.
  void AddObserver(TabRestoreServiceObserver* observer);
  void RemoveObserver(TabRestoreServiceObserver* observer);

  // Creates a Tab to represent |tab| and notifies observers the list of
  // entries has changed.
  void CreateHistoricalTab(NavigationController* tab, int index);

  // Invoked when a browser is closing. If |delegate| is a tabbed browser with
  // at least one tab, a Window is created, added to entries and observers are
  // notified.
  void BrowserClosing(TabRestoreServiceDelegate* delegate);

  // Invoked when the browser is done closing.
  void BrowserClosed(TabRestoreServiceDelegate* delegate);

  // Removes all entries from the list and notifies observers the list
  // of tabs has changed.
  void ClearEntries();

  // Returns the entries, ordered with most recently closed entries at the
  // front.
  virtual const Entries& entries() const;

  // Restores the most recently closed entry. Does nothing if there are no
  // entries to restore. If the most recently restored entry is a tab, it is
  // added to |delegate|.
  void RestoreMostRecentEntry(TabRestoreServiceDelegate* delegate);

  // Restores an entry by id. If there is no entry with an id matching |id|,
  // this does nothing. If |replace_existing_tab| is true and id identifies a
  // tab, the newly created tab replaces the selected tab in |delegate|. If
  // |delegate| is NULL, this creates a new window for the entry.
  void RestoreEntryById(TabRestoreServiceDelegate* delegate,
                        SessionID::id_type id,
                        bool replace_existing_tab);

  // Loads the tabs and previous session. This does nothing if the tabs
  // from the previous session have already been loaded.
  void LoadTabsFromLastSession();

  // Max number of entries we'll keep around.
  static const size_t kMaxEntries;

  // Creates and add entries to |entries| for each of the windows in |windows|.
  void CreateEntriesFromWindows(std::vector<SessionWindow*>* windows,
                                std::vector<Entry*>* entries);

 protected:
  virtual void Save() OVERRIDE;

 private:
  // Used to indicate what has loaded.
  enum LoadState {
    // Indicates we haven't loaded anything.
    NOT_LOADED           = 1 << 0,

    // Indicates we've asked for the last sessions and tabs but haven't gotten
    // the result back yet.
    LOADING              = 1 << 2,

    // Indicates we finished loading the last tabs (but not necessarily the
    // last session).
    LOADED_LAST_TABS     = 1 << 3,

    // Indicates we finished loading the last session (but not necessarily the
    // last tabs).
    LOADED_LAST_SESSION  = 1 << 4
  };

  // Populates the tab's navigations from the NavigationController, and its
  // browser_id and pinned state from the browser.
  void PopulateTab(Tab* tab,
                   int index,
                   TabRestoreServiceDelegate* delegate,
                   NavigationController* controller);

  // Notifies observers the tabs have changed.
  void NotifyTabsChanged();

  // Adds |entry| to the list of entries and takes ownership. If |prune| is true
  // |PruneAndNotify| is invoked. If |to_front| is true the entry is added to
  // the front, otherwise the back. Normal closes go to the front, but
  // tab/window closes from the previous session are added to the back.
  void AddEntry(Entry* entry, bool prune, bool to_front);

  // Prunes entries_ to contain only kMaxEntries and invokes NotifyTabsChanged.
  void PruneAndNotify();

  // Returns an iterator into entries_ whose id matches |id|. If |id| identifies
  // a Window, then its iterator position will be returned. If it identifies a
  // tab, then the iterator position of the Window in which the Tab resides is
  // returned.
  Entries::iterator GetEntryIteratorById(SessionID::id_type id);

  // Schedules the commands for a window close.
  void ScheduleCommandsForWindow(const Window& window);

  // Schedules the commands for a tab close. |selected_index| gives the
  // index of the selected navigation.
  void ScheduleCommandsForTab(const Tab& tab, int selected_index);

  // Creates a window close command.
  SessionCommand* CreateWindowCommand(SessionID::id_type id,
                                      int selected_tab_index,
                                      int num_tabs,
                                      base::Time timestamp);

  // Creates a tab close command.
  SessionCommand* CreateSelectedNavigationInTabCommand(
      SessionID::id_type tab_id,
      int32 index,
      base::Time timestamp);

  // Creates a restore command.
  SessionCommand* CreateRestoredEntryCommand(SessionID::id_type entry_id);

  // Returns the index to persist as the selected index. This is the same
  // as |tab.current_navigation_index| unless the entry at
  // |tab.current_navigation_index| shouldn't be persisted. Returns -1 if
  // no valid navigation to persist.
  int GetSelectedNavigationIndexToPersist(const Tab& tab);

  // Invoked when we've loaded the session commands that identify the
  // previously closed tabs. This creates entries, adds them to
  // staging_entries_, and invokes LoadState.
  void OnGotLastSessionCommands(
      Handle handle,
      scoped_refptr<InternalGetCommandsRequest> request);

  // Populates |loaded_entries| with Entries from |request|.
  void CreateEntriesFromCommands(
      scoped_refptr<InternalGetCommandsRequest> request,
      std::vector<Entry*>* loaded_entries);

  // This is a helper function for RestoreEntryById() for restoring a single
  // tab. If |replace_existing_tab| is true, the newly created tab replaces the
  // selected tab in |delegate|. If |delegate| is NULL, this creates a new
  // window for the entry. This returns the TabRestoreServiceDelegate into which
  // the tab was restored.
  TabRestoreServiceDelegate* RestoreTab(const Tab& tab,
                                        TabRestoreServiceDelegate* delegate,
                                        bool replace_existing_tab);

  // Returns true if |tab| has more than one navigation. If |tab| has more
  // than one navigation |tab->current_navigation_index| is constrained based
  // on the number of navigations.
  bool ValidateTab(Tab* tab);

  // Validates all entries in |entries|, deleting any with no navigations.
  // This also deletes any entries beyond the max number of entries we can
  // hold.
  void ValidateAndDeleteEmptyEntries(std::vector<Entry*>* entries);

  // Finds tab entries with the old browser_id and sets it to the new one.
  void UpdateTabBrowserIDs(SessionID::id_type old_id,
                           SessionID::id_type new_id);

  // Callback from SessionService when we've received the windows from the
  // previous session. This creates and add entries to |staging_entries_|
  // and invokes LoadStateChanged.
  void OnGotPreviousSession(Handle handle,
                            std::vector<SessionWindow*>* windows);

  // Converts a SessionWindow into a Window, returning true on success. We use 0
  // as the timestamp here since we do not know when the window/tab was closed.
  bool ConvertSessionWindowToWindow(
      SessionWindow* session_window,
      Window* window);

  // Invoked when previous tabs or session is loaded. If both have finished
  // loading the entries in staging_entries_ are added to entries_ and
  // observers are notified.
  void LoadStateChanged();

  // Gets the current time. This uses the time_factory_ if there is one.
  base::Time TimeNow() const;

  // Set of entries.
  Entries entries_;

  // Whether we've loaded the last session.
  int load_state_;

  // Are we restoring a tab? If this is true we ignore requests to create a
  // historical tab.
  bool restoring_;

  // Have the max number of entries ever been created?
  bool reached_max_;

  // The number of entries to write.
  int entries_to_write_;

  // Number of entries we've written.
  int entries_written_;

  ObserverList<TabRestoreServiceObserver> observer_list_;

  // Set of delegates that we've received a BrowserClosing method for but no
  // corresponding BrowserClosed. We cache the set of delegates closing to
  // avoid creating historical tabs for them.
  std::set<TabRestoreServiceDelegate*> closing_delegates_;

  // Used when loading open tabs/session when recovering from a crash.
  CancelableRequestConsumer crash_consumer_;

  // Used when loading previous tabs/session.
  CancelableRequestConsumer load_consumer_;

  // Results from previously closed tabs/sessions is first added here. When
  // the results from both us and the session restore service have finished
  // loading LoadStateChanged is invoked, which adds these entries to
  // entries_.
  std::vector<Entry*> staging_entries_;

  TimeFactory* time_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreService);
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_
