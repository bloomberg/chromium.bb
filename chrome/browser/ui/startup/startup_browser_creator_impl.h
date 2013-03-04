// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "googleurl/src/gurl.h"

class Browser;
class CommandLine;
class Profile;
class StartupBrowserCreator;

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace internals {
GURL GetWelcomePageURL();
}

// Assists launching the application and appending the initial tabs for a
// browser window.
class StartupBrowserCreatorImpl {
 public:
  // There are two ctors. The first one implies a NULL browser_creator object
  // and thus no access to distribution-specific first-run behaviors. The
  // second one is always called when the browser starts even if it is not
  // the first run.  |is_first_run| indicates that this is a new profile.
  StartupBrowserCreatorImpl(const base::FilePath& cur_dir,
                            const CommandLine& command_line,
                            chrome::startup::IsFirstRun is_first_run);
  StartupBrowserCreatorImpl(const base::FilePath& cur_dir,
                            const CommandLine& command_line,
                            StartupBrowserCreator* browser_creator,
                            chrome::startup::IsFirstRun is_first_run);
  ~StartupBrowserCreatorImpl();

  // Creates the necessary windows for startup. Returns true on success,
  // false on failure. process_startup is true if Chrome is just
  // starting up. If process_startup is false, it indicates Chrome was
  // already running and the user wants to launch another instance.
  bool Launch(Profile* profile,
              const std::vector<GURL>& urls_to_open,
              bool process_startup);

  // Convenience for OpenTabsInBrowser that converts |urls| into a set of
  // Tabs.
  Browser* OpenURLsInBrowser(Browser* browser,
                             bool process_startup,
                             const std::vector<GURL>& urls);

  // Creates a tab for each of the Tabs in |tabs|. If browser is non-null
  // and a tabbed browser, the tabs are added to it. Otherwise a new tabbed
  // browser is created and the tabs are added to it. The browser the tabs
  // are added to is returned, which is either |browser| or the newly created
  // browser.
  Browser* OpenTabsInBrowser(Browser* browser,
                             bool process_startup,
                             const StartupTabs& tabs);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, RestorePinnedTabs);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);

  // Extracts optional application window size passed in command line.
  void ExtractOptionalAppWindowSize(gfx::Rect* bounds);

  // If the process was launched with the web application command line flags,
  // e.g. --app=http://www.google.com/ or --app_id=... return true.
  // In this case |app_url| or |app_id| are populated if they're non-null.
  bool IsAppLaunch(std::string* app_url, std::string* app_id);

  // If IsAppLaunch is true, tries to open an application window.
  // If the app is specified to start in a tab, or IsAppLaunch is false,
  // returns false to specify default processing. |out_app_contents| is an
  // optional argument to receive the created WebContents for the app.
  bool OpenApplicationWindow(Profile* profile,
                             content::WebContents** out_app_contents);

  // If IsAppLaunch is true and the user set a pref indicating that the app
  // should open in a tab, do so.
  bool OpenApplicationTab(Profile* profile);

  // Invoked from Launch to handle processing of urls. This may do any of the
  // following:
  // . Invoke ProcessStartupURLs if |process_startup| is true.
  // . If |process_startup| is false, restore the last session if necessary,
  //   or invoke ProcessSpecifiedURLs.
  // . Open the urls directly.
  void ProcessLaunchURLs(bool process_startup,
                         const std::vector<GURL>& urls_to_open);

  // Does the following:
  // . If the user's startup pref is to restore the last session (or the
  //   command line flag is present to force using last session), it is
  //   restored.
  // . Otherwise invoke ProcessSpecifiedURLs
  // If a browser was created, true is returned.  Otherwise returns false and
  // the caller must create a new browser.
  bool ProcessStartupURLs(const std::vector<GURL>& urls_to_open);

  // Invoked from either ProcessLaunchURLs or ProcessStartupURLs to handle
  // processing of URLs where the behavior is common between process startup
  // and launch via an existing process (i.e. those explicitly specified by
  // the user somehow).  Does the following:
  // . Attempts to restore any pinned tabs from last run of chrome.
  // . If urls_to_open is non-empty, they are opened.
  // . If the user's startup pref is to launch a specific set of URLs they
  //   are opened.
  //
  // If any tabs were opened, the Browser which was created is returned.
  // Otherwise null is returned and the caller must create a new browser.
  Browser* ProcessSpecifiedURLs(const std::vector<GURL>& urls_to_open);

  // Adds a Tab to |tabs| for each url in |urls| that doesn't already exist
  // in |tabs|.
  void AddUniqueURLs(const std::vector<GURL>& urls, StartupTabs* tabs);

  // Adds any startup infobars to the selected tab of the given browser.
  void AddInfoBarsIfNecessary(
      Browser* browser,
      chrome::startup::IsProcessStartup is_process_startup);

  // Adds additional startup URLs to the specified vector.
  void AddStartupURLs(std::vector<GURL>* startup_urls) const;

  // Checks whether the Preferences backup is invalid and notifies user in
  // that case.
  void CheckPreferencesBackup(Profile* profile);

  // Function to open startup urls in an existing Browser instance for the
  // profile passed in. Returns true on success.
  static bool OpenStartupURLsInExistingBrowser(
      Profile* profile,
      const std::vector<GURL>& startup_urls);

  const base::FilePath cur_dir_;
  const CommandLine& command_line_;
  Profile* profile_;
  StartupBrowserCreator* browser_creator_;
  bool is_first_run_;
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreatorImpl);
};

// Returns true if |profile| has exited uncleanly and has not been launched
// after the unclean exit.
bool HasPendingUncleanExit(Profile* profile);

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_
