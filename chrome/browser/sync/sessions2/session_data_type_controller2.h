// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS2_SESSION_DATA_TYPE_CONTROLLER2_H_
#define CHROME_BROWSER_SYNC_SESSIONS2_SESSION_DATA_TYPE_CONTROLLER2_H_

#include "chrome/browser/sync/glue/ui_data_type_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace browser_sync {

// Overrides StartModels to avoid sync contention with sessions during
// a session restore operation at startup.
class SessionDataTypeController2 : public UIDataTypeController,
                                   public content::NotificationObserver {
 public:
  SessionDataTypeController2(ProfileSyncComponentsFactory* factory,
                             Profile* profile,
                             ProfileSyncService* service);

  // NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  virtual ~SessionDataTypeController2();
  virtual bool StartModels() OVERRIDE;
  virtual void StopModels() OVERRIDE;

 private:
  content::NotificationRegistrar notification_registrar_;
  DISALLOW_COPY_AND_ASSIGN(SessionDataTypeController2);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS2_SESSION_DATA_TYPE_CONTROLLER2_H_

