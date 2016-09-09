// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab_helper.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/ui/sad_tab.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SadTabHelper);

namespace {

chrome::SadTabKind SadTabKindFromTerminationStatus(
    base::TerminationStatus status) {
  switch (status) {
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      return chrome::SAD_TAB_KIND_KILLED_BY_OOM;
#endif
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      return chrome::SAD_TAB_KIND_KILLED;
    case base::TERMINATION_STATUS_OOM:
      return chrome::SAD_TAB_KIND_OOM;
    default:
      return chrome::SAD_TAB_KIND_CRASHED;
  }
}

}  // namespace

SadTabHelper::~SadTabHelper() {
}

SadTabHelper::SadTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

void SadTabHelper::RenderViewReady() {
  sad_tab_.reset();
}

void SadTabHelper::RenderProcessGone(base::TerminationStatus status) {
  // Only show the sad tab if we're not in browser shutdown, so that WebContents
  // objects that are not in a browser (e.g., HTML dialogs) and thus are
  // visible do not flash a sad tab page.
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  if (sad_tab_)
    return;

  if (chrome::SadTab::ShouldShow(status))
    InstallSadTab(status);
}

void SadTabHelper::InstallSadTab(base::TerminationStatus status) {
  sad_tab_.reset(chrome::SadTab::Create(
      web_contents(), SadTabKindFromTerminationStatus(status)));
}
