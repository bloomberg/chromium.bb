// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_UI_TEST_UTILS_H_
#define CHROME_TEST_BASE_UI_TEST_UTILS_H_

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/history/history.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/glue/window_open_disposition.h"

class AppModalDialog;
class BookmarkModel;
class Browser;
class FilePath;
class LocationBar;
class Profile;
class SkBitmap;
class TemplateURLService;

namespace chrome {
struct NavigateParams;
}

namespace content {
class MessageLoopRunner;
class RenderViewHost;
class RenderWidgetHost;
class WebContents;
}

namespace gfx {
class Rect;
class Size;
}

// A collections of functions designed for use with InProcessBrowserTest.
namespace ui_test_utils {

// Flags to indicate what to wait for in a navigation test.
// They can be ORed together.
// The order in which the waits happen when more than one is selected, is:
//    Browser
//    Tab
//    Navigation
enum BrowserTestWaitFlags {
  BROWSER_TEST_NONE = 0,                      // Don't wait for anything.
  BROWSER_TEST_WAIT_FOR_BROWSER = 1 << 0,     // Wait for a new browser.
  BROWSER_TEST_WAIT_FOR_TAB = 1 << 1,         // Wait for a new tab.
  BROWSER_TEST_WAIT_FOR_NAVIGATION = 1 << 2,  // Wait for navigation to finish.

  BROWSER_TEST_MASK = BROWSER_TEST_WAIT_FOR_BROWSER |
                      BROWSER_TEST_WAIT_FOR_TAB |
                      BROWSER_TEST_WAIT_FOR_NAVIGATION
};

// Puts the current tab title in |title|. Returns true on success.
bool GetCurrentTabTitle(const Browser* browser, string16* title);

// Opens |url| in an incognito browser window with the incognito profile of
// |profile|, blocking until the navigation finishes. This will create a new
// browser if a browser with the incognito profile does not exist. Returns the
// incognito window Browser.
Browser* OpenURLOffTheRecord(Profile* profile, const GURL& url);

// Performs the provided navigation process, blocking until the navigation
// finishes. May change the params in some cases (i.e. if the navigation
// opens a new browser window). Uses chrome::Navigate.
void NavigateToURL(chrome::NavigateParams* params);

// Navigates the selected tab of |browser| to |url|, blocking until the
// navigation finishes. Uses Browser::OpenURL --> chrome::Navigate.
void NavigateToURL(Browser* browser, const GURL& url);

// Navigates the specified tab of |browser| to |url|, blocking until the
// navigation finishes.
// |disposition| indicates what tab the navigation occurs in, and
// |browser_test_flags| controls what to wait for before continuing.
void NavigateToURLWithDisposition(Browser* browser,
                                  const GURL& url,
                                  WindowOpenDisposition disposition,
                                  int browser_test_flags);

// Navigates the selected tab of |browser| to |url|, blocking until the
// number of navigations specified complete.
void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations);

// Generate the file path for testing a particular test.
// The file for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is FilePath format.
FilePath GetTestFilePath(const FilePath& dir, const FilePath& file);

// Generate the URL for testing a particular test.
// HTML for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is GURL format.
GURL GetTestUrl(const FilePath& dir, const FilePath& file);

// Generate the path of the build directory, relative to the source root.
bool GetRelativeBuildDirectory(FilePath* build_dir);

// Blocks until an application modal dialog is showns and returns it.
AppModalDialog* WaitForAppModalDialog();

// Performs a find in the page of the specified tab. Returns the number of
// matches found.  |ordinal| is an optional parameter which is set to the index
// of the current match. |selection_rect| is an optional parameter which is set
// to the location of the current match.
int FindInPage(content::WebContents* tab,
               const string16& search_string,
               bool forward,
               bool case_sensitive,
               int* ordinal,
               gfx::Rect* selection_rect);

// Register |observer| for the given |type| and |source| and run
// the message loop until the observer posts a quit task.
void RegisterAndWait(content::NotificationObserver* observer,
                     int type,
                     const content::NotificationSource& source);

// Blocks until |model| finishes loading.
void WaitForBookmarkModelToLoad(BookmarkModel* model);

// Blocks until |service| finishes loading.
void WaitForTemplateURLServiceToLoad(TemplateURLService* service);

// Blocks until the |history_service|'s history finishes loading.
void WaitForHistoryToLoad(HistoryService* history_service);

// Download the given file and waits for the download to complete.
void DownloadURL(Browser* browser, const GURL& download_url);

// Send the given text to the omnibox and wait until it's updated.
void SendToOmniboxAndSubmit(LocationBar* location_bar,
                            const std::string& input);

// Gets the first browser that is not in the specified set.
Browser* GetBrowserNotInSet(std::set<Browser*> excluded_browsers);

// A WindowedNotificationObserver hard-wired to observe
// chrome::NOTIFICATION_TAB_ADDED.
class WindowedTabAddedNotificationObserver
    : public content::WindowedNotificationObserver {
 public:
  // Register to listen for notifications of NOTIFICATION_TAB_ADDED from either
  // a specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  explicit WindowedTabAddedNotificationObserver(
      const content::NotificationSource& source);

  // Returns the added tab, or NULL if no notification was observed yet.
  content::WebContents* GetTab() { return added_tab_; }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  content::WebContents* added_tab_;

  DISALLOW_COPY_AND_ASSIGN(WindowedTabAddedNotificationObserver);
};

// Similar to WindowedNotificationObserver but also provides a way of retrieving
// the details associated with the notification.
// Note that in order to use that class the details class should be copiable,
// which is the case with most notifications.
template <class U>
class WindowedNotificationObserverWithDetails
    : public content::WindowedNotificationObserver {
 public:
  WindowedNotificationObserverWithDetails(
      int notification_type,
      const content::NotificationSource& source)
      : content::WindowedNotificationObserver(notification_type, source) {}

  // Fills |details| with the details of the notification received for |source|.
  bool GetDetailsFor(uintptr_t source, U* details) {
    typename std::map<uintptr_t, U>::const_iterator iter =
        details_.find(source);
    if (iter == details_.end())
      return false;
    *details = iter->second;
    return true;
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    const U* details_ptr = content::Details<U>(details).ptr();
    if (details_ptr)
      details_[source.map_key()] = *details_ptr;
    content::WindowedNotificationObserver::Observe(type, source, details);
  }

 private:
  std::map<uintptr_t, U> details_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserverWithDetails);
};

// Notification observer which waits for navigation events and blocks until
// a specific URL is loaded. The URL must be an exact match.
class UrlLoadObserver : public content::WindowedNotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  UrlLoadObserver(const GURL& url, const content::NotificationSource& source);
  virtual ~UrlLoadObserver();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(UrlLoadObserver);
};

// Convenience class for waiting for a new browser to be created.
// Like WindowedNotificationObserver, this class provides a safe, non-racey
// way to wait for a new browser to be created.
class BrowserAddedObserver {
 public:
  BrowserAddedObserver();
  ~BrowserAddedObserver();

  // Wait for a new browser to be created, and return a pointer to it.
  Browser* WaitForSingleNewBrowser();

 private:
  content::WindowedNotificationObserver notification_observer_;
  std::set<Browser*> original_browsers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAddedObserver);
};

// Takes a snapshot of the given render widget, rendered at |page_size|. The
// snapshot is set to |bitmap|. Returns true on success.
bool TakeRenderWidgetSnapshot(content::RenderWidgetHost* rwh,
                              const gfx::Size& page_size,
                              SkBitmap* bitmap) WARN_UNUSED_RESULT;

// Takes a snapshot of the entire page, according to the width and height
// properties of the DOM's document. Returns true on success. DOMAutomation
// must be enabled.
bool TakeEntirePageSnapshot(content::RenderViewHost* rvh,
                            SkBitmap* bitmap) WARN_UNUSED_RESULT;

#if defined(OS_WIN)
// Saves a snapshot of the entire screen to a file named
// ChromiumSnapshotYYYYMMDDHHMMSS.png to |directory|, returning true on success.
// The path to the file produced is returned in |screenshot_path| if non-NULL.
bool SaveScreenSnapshotToDirectory(const FilePath& directory,
                                   FilePath* screenshot_path);

// Saves a snapshot of the entire screen as above to the current user's desktop.
// The Chrome path provider must be registered prior to calling this function.
bool SaveScreenSnapshotToDesktop(FilePath* screenshot_path);
#endif

// Configures the geolocation provider to always return the given position.
void OverrideGeolocation(double latitude, double longitude);

// Enumerates all history contents on the backend thread. Returns them in
// descending order by time.
class HistoryEnumerator {
 public:
  explicit HistoryEnumerator(Profile* profile);
  ~HistoryEnumerator();

  std::vector<GURL>& urls() { return urls_; }

 private:
  void HistoryQueryComplete(
      const base::Closure& quit_task,
      HistoryService::Handle request_handle,
      history::QueryResults* results);

  std::vector<GURL> urls_;

  CancelableRequestConsumer consumer_;

  DISALLOW_COPY_AND_ASSIGN(HistoryEnumerator);
};

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_UI_TEST_UTILS_H_
