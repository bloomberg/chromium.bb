// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/importer/importer_observer.h"
#include "chrome/common/result_codes.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"

class CommandLine;
class FilePath;
class Profile;
class ProcessSingleton;
class ImporterHost;
class ImporterList;

// This class contains the chrome first-run installation actions needed to
// fully test the custom installer. It also contains the opposite actions to
// execute during uninstall. When the first run UI is ready we won't
// do the actions unconditionally. Currently the only action is to create a
// desktop shortcut.
//
// The way we detect first-run is by looking at a 'sentinel' file.
// If it does not exist we understand that we need to do the first time
// install work for this user. After that the sentinel file is created.
class FirstRun {
 public:
  // There are three types of possible first run bubbles:
  enum BubbleType {
    LARGE_BUBBLE,      // The normal bubble, with search engine choice
    OEM_BUBBLE,        // Smaller bubble for OEM builds
    MINIMAL_BUBBLE     // Minimal bubble shown after search engine dialog
  };

  // See ProcessMasterPreferences for more info about this structure.
  struct MasterPrefs {
    MasterPrefs();
    ~MasterPrefs();

    int ping_delay;
    bool homepage_defined;
    int do_import_items;
    int dont_import_items;
    bool run_search_engine_experiment;
    bool randomize_search_engine_experiment;
    bool make_chrome_default;
    std::vector<GURL> new_tabs;
    std::vector<GURL> bookmarks;
  };

#if defined(OS_WIN)
  // Creates the desktop shortcut to chrome for the current user. Returns
  // false if it fails. It will overwrite the shortcut if it exists.
  static bool CreateChromeDesktopShortcut();
  // Creates the quick launch shortcut to chrome for the current user. Returns
  // false if it fails. It will overwrite the shortcut if it exists.
  static bool CreateChromeQuickLaunchShortcut();
  // Returns true if we are being run in a locale in which search experiments
  // are allowed.
  static bool InSearchExperimentLocale();
#endif  // OS_WIN
  // Import bookmarks and/or browser items (depending on platform support)
  // in this process. This function is paired with FirstRun::ImportSettings().
  // This function might or might not show a visible UI depending on the
  // cmdline parameters.
  static int ImportNow(Profile* profile, const CommandLine& cmdline);

  // Automatically import history and home page (and search engine, if
  // nonorganic).
  static void AutoImport(
      Profile* profile,
      bool homepage_defined,
      int import_items,
      int dont_import_items,
      bool search_engine_experiment,
      bool randomize_search_engine_experiment,
      bool make_chrome_default,
      ProcessSingleton* process_singleton);

  // Does platform specific setup. Called at the start of AutoImport.
  static void PlatformSetup();

  // Returns whether the current install is "organic".
  static bool IsOrganic();

  // Shows the search engine choice dialog, and any other platform dialogs.
  // Only called in "organic" installs.
  static void ShowFirstRunDialog(Profile* profile,
                                 bool randomize_search_engine_experiment);

  // The master preferences is a JSON file with the same entries as the
  // 'Default\Preferences' file. This function locates this file from a standard
  // location and processes it so it becomes the default preferences in the
  // profile pointed to by |user_data_dir|. After processing the file, the
  // function returns true if and only if showing the first run dialog is
  // needed. The detailed settings in the preference file are reported via
  // |preference_details|.
  //
  // This function destroys any existing prefs file and it is meant to be
  // invoked only on first run.
  //
  // See chrome/installer/util/master_preferences.h for a description of
  // 'master_preferences' file.
  static bool ProcessMasterPreferences(const FilePath& user_data_dir,
                                       MasterPrefs* out_prefs);

  // Returns true if this is the first time chrome is run for this user.
  static bool IsChromeFirstRun();
  // Creates the sentinel file that signals that chrome has been configured.
  static bool CreateSentinel();
  // Removes the sentinel file created in ConfigDone(). Returns false if the
  // sentinel file could not be removed.
  static bool RemoveSentinel();
  // Imports settings. This may be done in a separate process depending on the
  // platform, but it will always block until done. The return value indicates
  // success.
  static bool ImportSettings(Profile* profile,
                             scoped_refptr<ImporterHost> importer_host,
                             scoped_refptr<ImporterList> importer_list,
                             int items_to_import);

  // Sets the kShouldShowFirstRunBubble local state pref so that the browser
  // shows the bubble once the main message loop gets going (or refrains from
  // showing the bubble, if |show_bubble| is false). Returns false if the pref
  // could not be set. This function can be called multiple times, but only the
  // initial call will actually set the preference.
  static bool SetShowFirstRunBubblePref(bool show_bubble);

  // Sets the kShouldUseOEMFirstRunBubble local state pref so that the
  // browser shows the OEM first run bubble once the main message loop
  // gets going. Returns false if the pref could not be set.
  static bool SetOEMFirstRunBubblePref();

  // Sets the kShouldUseMinimalFirstRunBubble local state pref so that the
  // browser shows the minimal first run bubble once the main message loop
  // gets going. Returns false if the pref could not be set.
  static bool SetMinimalFirstRunBubblePref();

  // Sets the kShouldShowWelcomePage local state pref so that the browser
  // loads the welcome tab once the message loop gets going. Returns false
  // if the pref could not be set.
  static bool SetShowWelcomePagePref();

  // Sets the kAutoFillPersonalDataManagerFirstRun local state pref so that the
  // browser loads PersonalDataManager once the main message loop gets going.
  // Returns false if the pref could not be set.
  static bool SetPersonalDataManagerFirstRunPref();

 private:
  friend class FirstRunTest;
  FRIEND_TEST_ALL_PREFIXES(Toolbar5ImporterTest, BookmarkParse);

#if defined(OS_WIN)
  // Writes the EULA to a temporary file, returned in |*eula_path|, and returns
  // true if successful.
  static bool WriteEULAtoTempFile(FilePath* eula_path);
  // Launches the setup exe with the given parameter/value on the command-line,
  // waits for its termination, returns its exit code in |*ret_code|, and
  // returns true if the exit code is valid.
  static bool LaunchSetupWithParam(const std::string& param,
                                   const std::wstring& value,
                                   int* ret_code);
  // Installs a task to do an extensions update check once the extensions system
  // is running.
  static void DoDelayedInstallExtensions();
  // Imports settings in a separate process. It is the implementation of the
  // public version.  |skip_first_run_ui| is true if no first run UI should
  // appear (search engine dialog, Firefox import warning dialog).
  static bool ImportSettings(Profile* profile, int browser_type,
                             int items_to_import,
                             const FilePath& import_path,
                             bool skip_first_run_ui,
                             gfx::NativeView parent_window);
  // Import browser items in this process. The browser and the items to
  // import are encoded int the command line.
  static int ImportFromBrowser(Profile* profile, const CommandLine& cmdline);
#else
  static bool ImportBookmarks(const FilePath& import_bookmarks_path);
#endif

  // Import bookmarks from an html file. The path to the file is provided in
  // the command line.
  static int ImportFromFile(Profile* profile, const CommandLine& cmdline);

  // Gives the full path to the sentinel file. The file might not exist.
  static bool GetFirstRunSentinelFilePath(FilePath* path);

  enum FirstRunState {
    FIRST_RUN_UNKNOWN,  // The state is not tested or set yet.
    FIRST_RUN_TRUE,
    FIRST_RUN_FALSE
  };
  // This variable should only be accessed through IsChromeFirstRun().
  static FirstRunState first_run_;

  // This class is for scoping purposes.
  DISALLOW_IMPLICIT_CONSTRUCTORS(FirstRun);
};

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
// This class contains the actions that need to be performed when an upgrade
// is required. This involves mainly swapping the chrome exe and relaunching
// the new browser.
class Upgrade {
 public:
#if defined(OS_WIN)
  // Possible results of ShowTryChromeDialog().
  enum TryResult {
    TD_TRY_CHROME,          // Launch chrome right now.
    TD_NOT_NOW,             // Don't launch chrome. Exit now.
    TD_UNINSTALL_CHROME,    // Initiate chrome uninstall and exit.
    TD_DIALOG_ERROR,        // An error occurred creating the dialog.
    TD_LAST_ENUM
  };

  // Check if current chrome.exe is already running as a browser process by
  // trying to create a Global event with name same as full path of chrome.exe.
  // This method caches the handle to this event so on subsequent calls also
  // it can first close the handle and check for any other process holding the
  // handle to the event.
  static bool IsBrowserAlreadyRunning();

  // If the new_chrome.exe exists (placed by the installer then is swapped
  // to chrome.exe and the old chrome is renamed to old_chrome.exe. If there
  // is no new_chrome.exe or the swap fails the return is false;
  static bool SwapNewChromeExeIfPresent();

  // Combines the two methods, RelaunchChromeBrowser and
  // SwapNewChromeExeIfPresent, to perform the rename and relaunch of
  // the browser. Note that relaunch does NOT exit the existing browser process.
  // If this is called before message loop is executed, simply exit the main
  // function. If browser is already running, you will need to exit it.
  static bool DoUpgradeTasks(const CommandLine& command_line);

  // Shows a modal dialog asking the user to give chrome another try. See
  // above for the possible outcomes of the function. This is an experimental,
  // non-localized dialog.
  // |version| can be 0, 1 or 2 and selects what strings to present.
  // |process_singleton| needs to be valid and it will be locked while
  // the dialog is shown.
  static TryResult ShowTryChromeDialog(size_t version,
                                       ProcessSingleton* process_singleton);
#endif  // OS_WIN

  // Launches chrome again simulating a 'user' launch. If chrome could not
  // be launched the return is false.
  static bool RelaunchChromeBrowser(const CommandLine& command_line);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  static void SaveLastModifiedTimeOfExe();
#endif

  static void SetNewCommandLine(CommandLine* new_command_line) {
    // Takes ownership of the pointer.
    new_command_line_ = new_command_line;
  }

  // Launches a new instance of the browser if the current instance in
  // persistent mode an upgrade is detected.
  static void RelaunchChromeBrowserWithNewCommandLineIfNeeded();

  // Windows:
  //  Checks if chrome_new.exe is present in the current instance's install.
  // Linux:
  //  Checks if the last modified time of chrome is newer than that of the
  //  current running instance.
  static bool IsUpdatePendingRestart();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
 private:
  static double GetLastModifiedTimeOfExe();
  static double saved_last_modified_time_of_exe_;
#endif
  static CommandLine* new_command_line_;
};
#endif  // (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)

// A subclass of BrowserProcessImpl that does not have a GoogleURLTracker or
// IntranetRedirectDetector so we don't do any URL fetches (as we have no IO
// thread to fetch on).
class FirstRunBrowserProcess : public BrowserProcessImpl {
 public:
  explicit FirstRunBrowserProcess(const CommandLine& command_line);
  virtual ~FirstRunBrowserProcess();

  virtual GoogleURLTracker* google_url_tracker();
  virtual IntranetRedirectDetector* intranet_redirect_detector();

 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunBrowserProcess);
};

// This class is used by FirstRun::ImportNow to get notified of the outcome of
// the import operation. It differs from ImportProcessRunner in that this
// class executes in the context of importing child process.
// The values that it handles are meant to be used as the process exit code.
class FirstRunImportObserver : public ImporterObserver {
 public:
  FirstRunImportObserver();

  int import_result() const;
  void RunLoop();

 private:
  void Finish();

  // ImporterObserver:
  virtual void ImportCompleted() OVERRIDE;
  virtual void ImportCanceled() OVERRIDE;

  bool loop_running_;
  int import_result_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunImportObserver);
};

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
