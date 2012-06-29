// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_select_file_policy.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ChromeSelectFilePolicy::ChromeSelectFilePolicy(
    content::WebContents* source_contents)
    : source_contents_(source_contents) {
}

ChromeSelectFilePolicy::~ChromeSelectFilePolicy() {}

bool ChromeSelectFilePolicy::CanOpenSelectFileDialog() {
  DCHECK(g_browser_process);

  // local_state() can return NULL for tests.
  if (!g_browser_process->local_state())
    return false;

  return !g_browser_process->local_state()->FindPreference(
             prefs::kAllowFileSelectionDialogs) ||
         g_browser_process->local_state()->GetBoolean(
             prefs::kAllowFileSelectionDialogs);
}

void ChromeSelectFilePolicy::SelectFileDenied() {
  // Show the InfoBar saying that file-selection dialogs are disabled.
  if (source_contents_) {
    TabContents* tab_contents = TabContents::FromWebContents(source_contents_);
    DCHECK(tab_contents);
    InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
    infobar_helper->AddInfoBar(new SimpleAlertInfoBarDelegate(
        infobar_helper,
        NULL,
        l10n_util::GetStringUTF16(IDS_FILE_SELECTION_DIALOG_INFOBAR),
        true));
  } else {
    LOG(WARNING) << "File-selection dialogs are disabled but no WebContents "
                 << "is given to display the InfoBar.";
  }
}
