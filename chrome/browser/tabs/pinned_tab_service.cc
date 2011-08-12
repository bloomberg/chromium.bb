// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/pinned_tab_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"

static bool IsLastNormalBrowser(Browser* browser) {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (*i != browser && (*i)->is_type_tabbed() &&
        (*i)->profile() == browser->profile()) {
      return false;
    }
  }
  return true;
}

PinnedTabService::PinnedTabService(Profile* profile)
    : profile_(profile),
      got_exiting_(false),
      has_normal_browser_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSING,
                 NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_APP_EXITING,
                 NotificationService::AllSources());
}

void PinnedTabService::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  if (got_exiting_)
    return;

  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = Source<Browser>(source).ptr();
      if (!has_normal_browser_ && browser->is_type_tabbed() &&
          browser->profile() == profile_) {
        has_normal_browser_ = true;
      }
      break;
    }

    case chrome::NOTIFICATION_BROWSER_CLOSING: {
      Browser* browser = Source<Browser>(source).ptr();
      if (has_normal_browser_ && browser->profile() == profile_) {
        if (*(Details<bool>(details)).ptr()) {
          GotExit();
        } else if (IsLastNormalBrowser(browser)) {
          has_normal_browser_ = false;
          PinnedTabCodec::WritePinnedTabs(profile_);
        }
      }
      break;
    }

    case content::NOTIFICATION_APP_EXITING: {
      if (has_normal_browser_)
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
