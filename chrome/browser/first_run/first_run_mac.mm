// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#import "base/scoped_nsobject.h"
#import "chrome/app/breakpad_mac.h"
#import "chrome/browser/cocoa/first_run_dialog.h"
#import "chrome/browser/cocoa/search_engine_dialog_controller.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"

namespace {

// Show the search engine selection dialog.
void ShowSearchEngineSelectionDialog(Profile* profile,
                                     bool randomize_search_engine_experiment) {
  scoped_nsobject<SearchEngineDialogController> dialog(
      [[SearchEngineDialogController alloc] init]);
  [dialog.get() setProfile:profile];
  [dialog.get() setRandomize:randomize_search_engine_experiment];

  [dialog.get() showWindow:nil];
}

// Show the first run UI.
void ShowFirstRun(Profile* profile) {
#if defined(GOOGLE_CHROME_BUILD)
  // The purpose of the dialog is to ask the user to enable stats and crash
  // reporting. This setting may be controlled through configuration management
  // in enterprise scenarios. If that is the case, skip the dialog entirely, as
  // it's not worth bothering the user for only the default browser question
  // (which is likely to be forced in enterprise deployments anyway).
  const PrefService::Preference* metrics_reporting_pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kMetricsReportingEnabled);
  if (!metrics_reporting_pref || !metrics_reporting_pref->IsManaged()) {
    scoped_nsobject<FirstRunDialogController> dialog(
        [[FirstRunDialogController alloc] init]);

    [dialog.get() showWindow:nil];

    // If the dialog asked the user to opt-in for stats and crash reporting,
    // record the decision and enable the crash reporter if appropriate.
    bool stats_enabled = [dialog.get() statsEnabled];
    GoogleUpdateSettings::SetCollectStatsConsent(stats_enabled);

    // Breakpad is normally enabled very early in the startup process.  However,
    // on the first run it may not have been enabled due to the missing opt-in
    // from the user.  If the user agreed now, enable breakpad if necessary.
    if (!IsCrashReporterEnabled() && stats_enabled) {
      InitCrashReporter();
      InitCrashProcessInfo();
    }

    // If selected set as default browser.
    BOOL make_default_browser = [dialog.get() makeDefaultBrowser];
    if (make_default_browser) {
      bool success = ShellIntegration::SetAsDefaultBrowser();
      DCHECK(success);
    }
  }
#else  // GOOGLE_CHROME_BUILD
  // We don't show the dialog in Chromium.
#endif  // GOOGLE_CHROME_BUILD

  FirstRun::CreateSentinel();

  // Set preference to show first run bubble and welcome page.
  // Don't display the minimal bubble if there is no default search provider.
  TemplateURLModel* search_engines_model = profile->GetTemplateURLModel();
  if (search_engines_model &&
      search_engines_model->GetDefaultSearchProvider()) {
    FirstRun::SetShowFirstRunBubblePref(true);
  }
  FirstRun::SetShowWelcomePagePref();
}

}  // namespace

// static
void FirstRun::ShowFirstRunDialog(Profile* profile,
                                  bool randomize_search_engine_experiment) {
  // If the default search is not managed via policy, ask the user to
  // choose a default.
  TemplateURLModel* model = profile->GetTemplateURLModel();
  if (model && !model->is_default_search_managed()) {
    ShowSearchEngineSelectionDialog(profile,
                                    randomize_search_engine_experiment);
  }
  ShowFirstRun(profile);
}

bool FirstRun::ImportBookmarks(const FilePath& import_bookmarks_path) {
  // http://crbug.com/48880
  return false;
}

// static
bool FirstRun::IsOrganic() {
  // We treat all installs as organic.
  return true;
}

// static
void FirstRun::PlatformSetup() {
  // Things that Windows does here (creating a desktop icon, for example) are
  // not needed.
}
