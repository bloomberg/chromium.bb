// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/html_dialog_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

HtmlDialogController::HtmlDialogController(HtmlDialogUIDelegate* delegate,
                                           Profile* profile,
                                           Browser* browser)
      : dialog_delegate_(delegate) {
  // It's only safe to show an off the record profile if we have a browser which
  // has this profile to maintain its existence.
  DCHECK(!profile->IsOffTheRecord() || (browser &&
                                        browser->profile() == profile));
  // If we're passed a browser it should own the profile we're using.
  DCHECK(!browser || browser->profile() == profile);
  if (browser) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BROWSER_CLOSING,
                   content::Source<Browser>(browser));
  }
}

// content::NotificationObserver implementation:
void HtmlDialogController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_CLOSING);

  // If the browser creating this dialog is closed, close the dialog to prevent
  // using potentially destroyed profiles.
  dialog_delegate_->OnDialogClosed(std::string());
}
