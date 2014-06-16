// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/synced_notifications_private/synced_notifications_private_api.h"

#include "chrome/browser/extensions/api/synced_notifications_private/synced_notifications_shim.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {
namespace api {

namespace {
const char kSyncNotRunningError[] = "Sync Not Running";
const char kDataConversionError[] = "Data Conversion Error";
}  // namespace

SyncedNotificationsPrivateGetInitialDataFunction::
    SyncedNotificationsPrivateGetInitialDataFunction() {
}

SyncedNotificationsPrivateGetInitialDataFunction::
    ~SyncedNotificationsPrivateGetInitialDataFunction() {
}

ExtensionFunction::ResponseAction
SyncedNotificationsPrivateGetInitialDataFunction::Run() {
  scoped_ptr<api::synced_notifications_private::GetInitialData::Params> params(
      api::synced_notifications_private::GetInitialData::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  notifier::ChromeNotifierService* notifier_service =
      notifier::ChromeNotifierServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()),
          Profile::IMPLICIT_ACCESS);
  if (!notifier_service ||
      !notifier_service->GetSyncedNotificationsShim()->IsSyncReady()) {
    return RespondNow(Error(kSyncNotRunningError));
  }

  std::vector<linked_ptr<synced_notifications_private::SyncData> >
      sync_data_list;
  if (!notifier_service->GetSyncedNotificationsShim()->GetInitialData(
          params->type, &sync_data_list)) {
    return RespondNow(Error(kSyncNotRunningError));
  }
  return RespondNow(ArgumentList(
      synced_notifications_private::GetInitialData::Results::Create(
          sync_data_list)));
}

SyncedNotificationsPrivateUpdateNotificationFunction::
    SyncedNotificationsPrivateUpdateNotificationFunction() {
}

SyncedNotificationsPrivateUpdateNotificationFunction::
    ~SyncedNotificationsPrivateUpdateNotificationFunction() {
}

ExtensionFunction::ResponseAction
SyncedNotificationsPrivateUpdateNotificationFunction::Run() {
  scoped_ptr<api::synced_notifications_private::UpdateNotification::Params>
      params(
          api::synced_notifications_private::UpdateNotification::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  notifier::ChromeNotifierService* notifier_service =
      notifier::ChromeNotifierServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()),
          Profile::IMPLICIT_ACCESS);
  if (!notifier_service ||
      !notifier_service->GetSyncedNotificationsShim()->IsSyncReady()) {
    return RespondNow(Error(kSyncNotRunningError));
  }

  if (!notifier_service->GetSyncedNotificationsShim()->UpdateNotification(
          params->changed_notification)) {
    return RespondNow(Error(kDataConversionError));
  }

  return RespondNow(NoArguments());
}

SyncedNotificationsPrivateSetRenderContextFunction::
    SyncedNotificationsPrivateSetRenderContextFunction() {
}

SyncedNotificationsPrivateSetRenderContextFunction::
    ~SyncedNotificationsPrivateSetRenderContextFunction() {
}

ExtensionFunction::ResponseAction
SyncedNotificationsPrivateSetRenderContextFunction::Run() {
  scoped_ptr<api::synced_notifications_private::SetRenderContext::Params>
      params(
          api::synced_notifications_private::SetRenderContext::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  notifier::ChromeNotifierService* notifier_service =
      notifier::ChromeNotifierServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()),
          Profile::IMPLICIT_ACCESS);
  if (!notifier_service ||
      !notifier_service->GetSyncedNotificationsShim()->IsSyncReady()) {
    return RespondNow(Error(kSyncNotRunningError));
  }

  if (!notifier_service->GetSyncedNotificationsShim()->SetRenderContext(
          params->refresh,
          params->data_type_context)) {
    return RespondNow(Error(kDataConversionError));
  }
  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace extensions
