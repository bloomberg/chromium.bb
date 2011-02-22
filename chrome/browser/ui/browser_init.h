// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INIT_H_
#define CHROME_BROWSER_UI_BROWSER_INIT_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "googleurl/src/gurl.h"

class Browser;
class CommandLine;
class GURL;
class Profile;
class TabContents;

// class containing helpers for BrowserMain to spin up a new instance and
// initialize the profile.
class BrowserInit {
 public:
  BrowserInit();
  ~BrowserInit();

  // Adds a url to be opened during first run. This overrides the standard
  // tabs shown at first run.
  void AddFirstRunTab(const GURL& url);

  // This function is equivalent to ProcessCommandLine but should only be
  // called during actual process startup.
  bool Start(const CommandLine& cmd_line, const FilePath& cur_dir,
             Profile* profile, int* return_code) {
    return ProcessCmdLineImpl(cmd_line, cur_dir, true, profile, return_code,
                              this);
  }

  // This function performs command-line handling and is invoked when process
  // starts as well as when we get a start request from another process (via the
  // WM_COPYDATA message). |command_line| holds the command line we need to
  // process - either from this process or from some other one (if
  // |process_startup| is true and we are being called from
  // ProcessSingleton::OnCopyData).
  static bool ProcessCommandLine(const CommandLine& cmd_line,
                                 const FilePath& cur_dir, bool process_startup,
                                 Profile* profile, int* return_code) {
    return ProcessCmdLineImpl(cmd_line, cur_dir, process_startup, profile,
                              return_code, NULL);
  }

  template <class AutomationProviderClass>
  static bool CreateAutomationProvider(const std::string& channel_id,
                                       Profile* profile,
                                       size_t expected_tabs);

  // Returns true if the browser is coming up.
  static bool InProcessStartup();

  // Launches a browser window associated with |profile|. |command_line| should
  // be the command line passed to this process. |cur_dir| can be empty, which
  // implies that the directory of the executable should be used.
  // |process_startup| indicates whether this is the first browser.
  bool LaunchBrowser(const CommandLine& command_line, Profile* profile,
                     const FilePath& cur_dir, bool process_startup,
                     int* return_code);

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

    // There are two ctors. The first one implies a NULL browser_init object
    // and thus no access to distribution-specific first-run behaviors. The
    // second one is always called when the browser starts even if it is not
    // the first run.
    LaunchWithProfile(const FilePath& cur_dir, const CommandLine& command_line);
    LaunchWithProfile(const FilePath& cur_dir, const CommandLine& command_line,
                      BrowserInit* browser_init);
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

    // Invoked from OpenURLsInBrowser to handle processing of urls. This may
    // do any of the following:
    // . Invoke ProcessStartupURLs if |process_startup| is true.
    // . Restore the last session if necessary.
    // . Open the urls directly.
    void ProcessLaunchURLs(bool process_startup,
                           const std::vector<GURL>& urls_to_open);

    // Does the following:
    // . If the user's startup pref is to restore the last session (or the
    //   command line flag is present to force using last session), it is
    //   restored, and true is returned.
    // . Attempts to restore any pinned tabs from last run of chrome and:
    //   . If urls_to_open is non-empty, they are opened and true is returned.
    //   . If the user's startup pref is to launch a specific set of URLs they
    //     are opened.
    //
    // Otherwise false is returned, which indicates the caller must create a
    // new browser.
    bool ProcessStartupURLs(const std::vector<GURL>& urls_to_open);

    // Adds any startup infobars to the selected tab of the given browser.
    void AddInfoBarsIfNecessary(Browser* browser);

    // If the last session didn't exit cleanly and tab is a web contents tab,
    // an infobar is added allowing the user to restore the last session.
    void AddCrashedInfoBarIfNecessary(TabContents* tab);

    // If we have been started with unsupported flags like --single-process,
    // politely nag the user about it.
    void AddBadFlagsInfoBarIfNecessary(TabContents* tab);

    // Adds additional startup URLs to the specified vector.
    void AddStartupURLs(std::vector<GURL>* startup_urls) const;

    // Checks whether Chrome is still the default browser (unless the user
    // previously instructed not to do so) and warns the user if it is not.
    void CheckDefaultBrowser(Profile* profile);

    const FilePath cur_dir_;
    const CommandLine& command_line_;
    Profile* profile_;
    BrowserInit* browser_init_;
    DISALLOW_COPY_AND_ASSIGN(LaunchWithProfile);
  };

 private:
  // Returns the list of URLs to open from the command line. The returned
  // vector is empty if the user didn't specify any URLs on the command line.
  static std::vector<GURL> GetURLsFromCommandLine(
      const CommandLine& command_line,
      const FilePath& cur_dir,
      Profile* profile);

  static bool ProcessCmdLineImpl(const CommandLine& command_line,
                                 const FilePath& cur_dir, bool process_startup,
                                 Profile* profile, int* return_code,
                                 BrowserInit* browser_init);

  // Additional tabs to open during first run.
  std::vector<GURL> first_run_tabs_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInit);
};

#endif  // CHROME_BROWSER_UI_BROWSER_INIT_H_
