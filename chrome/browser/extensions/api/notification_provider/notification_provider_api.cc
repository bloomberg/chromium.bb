// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notification_provider/notification_provider_api.h"

#include "base/callback.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/common/chrome_version_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "ui/base/layout.h"
#include "url/gurl.h"

namespace extensions {

NotificationProviderEventRouter::NotificationProviderEventRouter(
    Profile* profile)
    : profile_(profile) {
}

NotificationProviderEventRouter::~NotificationProviderEventRouter() {
}

void NotificationProviderEventRouter::CreateNotification(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id,
    const api::notifications::NotificationOptions& options) {
  Create(notification_provider_id, sender_id, notification_id, options);
}

void NotificationProviderEventRouter::UpdateNotification(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id,
    const api::notifications::NotificationOptions& options) {
  Update(notification_provider_id, sender_id, notification_id, options);
}
void NotificationProviderEventRouter::ClearNotification(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id) {
  Clear(notification_provider_id, sender_id, notification_id);
}

void NotificationProviderEventRouter::Create(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id,
    const api::notifications::NotificationOptions& options) {
  scoped_ptr<base::ListValue> args =
      api::notification_provider::OnCreated::Create(
          sender_id, notification_id, options);

  scoped_ptr<Event> event(new Event(
      api::notification_provider::OnCreated::kEventName, args.Pass()));

  EventRouter::Get(profile_)
      ->DispatchEventToExtension(notification_provider_id, event.Pass());
}

void NotificationProviderEventRouter::Update(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id,
    const api::notifications::NotificationOptions& options) {
  scoped_ptr<base::ListValue> args =
      api::notification_provider::OnUpdated::Create(
          sender_id, notification_id, options);

  scoped_ptr<Event> event(new Event(
      api::notification_provider::OnUpdated::kEventName, args.Pass()));

  EventRouter::Get(profile_)
      ->DispatchEventToExtension(notification_provider_id, event.Pass());
}

void NotificationProviderEventRouter::Clear(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id) {
  scoped_ptr<base::ListValue> args =
      api::notification_provider::OnCleared::Create(sender_id, notification_id);

  scoped_ptr<Event> event(new Event(
      api::notification_provider::OnCleared::kEventName, args.Pass()));

  EventRouter::Get(profile_)
      ->DispatchEventToExtension(notification_provider_id, event.Pass());
}

NotificationProviderNotifyOnClearedFunction::
    NotificationProviderNotifyOnClearedFunction() {
}

NotificationProviderNotifyOnClearedFunction::
    ~NotificationProviderNotifyOnClearedFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnClearedFunction::Run() {
  scoped_ptr<api::notification_provider::NotifyOnCleared::Params> params =
      api::notification_provider::NotifyOnCleared::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          params->notification_id);

  bool found_notification = notification != NULL;
  if (found_notification)
    notification->delegate()->Close(true);

  return RespondNow(
      ArgumentList(api::notification_provider::NotifyOnCleared::Results::Create(
          found_notification)));
}

NotificationProviderNotifyOnClickedFunction::
    NotificationProviderNotifyOnClickedFunction() {
}

NotificationProviderNotifyOnClickedFunction::
    ~NotificationProviderNotifyOnClickedFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnClickedFunction::Run() {
  scoped_ptr<api::notification_provider::NotifyOnClicked::Params> params =
      api::notification_provider::NotifyOnClicked::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          params->notification_id);

  bool found_notification = notification != NULL;
  if (found_notification)
    notification->delegate()->Click();

  return RespondNow(
      ArgumentList(api::notification_provider::NotifyOnClicked::Results::Create(
          found_notification)));
}

NotificationProviderNotifyOnButtonClickedFunction::
    NotificationProviderNotifyOnButtonClickedFunction() {
}

NotificationProviderNotifyOnButtonClickedFunction::
    ~NotificationProviderNotifyOnButtonClickedFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnButtonClickedFunction::Run() {
  scoped_ptr<api::notification_provider::NotifyOnButtonClicked::Params> params =
      api::notification_provider::NotifyOnButtonClicked::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          params->notification_id);

  bool found_notification = notification != NULL;
  if (found_notification)
    notification->delegate()->ButtonClick(params->button_index);

  return RespondNow(ArgumentList(
      api::notification_provider::NotifyOnButtonClicked::Results::Create(
          found_notification)));
}

NotificationProviderNotifyOnPermissionLevelChangedFunction::
    NotificationProviderNotifyOnPermissionLevelChangedFunction() {
}

NotificationProviderNotifyOnPermissionLevelChangedFunction::
    ~NotificationProviderNotifyOnPermissionLevelChangedFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnPermissionLevelChangedFunction::Run() {
  scoped_ptr<api::notification_provider::NotifyOnPermissionLevelChanged::Params>
      params = api::notification_provider::NotifyOnPermissionLevelChanged::
          Params::Create(*args_);

  EXTENSION_FUNCTION_VALIDATE(params.get());

  return RespondNow(
      ArgumentList(api::notification_provider::NotifyOnPermissionLevelChanged::
                       Results::Create(true)));
}

NotificationProviderNotifyOnShowSettingsFunction::
    NotificationProviderNotifyOnShowSettingsFunction() {
}

NotificationProviderNotifyOnShowSettingsFunction::
    ~NotificationProviderNotifyOnShowSettingsFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnShowSettingsFunction::Run() {
  scoped_ptr<api::notification_provider::NotifyOnShowSettings::Params> params =
      api::notification_provider::NotifyOnShowSettings::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return RespondNow(ArgumentList(
      api::notification_provider::NotifyOnShowSettings::Results::Create(true)));
}

NotificationProviderGetNotifierFunction::
    NotificationProviderGetNotifierFunction() {
}

NotificationProviderGetNotifierFunction::
    ~NotificationProviderGetNotifierFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderGetNotifierFunction::Run() {
  api::notification_provider::Notifier notifier;

  return RespondNow(ArgumentList(
      api::notification_provider::GetNotifier::Results::Create(notifier)));
}

NotificationProviderGetAllNotifiersFunction::
    NotificationProviderGetAllNotifiersFunction() {
}

NotificationProviderGetAllNotifiersFunction::
    ~NotificationProviderGetAllNotifiersFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderGetAllNotifiersFunction::Run() {
  std::vector<linked_ptr<api::notification_provider::Notifier> > notifiers;

  return RespondNow(ArgumentList(
      api::notification_provider::GetAllNotifiers::Results::Create(notifiers)));
}

}  // namespace extensions
