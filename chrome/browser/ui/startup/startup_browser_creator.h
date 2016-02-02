// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "url/gurl.h"

class Browser;
class GURL;
class PrefRegistrySimple;
class PrefService;

namespace base {
class CommandLine;
}

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
  bool Start(const base::CommandLine& cmd_line,
             const base::FilePath& cur_dir,
             Profile* last_used_profile,
             const Profiles& last_opened_profiles);

  // This function performs command-line handling and is invoked only after
  // start up (for example when we get a start request for another process).
  // |command_line| holds the command line we need to process.
  // |cur_dir| is the current working directory that the original process was
  // invoked from.
  // |startup_profile_dir| is the directory that contains the profile that the
  // command line arguments will be executed under.
  static void ProcessCommandLineAlreadyRunning(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      const base::FilePath& startup_profile_dir);

  // Returns true if we're launching a profile synchronously. In that case, the
  // opened window should not cause a session restore.
  static bool InSynchronousProfileLaunch();

  // Launches a browser window associated with |profile|. |command_line| should
  // be the command line passed to this process. |cur_dir| can be empty, which
  // implies that the directory of the executable should be used.
  // |process_startup| indicates whether this is the first browser.
  // |is_first_run| indicates that this is a new profile.
  bool LaunchBrowser(const base::CommandLine& command_line,
                     Profile* profile,
                     const base::FilePath& cur_dir,
                     chrome::startup::IsProcessStartup is_process_startup,
                     chrome::startup::IsFirstRun is_first_run);

  // When called the first time, reads the value of the preference kWasRestarted
  // and resets it to false. Subsequent calls return the value which was read
  // the first time.
  static bool WasRestarted();

  static SessionStartupPref GetSessionStartupPref(
      const base::CommandLine& command_line,
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

  bool show_desktop_search_redirection_infobar() const {
    return show_desktop_search_redirection_infobar_;
  }

  // For faking that no profiles have been launched yet.
  static void ClearLaunchedProfilesForTesting();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

#if defined(OS_WIN)
  // Setting Chrome as the default browser in Windows 10+ requires a specific
  // url to be opened through openwith.exe. This url is intercepted in
  // ProcessCmdLineImpl when the callback is set. See DefaultBrowserWorker in
  // shell_integration.h for more details. Only call this on the UI
  // thread.
  //
  // Returns false when the default browser callback was already set which
  // results in a no-op.
  static bool SetDefaultBrowserCallback(const base::Closure& callback);

  // Clears the callback when it isn't needed anymore. Only call this on the UI
  // thread.
  static void ClearDefaultBrowserCallback();

  // Returns the url used to set Chrome as the default browser asynchronously.
  static const wchar_t* GetDefaultBrowserUrl();
#endif  // defined(OS_WIN)

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

  bool ProcessCmdLineImpl(const base::CommandLine& command_line,
                          const base::FilePath& cur_dir,
                          bool process_startup,
                          Profile* last_used_profile,
                          const Profiles& last_opened_profiles);

  // Returns the list of URLs to open from the command line. The returned vector
  // is empty if the user didn't specify any URLs on the command line.
  // |show_search_redirection_infobar| is set to true if an infobar should be
  // shown to inform the user that a desktop search has been redirected to the
  // default search engine.
  static std::vector<GURL> GetURLsFromCommandLine(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      Profile* profile,
      bool* show_desktop_search_redirection_infobar);

  // This function performs command-line handling and is invoked only after
  // start up (for example when we get a start request for another process).
  // |command_line| holds the command line being processed.
  // |cur_dir| is the current working directory that the original process was
  // invoked from.
  // |profile| is the profile the apps will be launched in.
  static bool ProcessLoadApps(const base::CommandLine& command_line,
                              const base::FilePath& cur_dir,
                              Profile* profile);

  // Callback after a profile has been created.
  static void ProcessCommandLineOnProfileCreated(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      Profile* profile,
      Profile::CreateStatus status);

  // Returns true once a profile was activated. Used by the
  // StartupBrowserCreatorTest.LastUsedProfileActivated test.
  static bool ActivatedProfile();

  // Additional tabs to open during first run.
  std::vector<GURL> first_run_tabs_;

  // True if the set-as-default dialog has been explicitly suppressed.
  // This information is used to allow the default browser prompt to show on
  // first-run when the dialog has been suppressed.
  bool is_default_browser_dialog_suppressed_;

  // Whether the browser window should be shown immediately after it has been
  // created. Default is true.
  bool show_main_browser_window_;

  // Whether an infobar should be shown to inform the user that a desktop search
  // has been redirected to the default search engine.
  bool show_desktop_search_redirection_infobar_;

  // True if we have already read and reset the preference kWasRestarted. (A
  // member variable instead of a static variable inside WasRestarted because
  // of testing.)
  static bool was_restarted_read_;

  static bool in_synchronous_profile_launch_;

  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreator);
};

// Returns true if |profile| has exited uncleanly and has not been launched
// after the unclean exit.
bool HasPendingUncleanExit(Profile* profile);

// Returns the path that contains the profile that should be loaded on process
// startup.
base::FilePath GetStartupProfilePath(const base::FilePath& user_data_dir,
                                     const base::CommandLine& command_line);

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
