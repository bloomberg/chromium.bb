// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYNCED_NOTIFICATIONS_PRIVATE_SYNCED_NOTIFICATIONS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYNCED_NOTIFICATIONS_PRIVATE_SYNCED_NOTIFICATIONS_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/synced_notifications_private.h"

namespace extensions {
namespace api {

class SyncedNotificationsPrivateGetInitialDataFunction
    : public UIThreadExtensionFunction {
 public:
  SyncedNotificationsPrivateGetInitialDataFunction();
  DECLARE_EXTENSION_FUNCTION("syncedNotificationsPrivate.getInitialData",
                             SYNCEDNOTIFICATIONSPRIVATE_GETINITIALDATA);

 protected:
  virtual ~SyncedNotificationsPrivateGetInitialDataFunction();
  virtual ResponseAction Run() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedNotificationsPrivateGetInitialDataFunction);
};

class SyncedNotificationsPrivateUpdateNotificationFunction
    : public UIThreadExtensionFunction {
 public:
  SyncedNotificationsPrivateUpdateNotificationFunction();
  DECLARE_EXTENSION_FUNCTION("syncedNotificationsPrivate.updateNotification",
                             SYNCEDNOTIFICATIONSPRIVATE_UPDATENOTIFICATION);

 protected:
  virtual ~SyncedNotificationsPrivateUpdateNotificationFunction();
  virtual ResponseAction Run() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(
      SyncedNotificationsPrivateUpdateNotificationFunction);
};

class SyncedNotificationsPrivateSetRenderContextFunction
    : public UIThreadExtensionFunction {
 public:
  SyncedNotificationsPrivateSetRenderContextFunction();
  DECLARE_EXTENSION_FUNCTION("syncedNotificationsPrivate.setRenderContext",
                             SYNCEDNOTIFICATIONSPRIVATE_SETRENDERCONTEXT);

 protected:
  virtual ~SyncedNotificationsPrivateSetRenderContextFunction();
  virtual ResponseAction Run() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedNotificationsPrivateSetRenderContextFunction);
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYNCED_NOTIFICATIONS_PRIVATE_SYNCED_NOTIFICATIONS_PRIVATE_API_H_
