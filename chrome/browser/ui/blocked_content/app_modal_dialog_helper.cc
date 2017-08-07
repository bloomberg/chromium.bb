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
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#endif

AppModalDialogHelper::AppModalDialogHelper(content::WebContents* dialog_host)
    : popup_(nullptr) {
  // If a popup is the active window, and the WebContents that is going to be
  // activated is in the opener chain of that popup, then we suspect that
  // WebContents to be trying to create a popunder. Store the popup window so
  // that it can be re-activated once the dialog (or whatever is causing the
  // activation) is closed.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  if (!active_browser || !active_browser->is_type_popup())
    return;

  content::WebContents* active_popup =
      active_browser->tab_strip_model()->GetActiveWebContents();
  if (!active_popup)
    return;

  content::WebContents* actual_host = dialog_host;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the dialog was triggered via an PDF, get the actual web contents that
  // embeds the PDF.
  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromWebContents(dialog_host);
  if (guest)
    actual_host = guest->embedder_web_contents();
#endif

  content::WebContents* original_opener =
      content::WebContents::FromRenderFrameHost(
          active_popup->GetOriginalOpener());
  while (original_opener) {
    if (original_opener == actual_host) {
      // It's indeed a popup from the dialog opening WebContents. Store it, so
      // we can focus it later.
      popup_ = active_popup;
      Observe(popup_);
      return;
    }

    original_opener = content::WebContents::FromRenderFrameHost(
        original_opener->GetOriginalOpener());
  }
}

AppModalDialogHelper::~AppModalDialogHelper() {
  if (!popup_)
    return;

  content::WebContentsDelegate* delegate = popup_->GetDelegate();
  if (!delegate)
    return;

  delegate->ActivateContents(popup_);
}

void AppModalDialogHelper::WebContentsDestroyed() {
  popup_ = nullptr;
}
