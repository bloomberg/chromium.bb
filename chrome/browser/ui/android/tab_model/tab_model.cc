// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model.h"

#include "base/logging.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/browser/sync/glue/synced_window_delegate_android.h"
using browser_sync::SyncedWindowDelegate;
using browser_sync::SyncedWindowDelegateAndroid;


using content::NotificationService;

TabModel::TabModel() {
}

TabModel::~TabModel() {
}

void TabModel::BroadcastSessionRestoreComplete() {
  if (GetProfile()) {
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_SESSION_RESTORE_COMPLETE,
        content::Source<Profile>(GetProfile()),
        NotificationService::NoDetails());
  } else {
    NOTREACHED();
  }
}
