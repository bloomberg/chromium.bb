// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_popup_view.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

AutofillPopupView::AutofillPopupView(content::WebContents* web_contents) {
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_HIDDEN,
                 content::Source<content::WebContents>(web_contents));
  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &(web_contents->GetController())));
}

AutofillPopupView::~AutofillPopupView() {}

void AutofillPopupView::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_WEB_CONTENTS_HIDDEN
      || type == content::NOTIFICATION_NAV_ENTRY_COMMITTED)
    Hide();
}
