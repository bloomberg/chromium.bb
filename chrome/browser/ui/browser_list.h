// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;

namespace browser {
class BrowserActivityObserver;
#if defined(OS_MACOSX)
Browser* GetLastActiveBrowser();
#endif
#if defined(TOOLKIT_GTK)
class ExtensionInstallDialog;
#endif
}

namespace chrome {
class BrowserListObserver;
namespace internal {
void NotifyNotDefaultBrowserCallback();
}
}

#if defined(OS_CHROMEOS)
namespace chromeos {
class ScreenLocker;
}
#endif

namespace content {
class WebContents;
}

namespace web_intents {
Browser* GetBrowserForBackgroundWebIntentDelivery(Profile*);
}

#if defined(USE_ASH)
content::WebContents* GetActiveWebContents();
#endif

// Stores a list of all Browser objects.
class BrowserList {
 public:
  typedef std::vector<Browser*> BrowserVector;
  typedef BrowserVector::iterator iterator;
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
  // DO NOT ADD MORE FRIENDS TO THIS LIST. This list should be reduced over
  // time by wiring context through to the relevant code rather than using
  // GetLastActive().
  friend class BrowserView;
  friend class ChromeShellDelegate;
  friend class NetworkProfileBubble;
  friend class PrintPreviewHandler;
  friend class SelectFileDialogExtension;
  friend class StartupBrowserCreatorImpl;
  friend class TaskManager;
  friend class WindowSizer;
  friend class browser::BrowserActivityObserver;
  friend Browser* web_intents::GetBrowserForBackgroundWebIntentDelivery(
      Profile*);
#if defined(OS_CHROMEOS)
  friend class chromeos::ScreenLocker;
#endif
#if defined(OS_MACOSX)
  friend Browser* browser::GetLastActiveBrowser();
#endif
#if defined(USE_ASH)
  friend content::WebContents* GetActiveWebContents();
#endif
  friend void chrome::internal::NotifyNotDefaultBrowserCallback();
  // DO NOT ADD MORE FRIENDS TO THIS LIST.

  // Returns the Browser object whose window was most recently active.  If the
  // most recently open Browser's window was closed, returns the first Browser
  // in the list.  If no Browsers exist, returns NULL.
  //
  // WARNING: this is NULL until a browser becomes active. If during startup
  // a browser does not become active (perhaps the user launches Chrome, then
  // clicks on another app before the first browser window appears) then this
  // returns NULL.
  // WARNING #2: this will always be NULL in unit tests run on the bots.
  // THIS FUNCTION IS PRIVATE AND NOT TO BE USED AS A REPLACEMENT FOR RELEVANT
  // CONTEXT.
  static Browser* GetLastActive();
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
