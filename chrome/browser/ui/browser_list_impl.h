// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_IMPL_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_IMPL_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"

class Browser;
class Profile;

namespace chrome {

class BrowserListObserver;

// Maintains a list of Browser objects present in a given HostDesktop (see
// HostDesktopType).
class BrowserListImpl {
 public:
  typedef std::vector<Browser*> BrowserVector;
  typedef BrowserVector::iterator iterator;
  typedef BrowserVector::const_iterator const_iterator;
  typedef BrowserVector::const_reverse_iterator const_reverse_iterator;

  static BrowserListImpl* GetInstance(HostDesktopType type);

  // Adds and removes browsers from the global list. The browser object should
  // be valid BEFORE these calls (for the benefit of observers), so notify and
  // THEN delete the object.
  void AddBrowser(Browser* browser);
  void RemoveBrowser(Browser* browser);

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was.
  void SetLastActive(Browser* browser);

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

 private:
  BrowserListImpl();
  ~BrowserListImpl();

  // Only callable by BrowserList::(Add|Remove)Observer.
  // TODO(gab): Merge BrowserListImpl into BrowserList removing the need for
  // friend.
  friend void BrowserList::AddObserver(chrome::BrowserListObserver*);
  friend void BrowserList::RemoveObserver(chrome::BrowserListObserver*);
  void AddObserver(BrowserListObserver* observer);
  void RemoveObserver(BrowserListObserver* observer);

  // Helper method to remove a browser instance from a list of browsers
  void RemoveBrowserFrom(Browser* browser, BrowserVector* browser_list);

  ObserverList<BrowserListObserver> observers_;

  BrowserVector browsers_;
  BrowserVector last_active_browsers_;

  // Nothing fancy, since we only have two HDTs.
  static BrowserListImpl* native_instance_;
  static BrowserListImpl* ash_instance_;

  DISALLOW_COPY_AND_ASSIGN(BrowserListImpl);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_IMPL_H_
