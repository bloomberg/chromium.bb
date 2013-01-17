// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_HELPER_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_HELPER_H_

#include <set>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/ui/host_desktop.h"

class Profile;
class TabRestoreService;
class TabRestoreServiceDelegate;
class TabRestoreServiceObserver;
class TimeFactory;

namespace content {
class NavigationController;
class WebContents;
}

// Helper class used to implement InMemoryTabRestoreService and
// PersistentTabRestoreService. See tab_restore_service.h for method-level
// comments.
class TabRestoreServiceHelper {
 public:
  typedef TabRestoreService::Entries Entries;
  typedef TabRestoreService::Entry Entry;
  typedef TabRestoreService::Tab Tab;
  typedef TabRestoreService::TimeFactory TimeFactory;
  typedef TabRestoreService::Window Window;

  // Provides a way for the client to add behavior to the tab restore service
  // helper (e.g. implementing tabs persistence).
  class Observer {
   public:
    // Invoked before the entries are cleared.
    virtual void OnClearEntries();

    // Invoked before the entry is restored. |entry_iterator| points to the
    // entry corresponding to the session identified by |id|.
    virtual void OnRestoreEntryById(SessionID::id_type id,
                                    Entries::const_iterator entry_iterator);

    // Invoked after an entry was added.
    virtual void OnAddEntry();

   protected:
    virtual ~Observer();
  };

  enum {
    // Max number of entries we'll keep around.
    kMaxEntries = 25,
  };

  // Creates a new TabRestoreServiceHelper and provides an object that provides
  // the current time. The TabRestoreServiceHelper does not take ownership of
  // |time_factory| and |observer|. Note that |observer| can also be NULL.
  TabRestoreServiceHelper(TabRestoreService* tab_restore_service,
                          Observer* observer,
                          Profile* profile,
                          TimeFactory* time_factory);

  ~TabRestoreServiceHelper();

  // Helper methods used to implement TabRestoreService.
  void AddObserver(TabRestoreServiceObserver* observer);
  void RemoveObserver(TabRestoreServiceObserver* observer);
  void CreateHistoricalTab(content::WebContents* contents, int index);
  void BrowserClosing(TabRestoreServiceDelegate* delegate);
  void BrowserClosed(TabRestoreServiceDelegate* delegate);
  void ClearEntries();
  const Entries& entries() const;
  void RestoreMostRecentEntry(TabRestoreServiceDelegate* delegate,
                              chrome::HostDesktopType host_desktop_type);
  Tab* RemoveTabEntryById(SessionID::id_type id);
  void RestoreEntryById(TabRestoreServiceDelegate* delegate,
                        SessionID::id_type id,
                        chrome::HostDesktopType host_desktop_type,
                        WindowOpenDisposition disposition);

  // Notifies observers the tabs have changed.
  void NotifyTabsChanged();

  // Adds |entry| to the list of entries and takes ownership. If |prune| is true
  // |PruneAndNotify| is invoked. If |to_front| is true the entry is added to
  // the front, otherwise the back. Normal closes go to the front, but
  // tab/window closes from the previous session are added to the back.
  void AddEntry(Entry* entry, bool prune, bool to_front);

  // Prunes |entries_| to contain only kMaxEntries, and removes uninteresting
  // entries.
  void PruneEntries();

  // Returns an iterator into |entries_| whose id matches |id|. If |id|
  // identifies a Window, then its iterator position will be returned. If it
  // identifies a tab, then the iterator position of the Window in which the Tab
  // resides is returned.
  Entries::iterator GetEntryIteratorById(SessionID::id_type id);

  // Calls either ValidateTab or ValidateWindow as appropriate.
  static bool ValidateEntry(Entry* entry);

 private:
  friend class PersistentTabRestoreService;

  // Populates the tab's navigations from the NavigationController, and its
  // browser_id and pinned state from the browser.
  void PopulateTab(Tab* tab,
                   int index,
                   TabRestoreServiceDelegate* delegate,
                   content::NavigationController* controller);

  // This is a helper function for RestoreEntryById() for restoring a single
  // tab. If |delegate| is NULL, this creates a new window for the entry. This
  // returns the TabRestoreServiceDelegate into which the tab was restored.
  // |disposition| will be respected, but if it is UNKNOWN then the tab's
  // original attributes will be respected instead. If a new browser needs to be
  // created for this tab, it will be created on the desktop specified by
  // |host_desktop_type|.
  TabRestoreServiceDelegate* RestoreTab(
      const Tab& tab,
      TabRestoreServiceDelegate* delegate,
      chrome::HostDesktopType host_desktop_type,
      WindowOpenDisposition disposition);

  // Returns true if |tab| has more than one navigation. If |tab| has more
  // than one navigation |tab->current_navigation_index| is constrained based
  // on the number of navigations.
  static bool ValidateTab(Tab* tab);

  // Validates all the tabs in a window, plus the window's active tab index.
  static bool ValidateWindow(Window* window);

  // Returns true if |tab| is one we care about restoring.
  static bool IsTabInteresting(const Tab* tab);

  // Checks whether |window| is interesting --- if it only contains a single,
  // uninteresting tab, it's not interesting.
  static bool IsWindowInteresting(const Window* window);

  // Validates and checks |entry| for interesting.
  static bool FilterEntry(Entry* entry);

  // Finds tab entries with the old browser_id and sets it to the new one.
  void UpdateTabBrowserIDs(SessionID::id_type old_id,
                           SessionID::id_type new_id);

  // Gets the current time. This uses the time_factory_ if there is one.
  base::Time TimeNow() const;

  TabRestoreService* const tab_restore_service_;

  Observer* const observer_;

  Profile* const profile_;

  // Set of entries. They are ordered from most to least recent.
  Entries entries_;

  // Are we restoring a tab? If this is true we ignore requests to create a
  // historical tab.
  bool restoring_;

  ObserverList<TabRestoreServiceObserver> observer_list_;

  // Set of delegates that we've received a BrowserClosing method for but no
  // corresponding BrowserClosed. We cache the set of delegates closing to
  // avoid creating historical tabs for them.
  std::set<TabRestoreServiceDelegate*> closing_delegates_;

  TimeFactory* const time_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreServiceHelper);
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_HELPER_H_
