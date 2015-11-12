// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

GURL GetTargetTabUrl(int session_id, int index) {
  Browser* browser = chrome::FindBrowserWithID(session_id);
  // Sanity checks.
  if (!browser || index >= browser->tab_strip_model()->count())
    return GURL();

  if (index >= 0) {
    content::WebContents* target_tab =
        browser->tab_strip_model()->GetWebContentsAt(index);
    if (target_tab)
      return target_tab->GetURL();
  }

  return GURL();
}

}  // namespace

namespace chrome {

void ShowFeedbackPage(Browser* browser,
                      const std::string& description_template,
                      const std::string& category_tag) {
  GURL page_url;
  if (browser) {
    page_url = GetTargetTabUrl(browser->session_id().id(),
                               browser->tab_strip_model()->active_index());
  }

  Profile* profile = NULL;
  if (browser) {
    profile = browser->profile();
  } else {
    profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  }
  if (!profile) {
    LOG(ERROR) << "Cannot invoke feedback: No profile found!";
    return;
  }

  // We do not want to launch on an OTR profile.
  profile = profile->GetOriginalProfile();
  DCHECK(profile);

#if defined(OS_CHROMEOS)
  // Obtains the display profile ID on which the Feedback window should show.
  chrome::MultiUserWindowManager* const window_manager =
      chrome::MultiUserWindowManager::GetInstance();
  const AccountId display_account_id =
      window_manager && browser
          ? window_manager->GetUserPresentingWindow(
                browser->window()->GetNativeWindow())
          : EmptyAccountId();
  profile = display_account_id.is_valid()
                ? multi_user_util::GetProfileFromAccountId(display_account_id)
                : profile;
#endif

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile);

  api->RequestFeedback(description_template,
                       category_tag,
                       page_url);
}

}  // namespace chrome
