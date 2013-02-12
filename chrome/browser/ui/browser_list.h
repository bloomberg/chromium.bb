// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_H_

#include <vector>

#include "base/basictypes.h"

class Browser;
class Profile;

namespace chrome {
class BrowserListObserver;
}

// Stores a list of all Browser objects.
class BrowserList {
 public:
  typedef std::vector<Browser*> BrowserVector;
  typedef BrowserVector::const_iterator const_iterator;
  typedef BrowserVector::const_reverse_iterator const_reverse_iterator;

  // Adds and removes browsers from the global list. The browser object should
  // be valid BEFORE these calls (for the benefit of observers), so notify and
  // THEN delete the object.
  static void AddBrowser(Browser* browser);
  static void RemoveBrowser(Browser* browser);

  static void AddObserver(chrome::BrowserListObserver* observer);
  static void RemoveObserver(chrome::BrowserListObserver* observer);

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was.
  static void SetLastActive(Browser* browser);

  // Closes all browsers for |profile| across all desktops.
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
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
