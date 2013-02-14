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

  // Adds and removes browsers from the list they are associated with. The
  // browser object should be valid BEFORE these calls (for the benefit of
  // observers), so notify and THEN delete the object.
  static void AddBrowser(Browser* browser);
  static void RemoveBrowser(Browser* browser);

  // Adds and removes |observer| from the observer list of each desktop.
  static void AddObserver(chrome::BrowserListObserver* observer);
  static void RemoveObserver(chrome::BrowserListObserver* observer);

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was.
  static void SetLastActive(Browser* browser);

  // Closes all browsers for |profile| across all desktops.
  static void CloseAllBrowsersWithProfile(Profile* profile);

  // Returns true if at least one incognito session is active across all
  // desktops.
  static bool IsOffTheRecordSessionActive();

  // Returns true if at least one incognito session is active for |profile|
  // across all desktops.
  static bool IsOffTheRecordSessionActiveForProfile(Profile* profile);
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
