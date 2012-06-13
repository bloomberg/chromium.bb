// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_dialog_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using ui::WebDialogDelegate;

WebDialogController::WebDialogController(
    WebDialogDelegate* delegate,
    content::BrowserContext* context,
    Browser* browser)
      : dialog_delegate_(delegate) {
  // It's only safe to show an off the record profile under one of two
  // circumstances:
  // 1. For a modal dialog where the parent will maintain the profile.
  // 2. If we have a browser which will keep the reference to this profile
  //    alive. The dialog will be closed if this browser is closed.
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord() ||
         delegate->GetDialogModalType() != ui::MODAL_TYPE_NONE ||
         (browser && browser->profile() == profile));
  // If we're passed a browser it should own the profile we're using.
  DCHECK(!browser || browser->profile() == profile);
  if (browser) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BROWSER_CLOSING,
                   content::Source<Browser>(browser));
  }
}

// content::NotificationObserver implementation:
void WebDialogController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_CLOSING);

  // If the browser creating this dialog is closed, close the dialog to prevent
  // using potentially destroyed profiles.
  dialog_delegate_->OnDialogClosed(std::string());
}
