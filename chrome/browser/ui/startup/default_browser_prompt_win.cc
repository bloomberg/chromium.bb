// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt.h"

#include "base/macros.h"
#include "base/win/windows_version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/set_as_default_browser_ui.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

using content::BrowserThread;

namespace {

// Show the page prompting the user to make Chrome the default browser on
// Windows 8 (which means becoming "the browser" in Metro mode). The page
// will be shown at the first appropriate opportunity. It can be placed in
// a tab or in a dialog, depending on other settings.
class SetMetroBrowserFlowLauncher : public content::NotificationObserver {
 public:
  static void LaunchSoon(Profile* profile) {
    // The instance will manage its own lifetime.
    new SetMetroBrowserFlowLauncher(profile);
  }

 private:
  explicit SetMetroBrowserFlowLauncher(Profile* profile)
      : profile_(profile) {
    registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                   content::NotificationService::AllSources());
  }

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar registrar_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SetMetroBrowserFlowLauncher);
};

void SetMetroBrowserFlowLauncher::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME);
  Browser* browser = chrome::FindBrowserWithWebContents(
      content::Source<content::WebContents>(source).ptr());

  if (!browser || !browser->is_type_tabbed())
    return;

  // Unregister and delete.
  registrar_.RemoveAll();
  SetAsDefaultBrowserUI::Show(profile_, browser);
  delete this;
}

}  // namespace

namespace chrome {

bool ShowFirstRunDefaultBrowserPrompt(Profile* profile) {
  // The behavior on Windows 10 is no good at the moment, since there is no
  // known way to lead the user directly to a default browser picker.
  if (base::win::GetVersion() >= base::win::VERSION_WIN10)
    return false;

  // If the only available mode of setting the default browser requires
  // user interaction, it means this couldn't have been done yet. Therefore,
  // we launch the dialog and inform the caller of it.
  bool show_status =
      (ShellIntegration::CanSetAsDefaultBrowser() ==
       ShellIntegration::SET_DEFAULT_INTERACTIVE) &&
      (ShellIntegration::GetDefaultBrowser() == ShellIntegration::NOT_DEFAULT);

  if (show_status) {
    startup_metric_utils::SetNonBrowserUIDisplayed();
    SetMetroBrowserFlowLauncher::LaunchSoon(profile);
  }

  return show_status;
}

}  // namespace chrome
