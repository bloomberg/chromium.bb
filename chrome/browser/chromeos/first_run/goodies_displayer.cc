// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/goodies_displayer.h"

#include "base/files/file_util.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace first_run {

namespace {

// ChromeOS Goodies page for device's first New Window.
const char kGoodiesURL[] = "https://www.google.com/chrome/devices/goodies.html";

// Max days after initial login that we're willing to show Goodies.
static const int kMaxDaysAfterOobeForGoodies = 14;

// Checks timestamp on OOBE Complete flag file.  kCanShowOobeGoodiesPage
// defaults to |true|; we set it to |false| (return |false|) for any device over
// kMaxDaysAfterOobeForGoodies days old, to avoid showing it after update on
// older devices.
bool CheckGoodiesPrefAgainstOobeTimestamp() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  const base::FilePath oobe_timestamp_file =
      StartupUtils::GetOobeCompleteFlagPath();
  base::File::Info fileInfo;
  if (base::GetFileInfo(oobe_timestamp_file, &fileInfo)) {
    const base::TimeDelta time_since_oobe =
        base::Time::Now() - fileInfo.creation_time;
    if (time_since_oobe >
        base::TimeDelta::FromDays(kMaxDaysAfterOobeForGoodies))
      return false;
  }
  return true;
}

// Callback into main thread to set pref to |false| if too long since oobe, or
// to create GoodiesDisplayer otherwise.
void UpdateGoodiesPrefCantShow(bool can_show_goodies) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (can_show_goodies) {
    UserSessionManager::GetInstance()->CreateGoodiesDisplayer();
  } else {
    g_browser_process->local_state()->SetBoolean(prefs::kCanShowOobeGoodiesPage,
                                                 false);
  }
}

}  // namespace

GoodiesDisplayer::GoodiesDisplayer() {
  BrowserList::AddObserver(this);
}

// If Goodies page hasn't been shown yet, and Chromebook isn't too old, create
// GoodiesDisplayer to observe BrowserList.
void GoodiesDisplayer::Init() {
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kCanShowOobeGoodiesPage))
    base::PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool(), FROM_HERE,
        base::Bind(&CheckGoodiesPrefAgainstOobeTimestamp),
        base::Bind(&UpdateGoodiesPrefCantShow));
}

// If conditions enumerated below are met, this loads the Oobe Goodies page for
// new Chromebooks; when appropriate, it uses pref to mark page as shown,
// removes itself from BrowserListObservers, and deletes itself.
void GoodiesDisplayer::OnBrowserSetLastActive(Browser* browser) {
  // 1. Not guest or incognito session (keep observing).
  if (browser->profile()->IsOffTheRecord())
    return;

  PrefService* local_state = g_browser_process->local_state();
  // 2. Not previously shown, or otherwise marked as unavailable.
  if (local_state->GetBoolean(prefs::kCanShowOobeGoodiesPage)) {
    // 3. Device not enterprise enrolled.
    if (!g_browser_process->platform_part()
             ->browser_policy_connector_chromeos()
             ->IsEnterpriseManaged())
      chrome::AddTabAt(browser, GURL(kGoodiesURL), 2, false);

    // Set to |false| whether enterprise enrolled or Goodies shown.
    local_state->SetBoolean(prefs::kCanShowOobeGoodiesPage, false);
  }

  // Regardless of how we got here, we don't henceforth need to show Goodies.
  BrowserList::RemoveObserver(this);
  UserSessionManager::GetInstance()->DestroyGoodiesDisplayer();
}

}  // namespace first_run
}  // namespace chromeos
