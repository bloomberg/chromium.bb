// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/first_run_dialog.h"

#include "base/compiler_specific.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/cocoa/first_run_dialog_controller.h"
#include "chrome/common/url_constants.h"
#include "components/search_engines/template_url_service.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

@interface FirstRunDialogController (PrivateMethods)
// Show the dialog.
- (void)show;
@end

namespace {

class FirstRunShowBridge : public base::RefCounted<FirstRunShowBridge> {
 public:
  FirstRunShowBridge(FirstRunDialogController* controller);

  void ShowDialog();

 private:
  friend class base::RefCounted<FirstRunShowBridge>;

  ~FirstRunShowBridge();

  FirstRunDialogController* controller_;
};

FirstRunShowBridge::FirstRunShowBridge(
    FirstRunDialogController* controller) : controller_(controller) {
}

void FirstRunShowBridge::ShowDialog() {
  [controller_ show];
  base::MessageLoop::current()->QuitNow();
}

FirstRunShowBridge::~FirstRunShowBridge() {}

// Show the first run UI.
// Returns true if the first run dialog was shown.
bool ShowFirstRunModal(Profile* profile) {
  // The purpose of the dialog is to ask the user to enable stats and crash
  // reporting. This setting may be controlled through configuration management
  // in enterprise scenarios. If that is the case, skip the dialog entirely, as
  // it's not worth bothering the user for only the default browser question
  // (which is likely to be forced in enterprise deployments anyway).
  if (IsMetricsReportingPolicyManaged())
    return false;
  base::scoped_nsobject<FirstRunDialogController> dialog(
      [[FirstRunDialogController alloc] init]);

  [dialog.get() showWindow:nil];

  // If the dialog asked the user to opt-in for stats and crash reporting,
  // record the decision and enable the crash reporter if appropriate.
  bool consent_given = [dialog.get() isStatsReportingEnabled];
  ChangeMetricsReportingState(consent_given);

  // If selected set as default browser.
  BOOL make_default_browser = [dialog.get() isMakeDefaultBrowserEnabled];
  if (make_default_browser) {
    bool success = shell_integration::SetAsDefaultBrowser();
    DCHECK(success);
  }

  return true;
}

// True when the stats checkbox should be checked by default. This is only
// the case when the canary is running.
bool StatsCheckboxDefault() {
  // Opt-in means the checkbox is unchecked by default.
  return !first_run::IsMetricsReportingOptIn();
}

}  // namespace

namespace first_run {

bool ShowFirstRunDialog(Profile* profile) {
  bool dialog_shown = false;
#if defined(GOOGLE_CHROME_BUILD)
  dialog_shown = ShowFirstRunModal(profile);
#else
  (void)ShowFirstRunModal;  // Placate compiler.
#endif
  // Set preference to show first run bubble and welcome page.
  // Only display the bubble if there is a default search provider.
  TemplateURLService* search_engines_model =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (search_engines_model &&
      search_engines_model->GetDefaultSearchProvider()) {
    first_run::SetShowFirstRunBubblePref(first_run::FIRST_RUN_BUBBLE_SHOW);
  }
  first_run::SetShouldShowWelcomePage();

  return dialog_shown;
}

}  // namespace first_run

@implementation FirstRunDialogController {
  base::scoped_nsobject<FirstRunDialogViewController> viewController_;
}

- (instancetype)init {
  viewController_.reset([[FirstRunDialogViewController alloc]
      initWithStatsCheckboxInitiallyChecked:StatsCheckboxDefault()
              defaultBrowserCheckboxVisible:shell_integration::
                                                CanSetAsDefaultBrowser()]);

  // Create the content view controller (and the content view) *before* the
  // window, so that we can find out what the content view's frame is supposed
  // to be for use here.
  base::scoped_nsobject<NSWindow> window([[NSWindow alloc]
      initWithContentRect:[[viewController_ view] frame]
                styleMask:NSTitledWindowMask
                  backing:NSBackingStoreBuffered
                    defer:YES]);
  [window setContentView:[viewController_ view]];
  [window setTitle:[viewController_ windowTitle]];

  self = [super initWithWindow:window.get()];

  return self;
}

- (void)dealloc {
  [super dealloc];
}

- (IBAction)showWindow:(id)sender {
  // The main MessageLoop has not yet run, but has been spun. If we call
  // -[NSApplication runModalForWindow:] we will hang <http://crbug.com/54248>.
  // Therefore the main MessageLoop is run so things work.

  scoped_refptr<FirstRunShowBridge> bridge(new FirstRunShowBridge(self));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FirstRunShowBridge::ShowDialog, bridge.get()));
  base::RunLoop().Run();
}

- (void)show {
  NSWindow* win = [self window];

  // Neat weirdness in the below code - the Application menu stays enabled
  // while the window is open but selecting items from it (e.g. Quit) has
  // no effect.  I'm guessing that this is an artifact of us being a
  // background-only application at this stage and displaying a modal
  // window.

  // Display dialog.
  [win center];
  [NSApp runModalForWindow:win];
}

- (BOOL)isStatsReportingEnabled {
  return [viewController_ isStatsReportingEnabled];
}

- (BOOL)isMakeDefaultBrowserEnabled {
  return [viewController_ isMakeDefaultBrowserEnabled];
}

@end
