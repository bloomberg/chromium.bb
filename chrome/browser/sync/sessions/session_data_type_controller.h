// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_

#include "components/sync_driver/ui_data_type_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace browser_sync {

// Overrides StartModels to avoid sync contention with sessions during
// a session restore operation at startup.
class SessionDataTypeController : public UIDataTypeController,
                                  public content::NotificationObserver {
 public:
  SessionDataTypeController(SyncApiComponentFactory* factory,
                            Profile* profile,
                            const DisableTypeCallback& disable_callback);

  // NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  virtual ~SessionDataTypeController();
  virtual bool StartModels() OVERRIDE;
  virtual void StopModels() OVERRIDE;

 private:
  Profile* const profile_;
  content::NotificationRegistrar notification_registrar_;
  DISALLOW_COPY_AND_ASSIGN(SessionDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_

