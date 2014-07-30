// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/state_store_notification_observer.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/state_store.h"

namespace extensions {

StateStoreNotificationObserver::StateStoreNotificationObserver(
    StateStore* state_store)
    : state_store_(state_store) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_SESSION_RESTORE_DONE,
                 content::NotificationService::AllBrowserContextsAndSources());
}

StateStoreNotificationObserver::~StateStoreNotificationObserver() {
}

void StateStoreNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_SESSION_RESTORE_DONE);
  registrar_.RemoveAll();
  state_store_->RequestInitAfterDelay();
}

}  // namespace extensions
