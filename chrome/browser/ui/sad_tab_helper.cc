// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab_helper.h"

#include "base/logging.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/ui/sad_tab.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SadTabHelper);

SadTabHelper::~SadTabHelper() {
}

SadTabHelper::SadTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::Source<content::WebContents>(web_contents));
}

void SadTabHelper::RenderViewGone(base::TerminationStatus status) {
  // Only show the sad tab if we're not in browser shutdown, so that WebContents
  // objects that are not in a browser (e.g., HTML dialogs) and thus are
  // visible do not flash a sad tab page.
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  if (sad_tab_)
    return;

  if (status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
      status == base::TERMINATION_STATUS_PROCESS_CRASHED)
    InstallSadTab(status);
}

void SadTabHelper::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_CONNECTED, type);
  if (sad_tab_) {
    sad_tab_->Close();
    sad_tab_.reset();
  }
}

void SadTabHelper::InstallSadTab(base::TerminationStatus status) {
  chrome::SadTabKind kind =
      (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED) ?
          chrome::SAD_TAB_KIND_KILLED : chrome::SAD_TAB_KIND_CRASHED;
  sad_tab_.reset(chrome::SadTab::Create(web_contents(), kind));
  sad_tab_->Show();
}
