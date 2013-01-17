// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_

#include <list>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/session_storage_namespace.h"
#include "webkit/glue/window_open_disposition.h"

class TabRestoreServiceDelegate;
class TabRestoreServiceObserver;

namespace content {
class SessionStorageNamespace;
class WebContents;
}

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
class TabRestoreService : public ProfileKeyedService {
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
    scoped_refptr<content::SessionStorageNamespace> session_storage_namespace;

    // The user agent override used for the tab's navigations (if applicable).
    std::string user_agent_override;
  };

  // Represents a previously open window.
  struct Window : public Entry {
    Window();
    virtual ~Window();

    // The tabs that comprised the window, in order.
    std::vector<Tab> tabs;

    // Index of the selected tab.
    int selected_tab_index;

    // If an application window, the name of the app.
    std::string app_name;
  };

  typedef std::list<Entry*> Entries;

  virtual ~TabRestoreService();

  // Adds/removes an observer. TabRestoreService does not take ownership of
  // the observer.
  virtual void AddObserver(TabRestoreServiceObserver* observer) = 0;
  virtual void RemoveObserver(TabRestoreServiceObserver* observer) = 0;

  // Creates a Tab to represent |contents| and notifies observers the list of
  // entries has changed.
  virtual void CreateHistoricalTab(content::WebContents* contents,
                                   int index) = 0;

  // Invoked when a browser is closing. If |delegate| is a tabbed browser with
  // at least one tab, a Window is created, added to entries and observers are
  // notified.
  virtual void BrowserClosing(TabRestoreServiceDelegate* delegate) = 0;

  // Invoked when the browser is done closing.
  virtual void BrowserClosed(TabRestoreServiceDelegate* delegate) = 0;

  // Removes all entries from the list and notifies observers the list
  // of tabs has changed.
  virtual void ClearEntries() = 0;

  // Returns the entries, ordered with most recently closed entries at the
  // front.
  virtual const Entries& entries() const = 0;

  // Restores the most recently closed entry. Does nothing if there are no
  // entries to restore. If the most recently restored entry is a tab, it is
  // added to |delegate|. If a new browser needs to be created for this entry,
  // it will be created on the desktop specified by |host_desktop_type|.
  virtual void RestoreMostRecentEntry(
      TabRestoreServiceDelegate* delegate,
      chrome::HostDesktopType host_desktop_type) = 0;

  // Removes the Tab with id |id| from the list and returns it; ownership is
  // passed to the caller.
  virtual Tab* RemoveTabEntryById(SessionID::id_type id) = 0;

  // Restores an entry by id. If there is no entry with an id matching |id|,
  // this does nothing. If |delegate| is NULL, this creates a new window for the
  // entry. |disposition| is respected, but the attributes (tabstrip index,
  // browser window) of the tab when it was closed will be respected if
  // disposition is UNKNOWN. If a new browser needs to be created for this
  // entry, it will be created on the desktop specified by |host_desktop_type|.
  virtual void RestoreEntryById(TabRestoreServiceDelegate* delegate,
                                SessionID::id_type id,
                                chrome::HostDesktopType host_desktop_type,
                                WindowOpenDisposition disposition) = 0;

  // Loads the tabs and previous session. This does nothing if the tabs
  // from the previous session have already been loaded.
  virtual void LoadTabsFromLastSession() = 0;

  // Returns true if the tab entries have been loaded.
  virtual bool IsLoaded() const = 0;

  // Deletes the last session.
  virtual void DeleteLastSession() = 0;
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_H_
