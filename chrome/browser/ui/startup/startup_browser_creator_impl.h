// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "url/gurl.h"

class Browser;
class Profile;
class StartupBrowserCreator;

namespace base {
class CommandLine;
class FilePath;
}

namespace internals {
GURL GetTriggeredResetSettingsURL();
GURL GetWelcomePageURL();
}  // namespace internals

// Assists launching the application and appending the initial tabs for a
// browser window.
class StartupBrowserCreatorImpl {
 public:
  // There are two ctors. The first one implies a NULL browser_creator object
  // and thus no access to distribution-specific first-run behaviors. The
  // second one is always called when the browser starts even if it is not
  // the first run.  |is_first_run| indicates that this is a new profile.
  StartupBrowserCreatorImpl(const base::FilePath& cur_dir,
                            const base::CommandLine& command_line,
                            chrome::startup::IsFirstRun is_first_run);
  StartupBrowserCreatorImpl(const base::FilePath& cur_dir,
                            const base::CommandLine& command_line,
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

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, RestorePinnedTabs);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest, DetermineStartupTabs);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_IncognitoOrCrash);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_MasterPrefs);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_CommandLine);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_NewTabPage);

  enum class WelcomeRunType {
    NONE,                // Do not inject the welcome page for this run.
    FIRST_TAB,           // Inject the welcome page as the first tab.
    FIRST_RUN_LAST_TAB,  // Inject the welcome page as the last first-run tab.
  };

  enum class BrowserOpenBehavior {
    NEW,                  // Open in a new browser.
    SYNCHRONOUS_RESTORE,  // Attempt a synchronous session restore.
    USE_EXISTING,         // Attempt to add to an existing tabbed browser.
  };

  // Creates a tab for each of the Tabs in |tabs|. If browser is non-null
  // and a tabbed browser, the tabs are added to it. Otherwise a new tabbed
  // browser is created and the tabs are added to it. The browser the tabs
  // are added to is returned, which is either |browser| or the newly created
  // browser.
  Browser* OpenTabsInBrowser(Browser* browser,
                             bool process_startup,
                             const StartupTabs& tabs);

  // If the process was launched with the web application command line flags,
  // e.g. --app=http://www.google.com/ or --app_id=... return true.
  // In this case |app_url| or |app_id| are populated if they're non-null.
  bool IsAppLaunch(std::string* app_url, std::string* app_id);

  // If IsAppLaunch is true, tries to open an application window.
  // If the app is specified to start in a tab, or IsAppLaunch is false,
  // returns false to specify default processing.
  bool OpenApplicationWindow(Profile* profile);

  // If IsAppLaunch is true and the user set a pref indicating that the app
  // should open in a tab, do so.
  bool OpenApplicationTab(Profile* profile);

  // Determines the URLs to be shown at startup by way of various policies
  // (onboarding, pinned tabs, etc.), determines whether a session restore
  // is necessary, and opens the URLs in a new or restored browser accordingly.
  void ProcessLaunchUrlsUsingConsolidatedFlow(
      bool process_startup,
      const std::vector<GURL>& cmd_line_urls);

  // Returns the tabs to be shown on startup, based on the policy functions in
  // the given StartupTabProvider, the given tabs passed by the command line,
  // and the interactions between those policies.
  StartupTabs DetermineStartupTabs(const StartupTabProvider& provider,
                                   const StartupTabs& cmd_line_tabs,
                                   bool is_ephemeral_profile,
                                   bool is_post_crash_launch);

  // Begins an asynchronous session restore if current state allows it (e.g.,
  // this is not process startup) and SessionService indicates that one is
  // necessary. Returns true if restore was initiated, or false if launch
  // should continue (either synchronously, or asynchronously without
  // restoring).
  bool MaybeAsyncRestore(const StartupTabs& tabs,
                         bool process_startup,
                         bool is_post_crash_launch);

  // Returns a browser displaying the contents of |tabs|. Based on |behavior|,
  // this may attempt a session restore or create a new browser. May also allow
  // DOM Storage to begin cleanup once it's clear it is not needed anymore.
  Browser* RestoreOrCreateBrowser(const StartupTabs& tabs,
                                  BrowserOpenBehavior behavior,
                                  uint32_t restore_options,
                                  bool process_startup,
                                  bool is_post_crash_launch);

  // Adds a Tab to |tabs| for each url in |urls| that doesn't already exist
  // in |tabs|.
  void AddUniqueURLs(const std::vector<GURL>& urls, StartupTabs* tabs);

  // Adds any startup infobars to the selected tab of the given browser.
  void AddInfoBarsIfNecessary(
      Browser* browser,
      chrome::startup::IsProcessStartup is_process_startup);

  // Records Rappor metrics on startup URLs.
  void RecordRapporOnStartupURLs(const std::vector<GURL>& urls_to_open);

  // Determines how the launch flow should obtain a Browser.
  static BrowserOpenBehavior DetermineBrowserOpenBehavior(
      const SessionStartupPref& pref,
      bool process_startup,
      bool is_post_crash_launch,
      bool has_restore_switch,
      bool has_new_window_switch,
      bool has_cmd_line_tabs);

  // Returns the relevant bitmask options which must be passed when restoring a
  // session.
  static uint32_t DetermineSynchronousRestoreOptions(
      bool has_create_browser_default,
      bool has_create_browser_switch,
      bool was_mac_login_or_resume);

  // TODO(crbug.com/651465): The following functions are deprecated. They will
  // be removed once kUseConsolidatedStartupFlow is enabled by default.

  // Invoked from Launch to handle processing of urls. This may do any of the
  // following:
  // . Invoke ProcessStartupURLs if |process_startup| is true.
  // . If |process_startup| is false, restore the last session if necessary,
  //   or invoke ProcessSpecifiedURLs.
  // . Open the urls directly.
  // Under the kUseConsolidatedStartupFlow feature, this is replaced by
  // ProcessLaunchUrlsUsingConsolidatedFlow().
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

  // Adds additional startup URLs to the specified vector.
  void AddStartupURLs(std::vector<GURL>* startup_urls) const;

  // Adds special URLs to the specified vector. These URLs are triggered by
  // special-case logic, such as profile reset or presentation of the welcome
  // page.
  void AddSpecialURLs(std::vector<GURL>* startup_urls) const;

  // Initializes |welcome_run_type_| for this launch. Also persists state to
  // suppress injecting the welcome page for future launches.
  void InitializeWelcomeRunType(const std::vector<GURL>& urls_to_open);

  // Checks whether |profile_| has a reset trigger set.
  bool ProfileHasResetTrigger() const;

  const base::FilePath cur_dir_;
  const base::CommandLine& command_line_;
  Profile* profile_;
  StartupBrowserCreator* browser_creator_;
  bool is_first_run_;
  WelcomeRunType welcome_run_type_;
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreatorImpl);
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_
