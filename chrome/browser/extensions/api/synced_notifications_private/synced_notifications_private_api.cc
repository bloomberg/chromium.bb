// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/synced_notifications_private/synced_notifications_private_api.h"

namespace extensions {
namespace api {

SyncedNotificationsPrivateGetInitialDataFunction::
    SyncedNotificationsPrivateGetInitialDataFunction() {
}

SyncedNotificationsPrivateGetInitialDataFunction::
    ~SyncedNotificationsPrivateGetInitialDataFunction() {
}

bool SyncedNotificationsPrivateGetInitialDataFunction::RunSync() {
  scoped_ptr<api::synced_notifications_private::GetInitialData::Params> params(
      api::synced_notifications_private::GetInitialData::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SetError("Not Implemented");
  return false;
}

SyncedNotificationsPrivateUpdateNotificationFunction::
    SyncedNotificationsPrivateUpdateNotificationFunction() {
}

SyncedNotificationsPrivateUpdateNotificationFunction::
    ~SyncedNotificationsPrivateUpdateNotificationFunction() {
}

bool SyncedNotificationsPrivateUpdateNotificationFunction::RunSync() {
  scoped_ptr<api::synced_notifications_private::UpdateNotification::Params>
      params(
          api::synced_notifications_private::UpdateNotification::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SetError("Not Implemented");
  return false;
}

SyncedNotificationsPrivateSetRenderContextFunction::
    SyncedNotificationsPrivateSetRenderContextFunction() {
}

SyncedNotificationsPrivateSetRenderContextFunction::
    ~SyncedNotificationsPrivateSetRenderContextFunction() {
}

bool SyncedNotificationsPrivateSetRenderContextFunction::RunSync() {
  scoped_ptr<api::synced_notifications_private::SetRenderContext::Params>
      params(
          api::synced_notifications_private::SetRenderContext::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SetError("Not Implemented");
  return false;
}

}  // namespace api
}  // namespace extensions
