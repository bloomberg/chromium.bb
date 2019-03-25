// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace send_tab_to_self {

void CreateNewEntry(content::WebContents* tab, Profile* profile) {
  content::NavigationEntry* navigation_entry =
      tab->GetController().GetLastCommittedEntry();

  GURL url = navigation_entry->GetURL();
  std::string title = base::UTF16ToUTF8(navigation_entry->GetTitle());
  base::Time navigation_time = navigation_entry->GetTimestamp();

  const SendTabToSelfEntry* entry =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel()
          ->AddEntry(url, title, navigation_time);

  if (entry) {
    DesktopNotificationHandler(profile).DisplaySendingConfirmation(entry);
  } else {
    DesktopNotificationHandler(profile).DisplayFailureMessage();
  }
}

}  // namespace send_tab_to_self
