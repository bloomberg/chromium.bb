// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_H_

#include <vector>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/host_desktop.h"

class Browser;
class Profile;

namespace chrome {
class BrowserListObserver;
}

// Maintains a list of Browser objects present in a given HostDesktop (see
// HostDesktopType).
class BrowserList {
 public:
  typedef std::vector<Browser*> BrowserVector;
  typedef BrowserVector::const_iterator const_iterator;
  typedef BrowserVector::const_reverse_iterator const_reverse_iterator;

  // Returns the last active browser for this list.
  Browser* GetLastActive() const;

  // Browsers are added to the list before they have constructed windows,
  // so the |window()| member function may return NULL.
  const_iterator begin() const { return browsers_.begin(); }
  const_iterator end() const { return browsers_.end(); }

  bool empty() const { return browsers_.empty(); }
  size_t size() const { return browsers_.size(); }

  Browser* get(size_t index) const { return browsers_[index]; }

  // Returns iterated access to list of open browsers ordered by when
  // they were last active. The underlying data structure is a vector
  // and we push_back on recent access so a reverse iterator gives the
  // latest accessed browser first.
  const_reverse_iterator begin_last_active() const {
    return last_active_browsers_.rbegin();
  }
  const_reverse_iterator end_last_active() const {
    return last_active_browsers_.rend();
  }

  static BrowserList* GetInstance(chrome::HostDesktopType type);

  // Adds or removes |browser| from the list it is associated with. The browser
  // object should be valid BEFORE these calls (for the benefit of observers),
  // so notify and THEN delete the object.
  static void AddBrowser(Browser* browser);
  static void RemoveBrowser(Browser* browser);

  // Adds and removes |observer| from the observer list for all desktops.
  // Observers are responsible for making sure the notifying browser is relevant
  // to them (e.g., on the specific desktop they care about if any).
  static void AddObserver(chrome::BrowserListObserver* observer);
  static void RemoveObserver(chrome::BrowserListObserver* observer);

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was on each desktop.
  // Note: This only takes effect on the appropriate browser list as determined
  // by |browser->host_desktop_type()|.
  static void SetLastActive(Browser* browser);

  // Closes all browsers for |profile| across all desktops.
  static void CloseAllBrowsersWithProfile(Profile* profile);

  // Returns true if at least one incognito session is active across all
  // desktops.
  static bool IsOffTheRecordSessionActive();

  // Returns true if at least one incognito session is active for |profile|
  // across all desktops.
  static bool IsOffTheRecordSessionActiveForProfile(Profile* profile);

 private:
  BrowserList();
  ~BrowserList();

  // Helper method to remove a browser instance from a list of browsers
  static void RemoveBrowserFrom(Browser* browser, BrowserVector* browser_list);

  // A vector of the browsers in this list, in the order they were added.
  BrowserVector browsers_;
  // A vector of the browsers in this list that have been activated, in the
  // reverse order in which they were last activated.
  BrowserVector last_active_browsers_;

  // A list of observers which will be notified of every browser addition and
  // removal across all BrowserLists.
  static base::LazyInstance<ObserverList<chrome::BrowserListObserver> >::Leaky
      observers_;

  // Nothing fancy, since we only have two HDTs.
  static BrowserList* native_instance_;
  static BrowserList* ash_instance_;

  DISALLOW_COPY_AND_ASSIGN(BrowserList);
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
