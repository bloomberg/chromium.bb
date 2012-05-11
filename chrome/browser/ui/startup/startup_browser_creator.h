// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "googleurl/src/gurl.h"

class Browser;
class CommandLine;
class GURL;
class PrefService;
class TabContentsWrapper;

// class containing helpers for BrowserMain to spin up a new instance and
// initialize the profile.
class StartupBrowserCreator {
 public:
  typedef std::vector<Profile*> Profiles;

  enum IsProcessStartup {
    IS_NOT_PROCESS_STARTUP,
    IS_PROCESS_STARTUP
  };
  enum IsFirstRun {
    IS_NOT_FIRST_RUN,
    IS_FIRST_RUN
  };

  StartupBrowserCreator();
  ~StartupBrowserCreator();

  // Adds a url to be opened during first run. This overrides the standard
  // tabs shown at first run.
  void AddFirstRunTab(const GURL& url);

  // This function is equivalent to ProcessCommandLine but should only be
  // called during actual process startup.
  bool Start(const CommandLine& cmd_line,
             const FilePath& cur_dir,
             Profile* last_used_profile,
             const Profiles& last_opened_profiles,
             int* return_code) {
    return ProcessCmdLineImpl(cmd_line, cur_dir, true, last_used_profile,
                              last_opened_profiles, return_code, this);
  }

  // This function performs command-line handling and is invoked only after
  // start up (for example when we get a start request for another process).
  // |command_line| holds the command line we need to process
  static void ProcessCommandLineAlreadyRunning(const CommandLine& cmd_line,
                                               const FilePath& cur_dir);

  template <class AutomationProviderClass>
  static bool CreateAutomationProvider(const std::string& channel_id,
                                       Profile* profile,
                                       size_t expected_tabs);

  // Returns true if we're launching a profile synchronously. In that case, the
  // opened window should not cause a session restore.
  static bool InSynchronousProfileLaunch();

  // Launches a browser window associated with |profile|. |command_line| should
  // be the command line passed to this process. |cur_dir| can be empty, which
  // implies that the directory of the executable should be used.
  // |process_startup| indicates whether this is the first browser.
  // |is_first_run| indicates that this is a new profile.
  bool LaunchBrowser(const CommandLine& command_line,
                     Profile* profile,
                     const FilePath& cur_dir,
                     IsProcessStartup is_process_startup,
                     IsFirstRun is_first_run,
                     int* return_code);

  // When called the first time, reads the value of the preference kWasRestarted
  // and resets it to false. Subsequent calls return the value which was read
  // the first time.
  static bool WasRestarted();

  static SessionStartupPref GetSessionStartupPref(
      const CommandLine& command_line,
      Profile* profile);

  // LaunchWithProfile ---------------------------------------------------------
  //
  // Assists launching the application and appending the initial tabs for a
  // browser window.

  class LaunchWithProfile {
   public:
    // Used by OpenTabsInBrowser.
    struct Tab {
      Tab();
      ~Tab();

      // The url to load.
      GURL url;

      // If true, the tab corresponds to an app an |app_id| gives the id of the
      // app.
      bool is_app;

      // True if the is tab pinned.
      bool is_pinned;

      // Id of the app.
      std::string app_id;
    };

    // There are two ctors. The first one implies a NULL browser_creator object
    // and thus no access to distribution-specific first-run behaviors. The
    // second one is always called when the browser starts even if it is not
    // the first run.  |is_first_run| indicates that this is a new profile.
    LaunchWithProfile(const FilePath& cur_dir,
                      const CommandLine& command_line,
                      IsFirstRun is_first_run);
    LaunchWithProfile(const FilePath& cur_dir,
                      const CommandLine& command_line,
                      StartupBrowserCreator* browser_creator,
                      IsFirstRun is_first_run);
    ~LaunchWithProfile();

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
                               const std::vector<Tab>& tabs);

   private:
    FRIEND_TEST_ALL_PREFIXES(BrowserTest, RestorePinnedTabs);
    FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);

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
    void AddUniqueURLs(const std::vector<GURL>& urls,
                       std::vector<Tab>* tabs);

    // Adds any startup infobars to the selected tab of the given browser.
    void AddInfoBarsIfNecessary(Browser* browser,
                                IsProcessStartup is_process_startup);

    // Adds additional startup URLs to the specified vector.
    void AddStartupURLs(std::vector<GURL>* startup_urls) const;

    // Checks whether the Preferences backup is invalid and notifies user in
    // that case.
    void CheckPreferencesBackup(Profile* profile);

    const FilePath cur_dir_;
    const CommandLine& command_line_;
    Profile* profile_;
    StartupBrowserCreator* browser_creator_;
    bool is_first_run_;
    DISALLOW_COPY_AND_ASSIGN(LaunchWithProfile);
  };

 private:
  friend class CloudPrintProxyPolicyTest;
  friend class CloudPrintProxyPolicyStartupTest;
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ReadingWasRestartedAfterNormalStart);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ReadingWasRestartedAfterRestart);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, UpdateWithTwoProfiles);

  // Returns the list of URLs to open from the command line. The returned
  // vector is empty if the user didn't specify any URLs on the command line.
  static std::vector<GURL> GetURLsFromCommandLine(
      const CommandLine& command_line,
      const FilePath& cur_dir,
      Profile* profile);

  static bool ProcessCmdLineImpl(const CommandLine& command_line,
                                 const FilePath& cur_dir,
                                 bool process_startup,
                                 Profile* last_used_profile,
                                 const Profiles& last_opened_profiles,
                                 int* return_code,
                                 StartupBrowserCreator* browser_creator);

  // Callback after a profile has been created.
  static void ProcessCommandLineOnProfileCreated(
      const CommandLine& cmd_line,
      const FilePath& cur_dir,
      Profile* profile,
      Profile::CreateStatus status);

  // Additional tabs to open during first run.
  std::vector<GURL> first_run_tabs_;

  // True if we have already read and reset the preference kWasRestarted. (A
  // member variable instead of a static variable inside WasRestarted because
  // of testing.)
  static bool was_restarted_read_;

  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreator);
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
