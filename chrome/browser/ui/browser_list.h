// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_H_
#pragma once

#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/ui/browser.h"

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

  // Identical in behavior to GetLastActive(), except that the most recently
  // open browser owned by |profile| is returned. If none exist, returns NULL.
  // WARNING: see warnings in GetLastActive().
  static Browser* GetLastActiveWithProfile(Profile *profile);

  // Find an existing browser window with the provided type. Searches in the
  // order of last activation. Only browsers that have been active can be
  // returned. If |match_incognito| is true, will match a browser with either
  // a regular or incognito profile that matches the given one. Returns NULL if
  // no such browser currently exists.
  static Browser* FindBrowserWithType(Profile* p, Browser::Type t,
                                      bool match_incognito);

  // Find an existing browser window that can provide the specified type (this
  // uses Browser::CanSupportsWindowFeature, not
  // Browser::SupportsWindowFeature). This searches in the order of last
  // activation. Only browsers that have been active can be returned. Returns
  // NULL if no such browser currently exists.
  static Browser* FindBrowserWithFeature(Profile* p,
                                         Browser::WindowFeature feature);

  // Find an existing browser window with the provided profile. Searches in the
  // order of last activation. Only browsers that have been active can be
  // returned. Returns NULL if no such browser currently exists.
  static Browser* FindBrowserWithProfile(Profile* p);

  // Find an existing browser with the provided ID. Returns NULL if no such
  // browser currently exists.
  static Browser* FindBrowserWithID(SessionID::id_type desired_id);

  // Checks if the browser can be automatically restarted to install upgrades
  // The browser can be automatically restarted when:
  // 1. It's in the background mode (no visible windows).
  // 2. An update exe is present in the install folder.
  static bool CanRestartForUpdate();

  // Called from Browser::Exit.
  static void Exit();

  // Closes all browsers and exits.  This is equivalent to
  // CloseAllBrowsers(true) on platforms where the application exits when no
  // more windows are remaining.  On other platforms (the Mac), this will
  // additionally exit the application.
  static void CloseAllBrowsersAndExit();

  // Closes all browsers. If the session is ending the windows are closed
  // directly. Otherwise the windows are closed by way of posting a WM_CLOSE
  // message.
  static void CloseAllBrowsers();

  // Begins shutdown of the application when the desktop session is ending.
  static void SessionEnding();

  // Returns true if there is at least one Browser with the specified profile.
  static bool HasBrowserWithProfile(Profile* profile);

  // Tells the BrowserList to keep the application alive after the last Browser
  // closes. This is implemented as a count, so callers should pair their calls
  // to StartKeepAlive() with matching calls to EndKeepAlive() when they no
  // longer need to keep the application running.
  static void StartKeepAlive();

  // Stops keeping the application alive after the last Browser is closed.
  // Should match a previous call to StartKeepAlive().
  static void EndKeepAlive();

  // Returns true if application will continue running after the last Browser
  // closes.
  static bool WillKeepAlive();

  // Browsers are added to |browsers_| before they have constructed windows,
  // so the |window()| member function may return NULL.
  static const_iterator begin() { return browsers_.begin(); }
  static const_iterator end() { return browsers_.end(); }

  static bool empty() { return browsers_.empty(); }
  static size_t size() { return browsers_.size(); }

  // Returns iterated access to list of open browsers ordered by when
  // they were last active. The underlying data structure is a vector
  // and we push_back on recent access so a reverse iterator gives the
  // latest accessed browser first.
  static const_reverse_iterator begin_last_active() {
    return last_active_browsers_.rbegin();
  }

  static const_reverse_iterator end_last_active() {
    return last_active_browsers_.rend();
  }

  // Return the number of browsers with the following profile which are
  // currently open.
  static size_t GetBrowserCount(Profile* p);

  // Return the number of browsers with the following profile and type which are
  // currently open.
  static size_t GetBrowserCountForType(Profile* p, Browser::Type type);

  // Returns true if at least one off the record session is active.
  static bool IsOffTheRecordSessionActive();

  // Send out notifications.
  // For ChromeOS, also request session manager to end the session.
  static void NotifyAndTerminate(bool fast_path);

  // Called once there are no more browsers open and the application is exiting.
  static void AllBrowsersClosedAndAppExiting();

 private:
  // Helper method to remove a browser instance from a list of browsers
  static void RemoveBrowserFrom(Browser* browser, BrowserVector* browser_list);
  static void MarkAsCleanShutdown();
#if defined(OS_CHROMEOS)
  static bool NeedBeforeUnloadFired();
  static bool PendingDownloads();
  static void NotifyWindowManagerAboutSignout();
#endif

  static BrowserVector browsers_;
  static BrowserVector last_active_browsers_;
  static ObserverList<Observer> observers_;

  // Counter of calls to StartKeepAlive(). If non-zero, the application will
  // continue running after the last browser has exited.
  static int keep_alive_count_;
  static bool signout_;
};

class TabContents;

// Iterates through all web view hosts in all browser windows. Because the
// renderers act asynchronously, getting a host through this interface does
// not guarantee that the renderer is ready to go. Doing anything to affect
// browser windows or tabs while iterating may cause incorrect behavior.
//
// Example:
//   for (TabContentsIterator iterator; !iterator.done(); ++iterator) {
//     TabContents* cur = *iterator;
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

  // Returns the Browser instance associated with the current TabContents.
  // Valid as long as !Done()
  Browser* browser() const {
    return *browser_iterator_;
  }

  // Returns the current TabContents, valid as long as !Done()
  TabContents* operator->() const {
    return cur_;
  }
  TabContents* operator*() const {
    return cur_;
  }

  // Incrementing operators, valid as long as !Done()
  TabContents* operator++() {  // ++preincrement
    Advance();
    return cur_;
  }
  TabContents* operator++(int) {  // postincrement++
    TabContents* tmp = cur_;
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

  // Current TabContents, or NULL if we're at the end of the list. This can
  // be extracted given the browser iterator and index, but it's nice to cache
  // this since the caller may access the current host many times.
  TabContents* cur_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
