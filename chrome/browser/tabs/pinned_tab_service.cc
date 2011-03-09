// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/pinned_tab_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/browser.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

PinnedTabService::PinnedTabService(Profile* profile)
    : profile_(profile),
      got_exiting_(false) {
  registrar_.Add(this, NotificationType::BROWSER_CLOSING,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::APP_EXITING,
                 NotificationService::AllSources());
}

void PinnedTabService::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  if (got_exiting_)
    return;

  switch (type.value) {
    case NotificationType::BROWSER_CLOSING: {
      Browser* browser = Source<Browser>(source).ptr();
      if (browser->profile() == profile_ && *(Details<bool>(details)).ptr())
        GotExit();
      break;
    }

    case NotificationType::APP_EXITING: {
      GotExit();
      break;
    }

    default:
      NOTREACHED();
  }
}

void PinnedTabService::GotExit() {
  DCHECK(!got_exiting_);
  got_exiting_ = true;
  PinnedTabCodec::WritePinnedTabs(profile_);
}
