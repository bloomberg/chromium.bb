// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_H_
#pragma once

#include <set>
#include <vector>

#include "base/observer_list.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;

// Stores a list of all Browser objects.
class BrowserList {
 public:
  typedef std::vector<Browser*> BrowserVector;
  typedef BrowserVector::iterator iterator;
  typedef BrowserVector::const_iterator const_iterator;
  typedef BrowserVector::const_reverse_iterator const_reverse_iterator;

  // It is not allowed to change the global window list (add or remove any
  // browser windows while handling observer callbacks.
  class Observer {
   public:
    // Called immediately after a browser is added to the list
    virtual void OnBrowserAdded(const Browser* browser) = 0;

    // Called immediately after a browser is removed from the list
    virtual void OnBrowserRemoved(const Browser* browser) = 0;

    // Called immediately after a browser is set active (SetLastActive)
    virtual void OnBrowserSetLastActive(const Browser* browser) {}

   protected:
    virtual ~Observer() {}
  };

  // Adds and removes browsers from the global list. The browser object should
  // be valid BEFORE these calls (for the benefit of observers), so notify and
  // THEN delete the object.
  static void AddBrowser(Browser* browser);
  static void RemoveBrowser(Browser* browser);

  static void AddObserver(Observer* observer);
  static void RemoveObserver(Observer* observer);

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was.
  static void SetLastActive(Browser* browser);

  // Returns the Browser object whose window was most recently active.  If the
  // most recently open Browser's window was closed, returns the first Browser
  // in the list.  If no Browsers exist, returns NULL.
  //
  // WARNING: this is NULL until a browser becomes active. If during startup
  // a browser does not become active (perhaps the user launches Chrome, then
  // clicks on another app before the first browser window appears) then this
  // returns NULL.
  // WARNING #2: this will always be NULL in unit tests run on the bots.
  static Browser* GetLastActive();

  // Closes all browsers for |profile|.
  static void CloseAllBrowsersWithProfile(Profile* profile);

  // Browsers are added to the list before they have constructed windows,
  // so the |window()| member function may return NULL.
  static const_iterator begin();
  static const_iterator end();

  static bool empty();
  static size_t size();

  // Returns iterated access to list of open browsers ordered by when
  // they were last active. The underlying data structure is a vector
  // and we push_back on recent access so a reverse iterator gives the
  // latest accessed browser first.
  static const_reverse_iterator begin_last_active();
  static const_reverse_iterator end_last_active();

  // Returns true if at least one incognito session is active.
  static bool IsOffTheRecordSessionActive();

  // Returns true if at least one incognito session is active for |profile|.
  static bool IsOffTheRecordSessionActiveForProfile(Profile* profile);

 private:
  // Helper method to remove a browser instance from a list of browsers
  static void RemoveBrowserFrom(Browser* browser, BrowserVector* browser_list);
};

class TabContentsWrapper;

// Iterates through all web view hosts in all browser windows. Because the
// renderers act asynchronously, getting a host through this interface does
// not guarantee that the renderer is ready to go. Doing anything to affect
// browser windows or tabs while iterating may cause incorrect behavior.
//
// Example:
//   for (TabContentsIterator iterator; !iterator.done(); ++iterator) {
//     TabContentsWrapper* cur = *iterator;
//     -or-
//     iterator->operationOnTabContents();
//     ...
//   }
class TabContentsIterator {
 public:
  TabContentsIterator();

  // Returns true if we are past the last Browser.
  bool done() const {
    return cur_ == NULL;
  }

  // Returns the Browser instance associated with the current
  // TabContentsWrapper. Valid as long as !done()
  Browser* browser() const {
    if (browser_iterator_ != BrowserList::end())
      return *browser_iterator_;
    return NULL;
  }

  // Returns the current TabContentsWrapper, valid as long as !Done()
  TabContentsWrapper* operator->() const {
    return cur_;
  }
  TabContentsWrapper* operator*() const {
    return cur_;
  }

  // Incrementing operators, valid as long as !Done()
  TabContentsWrapper* operator++() {  // ++preincrement
    Advance();
    return cur_;
  }
  TabContentsWrapper* operator++(int) {  // postincrement++
    TabContentsWrapper* tmp = cur_;
    Advance();
    return tmp;
  }

 private:
  // Loads the next host into Cur. This is designed so that for the initial
  // call when browser_iterator_ points to the first browser and
  // web_view_index_ is -1, it will fill the first host.
  void Advance();

  // iterator over all the Browser objects
  BrowserList::const_iterator browser_iterator_;

  // tab index into the current Browser of the current web view
  int web_view_index_;

  // iterator over the TabContentsWrappers doing background printing.
  std::set<TabContentsWrapper*>::const_iterator bg_printing_iterator_;

  // Current TabContentsWrapper, or NULL if we're at the end of the list. This
  // can be extracted given the browser iterator and index, but it's nice to
  // cache this since the caller may access the current host many times.
  TabContentsWrapper* cur_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
