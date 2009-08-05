// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/app/breakpad_mac.h"
#import "chrome/browser/cocoa/first_run_dialog.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"

// static
bool FirstRun::IsChromeFirstRun() {
#if defined(GOOGLE_CHROME_BUILD)
  // Use presence of kRegUsageStatsField key as an indicator of whether or not
  // this is the first run.
  // See chrome/browser/google_update_settings_mac.mm for details on why we use
  // the defualts dictionary here.
  NSUserDefaults* std_defaults = [NSUserDefaults standardUserDefaults];
  NSDictionary* defaults_dict = [std_defaults dictionaryRepresentation];
  NSString* collect_stats_key = base::SysWideToNSString(
      google_update::kRegUsageStatsField);

  bool not_in_dict = [defaults_dict objectForKey:collect_stats_key] == nil;
  return not_in_dict;
#else
  return false; // no first run UI for Chromium builds
#endif  // defined(GOOGLE_CHROME_BUILD)
}

bool OpenFirstRunDialog(Profile* profile, ProcessSingleton* process_singleton) {
// OpenFirstRunDialog is a no-op on non-branded builds.
#if defined(GOOGLE_CHROME_BUILD)
  // Breakpad should not be enabled on first run until the user has explicitly
  // opted-into stats.
  // TODO: The behavior we probably want here is to enable Breakpad on first run
  // but display a confirmation dialog before sending a crash report so we
  // respect a user's privacy while still getting any crashes that might happen
  // before this point.  Then remove the need for that dialog here.
  DCHECK(IsCrashReporterDisabled());

  scoped_nsobject<FirstRunDialogController> dialog(
      [[FirstRunDialogController alloc] init]);

  scoped_refptr<ImporterHost> importer_host(new ImporterHost());

  // Set list of browsers we know how to import.
  ssize_t profiles_count = importer_host->GetAvailableProfileCount();

  // TODO(jeremy): Test on newly created account.
  // TODO(jeremy): Correctly handle case where no browsers to import from
  // are detected.
  NSMutableArray *browsers = [NSMutableArray arrayWithCapacity:profiles_count];
  for (int i = 0; i < profiles_count; ++i) {
    std::wstring profile = importer_host->GetSourceProfileNameAt(i);
    [browsers addObject:base::SysWideToNSString(profile)];
  }
  [dialog.get() setBrowserImportList:browsers];

  // FirstRunDialogController will call exit if "Cancel" is clicked.
  [dialog.get() showWindow:nil];

  // If user clicked cancel, bail - browser_main will return if we haven't
  // turned off the first run flag when this function returns.
  if ([dialog.get() userDidCancel]) {
    return false;
  }

  BOOL stats_enabled = [dialog.get() statsEnabled];

  // Breakpad is normally enabled very early in the startup process,
  // however, on the first run it's off by default.  If the user opts-in to
  // stats, enable breakpad.
  if (stats_enabled) {
    InitCrashReporter();
    InitCrashProcessInfo();
  }

  GoogleUpdateSettings::SetCollectStatsConsent(stats_enabled);

  // If selected set as default browser.
  BOOL make_default_browser = [dialog.get() makeDefaultBrowser];
  if (make_default_browser) {
    bool success = ShellIntegration::SetAsDefaultBrowser();
    DCHECK(success);
  }

  // Import bookmarks.
  if ([dialog.get() importBookmarks]) {
    const ProfileInfo& source_profile = importer_host->GetSourceProfileInfoAt(
        [dialog.get() browserImportSelectedIndex]);
    int items = SEARCH_ENGINES + HISTORY + FAVORITES + HOME_PAGE + PASSWORDS;
    // TODO(port): Call StartImportingWithUI here instead and launch
    // a new process that does the actual import.
    importer_host->StartImportSettings(source_profile, profile, items,
                                       new ProfileWriter(profile), true);
  }

#endif  // defined(GOOGLE_CHROME_BUILD)
  return true;
}
