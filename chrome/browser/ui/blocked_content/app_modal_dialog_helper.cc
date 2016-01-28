// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/app_modal_dialog_helper.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

AppModalDialogHelper::AppModalDialogHelper(content::WebContents* dialog_host)
    : popup_(nullptr) {
  // If the WebContents that triggered this dialog is not currently focused, we
  // want to store a potential popup here to restore it after the dialog was
  // closed.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  if (active_browser) {
    content::WebContents* active_web_contents =
        active_browser->tab_strip_model()->GetActiveWebContents();
    if (active_browser->is_type_popup() && active_web_contents &&
        active_web_contents->GetOpener() == dialog_host) {
      // It's indeed a popup from the dialog opening WebContents. Store it, so
      // we can focus it later.
      popup_ = active_web_contents;
      Observe(popup_);
    }
  }
}

AppModalDialogHelper::~AppModalDialogHelper() {
  if (popup_)
    popup_->GetDelegate()->ActivateContents(popup_);
}

void AppModalDialogHelper::WebContentsDestroyed() {
  popup_ = nullptr;
}
