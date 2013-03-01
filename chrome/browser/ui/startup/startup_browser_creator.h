// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "googleurl/src/gurl.h"

class Browser;
class CommandLine;
class GURL;
class PrefService;

// class containing helpers for BrowserMain to spin up a new instance and
// initialize the profile.
class StartupBrowserCreator {
 public:
  typedef std::vector<Profile*> Profiles;

  StartupBrowserCreator();
  ~StartupBrowserCreator();

  // Adds a url to be opened during first run. This overrides the standard
  // tabs shown at first run.
  void AddFirstRunTab(const GURL& url);

  // This function is equivalent to ProcessCommandLine but should only be
  // called during actual process startup.
  bool Start(const CommandLine& cmd_line,
             const base::FilePath& cur_dir,
             Profile* last_used_profile,
             const Profiles& last_opened_profiles,
             int* return_code) {
    return ProcessCmdLineImpl(cmd_line, cur_dir, true, last_used_profile,
                              last_opened_profiles, return_code, this);
  }

  // This function performs command-line handling and is invoked only after
  // start up (for example when we get a start request for another process).
  // |command_line| holds the command line we need to process.
  // |cur_dir| is the current working directory that the original process was
  // invoked from.
  // |startup_profile_dir| is the directory that contains the profile that the
  // command line arguments will be executed under.
  static void ProcessCommandLineAlreadyRunning(
      const CommandLine& command_line,
      const base::FilePath& cur_dir,
      const base::FilePath& startup_profile_dir);

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
                     const base::FilePath& cur_dir,
                     chrome::startup::IsProcessStartup is_process_startup,
                     chrome::startup::IsFirstRun is_first_run,
                     int* return_code);

  // When called the first time, reads the value of the preference kWasRestarted
  // and resets it to false. Subsequent calls return the value which was read
  // the first time.
  static bool WasRestarted();

  static SessionStartupPref GetSessionStartupPref(
      const CommandLine& command_line,
      Profile* profile);

  void set_is_default_browser_dialog_suppressed(bool new_value) {
    is_default_browser_dialog_suppressed_ = new_value;
  }

  bool is_default_browser_dialog_suppressed() const {
    return is_default_browser_dialog_suppressed_;
  }

  void set_show_main_browser_window(bool show_main_browser_window) {
    show_main_browser_window_ = show_main_browser_window;
  }

  bool show_main_browser_window() const {
    return show_main_browser_window_;
  }

  // For faking that no profiles have been launched yet.
  static void ClearLaunchedProfilesForTesting();

 private:
  friend class CloudPrintProxyPolicyTest;
  friend class CloudPrintProxyPolicyStartupTest;
  friend class StartupBrowserCreatorImpl;
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ReadingWasRestartedAfterNormalStart);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ReadingWasRestartedAfterRestart);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, UpdateWithTwoProfiles);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, LastUsedProfileActivated);

  // Returns the list of URLs to open from the command line. The returned
  // vector is empty if the user didn't specify any URLs on the command line.
  static std::vector<GURL> GetURLsFromCommandLine(
      const CommandLine& command_line,
      const base::FilePath& cur_dir,
      Profile* profile);

  static bool ProcessCmdLineImpl(const CommandLine& command_line,
                                 const base::FilePath& cur_dir,
                                 bool process_startup,
                                 Profile* last_used_profile,
                                 const Profiles& last_opened_profiles,
                                 int* return_code,
                                 StartupBrowserCreator* browser_creator);

  // Callback after a profile has been created.
  static void ProcessCommandLineOnProfileCreated(
      const CommandLine& command_line,
      const base::FilePath& cur_dir,
      Profile* profile,
      Profile::CreateStatus status);

  // Returns true once a profile was activated. Used by the
  // StartupBrowserCreatorTest.LastUsedProfileActivated test.
  static bool ActivatedProfile();

  // Additional tabs to open during first run.
  std::vector<GURL> first_run_tabs_;

  // True if the set-as-default dialog has been explicitly supressed.
  // This information is used to allow the default browser prompt to show on
  // first-run when the dialog has been suppressed.
  bool is_default_browser_dialog_suppressed_;

  // Whether the browser window should be shown immediately after it has been
  // created. Default is true.
  bool show_main_browser_window_;

  // True if we have already read and reset the preference kWasRestarted. (A
  // member variable instead of a static variable inside WasRestarted because
  // of testing.)
  static bool was_restarted_read_;

  static bool in_synchronous_profile_launch_;

  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreator);
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
