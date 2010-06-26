// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/app/breakpad_mac.h"
#import "chrome/browser/cocoa/first_run_dialog.h"
#import "chrome/browser/cocoa/import_progress_dialog.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"

// Class that handles conducting the first run operation.
// FirstRunController deletes itself when the first run operation ends.
class FirstRunController : public ImportObserver {
 public:
  explicit FirstRunController();
  virtual ~FirstRunController() {}

  // Overridden methods from ImportObserver.
  virtual void ImportCanceled() {
    FirstRunDone();
  }
  virtual void ImportComplete() {
    FirstRunDone();
  }

  // Display first run UI, start the import and return when it's all over.
  bool DoFirstRun(Profile* profile, ProcessSingleton* process_singleton);

 private:
  // This method closes the first run window and quits the message loop so that
  // the Chrome startup can continue. This should be called when all the
  // first run tasks are done.
  void FirstRunDone();

  scoped_refptr<ImporterHost> importer_host_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunController);
};


bool OpenFirstRunDialog(Profile* profile,
                        bool homepage_defined,
                        int import_items,
                        int dont_import_items,
                        bool search_engine_experiment,
                        bool randomize_search_engine_experiment,
                        ProcessSingleton* process_singleton) {
  FirstRunController* controller = new FirstRunController;
  return controller->DoFirstRun(profile, process_singleton);
}

bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        MasterPrefs* out_prefs) {
  // TODO(jeremy,viettrungluu): http://crbug.com/44901
  NOTIMPLEMENTED();
  return true;
}

FirstRunController::FirstRunController()
    : importer_host_(new ExternalProcessImporterHost) {
}

void FirstRunController::FirstRunDone() {
  // Set preference to show first run bubble and welcome page.
  FirstRun::SetShowFirstRunBubblePref(true);
  FirstRun::SetShowWelcomePagePref();
}

bool FirstRunController::DoFirstRun(Profile* profile,
    ProcessSingleton* process_singleton) {
  // This object is responsible for deleting itself, make sure that happens.
  scoped_ptr<FirstRunController> gc(this);

  // Breakpad should not be enabled on first run until the user has explicitly
  // opted-into stats.
  // TODO: The behavior we probably want here is to enable Breakpad on first run
  // but display a confirmation dialog before sending a crash report so we
  // respect a user's privacy while still getting any crashes that might happen
  // before this point.  Then remove the need for that dialog here.
  DCHECK(!IsCrashReporterEnabled());

  scoped_nsobject<FirstRunDialogController> dialog(
      [[FirstRunDialogController alloc] init]);

  // Set list of browsers we know how to import.
  ssize_t profiles_count = importer_host_->GetAvailableProfileCount();

  // TODO(jeremy): Test on newly created account.
  // TODO(jeremy): Correctly handle case where no browsers to import from
  // are detected.
  NSMutableArray *browsers = [NSMutableArray arrayWithCapacity:profiles_count];
  for (int i = 0; i < profiles_count; ++i) {
    std::wstring profile = importer_host_->GetSourceProfileNameAt(i);
    [browsers addObject:base::SysWideToNSString(profile)];
  }
  [dialog.get() setBrowserImportList:browsers];

  BOOL browser_import_disabled = profiles_count == 0;
  [dialog.get() setBrowserImportListHidden:browser_import_disabled];

  // FirstRunDialogController will call exit if "Cancel" is clicked.
  [dialog.get() showWindow:nil];

  // If user clicked cancel, bail - browser_main will return if we haven't
  // turned off the first run flag when this function returns.
  if ([dialog.get() userDidCancel]) {
    return false;
  }

  // Don't enable stats in Chromium.
  bool stats_enabled = false;
#if defined(GOOGLE_CHROME_BUILD)
  stats_enabled = [dialog.get() statsEnabled] ? true : false;
#endif  // GOOGLE_CHROME_BUILD
  FirstRun::CreateSentinel();
  GoogleUpdateSettings::SetCollectStatsConsent(stats_enabled);

#if defined(GOOGLE_CHROME_BUILD)
  // Breakpad is normally enabled very early in the startup process,
  // however, on the first run it's off by default.  If the user opts-in to
  // stats, enable breakpad.
  if (stats_enabled) {
    InitCrashReporter();
    InitCrashProcessInfo();
  }
#endif  // GOOGLE_CHROME_BUILD

  // If selected set as default browser.
  BOOL make_default_browser = [dialog.get() makeDefaultBrowser];
  if (make_default_browser) {
    bool success = ShellIntegration::SetAsDefaultBrowser();
    DCHECK(success);
  }

  // Import bookmarks.
  if (!browser_import_disabled && [dialog.get() importBookmarks]) {
    const importer::ProfileInfo& source_profile = importer_host_->
        GetSourceProfileInfoAt([dialog.get() browserImportSelectedIndex]);
    int16 items = source_profile.services_supported;
    // TODO(port): Do the actual import in a new process like Windows.
    ignore_result(gc.release());
    StartImportingWithUI(nil, items, importer_host_.get(),
                         source_profile, profile, this, true);
  } else {
    // This is called by the importer if it runs.
    FirstRunDone();
  }

  return true;
}
