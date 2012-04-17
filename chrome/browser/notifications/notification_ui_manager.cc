// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager.h"

#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification_ui_manager_impl.h"

// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
  return Create(local_state, BalloonCollection::Create());
}

#if !defined(OS_MACOSX)
// static
NotificationUIManager* NotificationUIManager::Create(
    PrefService* local_state,
    BalloonCollection* balloons) {
  NotificationUIManagerImpl* instance =
      new NotificationUIManagerImpl(local_state);
  instance->Initialize(balloons);
  balloons->set_space_change_listener(instance);
  return instance;
}
#endif  // !OS_MACOSX
