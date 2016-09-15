// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/md_feedback/md_feedback_dialog_controller.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

void ShowFeedbackPage(Browser* browser,
                      const std::string& description_template,
                      const std::string& category_tag) {
  GURL page_url;
  if (browser) {
    page_url = GetTargetTabUrl(browser->session_id().id(),
                               browser->tab_strip_model()->active_index());
  }

  Profile* profile = GetFeedbackProfile(browser);
  if (!profile) {
    LOG(ERROR) << "Cannot invoke feedback: No profile found!";
    return;
  }

  if (::switches::MdFeedbackEnabled()) {
    MdFeedbackDialogController::GetInstance()->Show(profile);
    return;
  }

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile);

  api->RequestFeedback(description_template,
                       category_tag,
                       page_url);
}

}  // namespace chrome
