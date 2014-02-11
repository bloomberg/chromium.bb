// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session.h"

#include "base/stl_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/url_constants.h"

namespace browser_sync {

SyncedSession::SyncedSession() : session_tag("invalid"),
                                 device_type(TYPE_UNSET) {
}

SyncedSession::~SyncedSession() {
  STLDeleteContainerPairSecondPointers(windows.begin(), windows.end());
}

sync_pb::SessionHeader SyncedSession::ToSessionHeader() const {
  sync_pb::SessionHeader header;
  SyncedWindowMap::const_iterator iter;
  for (iter = windows.begin(); iter != windows.end(); ++iter) {
    sync_pb::SessionWindow* w = header.add_window();
    w->CopyFrom(iter->second->ToSyncData());
  }
  header.set_client_name(session_name);
  switch (device_type) {
    case SyncedSession::TYPE_WIN:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_WIN);
      break;
    case SyncedSession::TYPE_MACOSX:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_MAC);
      break;
    case SyncedSession::TYPE_LINUX:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
      break;
    case SyncedSession::TYPE_CHROMEOS:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
      break;
    case SyncedSession::TYPE_PHONE:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
      break;
    case SyncedSession::TYPE_TABLET:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_TABLET);
      break;
    case SyncedSession::TYPE_OTHER:
      // Intentionally fall-through
    default:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_OTHER);
      break;
  }
  return header;
}

// Note: if you modify this, make sure you modify
// SessionModelAssociator::ShouldSyncTab to ensure the logic matches.
bool ShouldSyncSessionTab(const SessionTab& tab) {
  if (tab.navigations.empty())
    return false;
  bool found_valid_url = false;
  for (size_t i = 0; i < tab.navigations.size(); ++i) {
    const GURL& virtual_url = tab.navigations.at(i).virtual_url();
    if (virtual_url.is_valid() &&
        !virtual_url.SchemeIs(content::kChromeUIScheme) &&
        !virtual_url.SchemeIs(chrome::kChromeNativeScheme) &&
        !virtual_url.SchemeIsFile()) {
      found_valid_url = true;
      break;
    }
  }
  return found_valid_url;
}

bool SessionWindowHasNoTabsToSync(const SessionWindow& window) {
  int num_populated = 0;
  for (std::vector<SessionTab*>::const_iterator i = window.tabs.begin();
      i != window.tabs.end(); ++i) {
    const SessionTab* tab = *i;
    if (ShouldSyncSessionTab(*tab))
      num_populated++;
  }
  return (num_populated == 0);
}

}  // namespace browser_sync
