// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/network_time/navigation_time_helper.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NavigationTimeHelper);

NavigationTimeHelper::NavigationTimeHelper()
    : web_contents_(NULL) {}

NavigationTimeHelper::NavigationTimeHelper(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  registrar_.Add(this, content::NOTIFICATION_NAV_LIST_PRUNED,
                 content::NotificationService::AllSources());
}

NavigationTimeHelper::~NavigationTimeHelper() {}

base::Time NavigationTimeHelper::GetNavigationTime(
    const content::NavigationEntry* entry) {
  const void* entry_key = entry;
  base::Time local_time = entry->GetTimestamp();

  NavigationTimeCache::iterator iter = time_cache_.find(entry_key);
  if (iter == time_cache_.end() || iter->second.local_time != local_time) {
    // Calculate navigation time for new entry or existing entry that has new
    // navigation.
    base::Time navigation_time = GetNetworkTime(local_time);

    if (iter == time_cache_.end()) {
      time_cache_.insert(std::make_pair(entry_key,
                                        NavigationTimeInfo(local_time,
                                                           navigation_time)));
    } else {
      iter->second = NavigationTimeInfo(local_time, navigation_time);
    }

    return navigation_time;
  } else {
    // Use the navigation time calculated before for unchanged entry.
    return iter->second.network_time;
  }
}

void NavigationTimeHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_NAV_LIST_PRUNED);

  // Drop pruned entries from cache.
  const content::NavigationController& controller =
      web_contents_->GetController();
  NavigationTimeCache new_cache;
  for (int i = 0; i < controller.GetEntryCount(); ++i) {
    const void* entry_key = controller.GetEntryAtIndex(i);
    NavigationTimeCache::const_iterator iter = time_cache_.find(entry_key);
    if (iter != time_cache_.end()) {
      new_cache.insert(std::make_pair(entry_key,  iter->second));
    }
  }
  time_cache_.swap(new_cache);
}

base::Time NavigationTimeHelper::GetNetworkTime(base::Time local_time) {
  // TODO(haitaol): calculate network time based on local_time.
  return local_time;
}
