// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"

namespace {

const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

// Callback used by GLib when the "Notify" message completes for the
// first time.
void NotifyCompleteReceiver(GObject* source_object,
                            GAsyncResult* result,
                            gpointer user_data) {
  GDBusProxy* proxy = G_DBUS_PROXY(source_object);
  GVariant* value = g_dbus_proxy_call_finish(proxy, result, nullptr);
  if (!value) {
    // The message might have been cancelled, in which case
    // |user_data| points to a destroyed NotificationData.
    return;
  }
  auto* platform_bridge_linux = static_cast<NotificationPlatformBridgeLinux*>(
      g_browser_process->notification_platform_bridge());
  platform_bridge_linux->NotifyCompleteInternal(user_data, value);
}

}  // namespace

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  GDBusProxy* notification_proxy = g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SESSION,
      static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                   G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                                   G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START),
      nullptr, kFreedesktopNotificationsName, kFreedesktopNotificationsPath,
      kFreedesktopNotificationsName, nullptr, nullptr);
  if (!notification_proxy)
    return nullptr;
  return new NotificationPlatformBridgeLinux(notification_proxy);
}

struct NotificationPlatformBridgeLinux::NotificationData {
  NotificationData(const std::string& notification_id,
                   const std::string& profile_id,
                   bool is_incognito)
      : notification_id(notification_id),
        profile_id(profile_id),
        is_incognito(is_incognito) {}

  ~NotificationData() {
    if (cancellable)
      g_cancellable_cancel(cancellable);
  }

  // The ID used by the notification server.  Will be 0 until the
  // first "Notify" message completes.
  uint32_t dbus_id = 0;

  // Same parameters used by NotificationPlatformBridge::Display().
  const std::string notification_id;
  const std::string profile_id;
  const bool is_incognito;

  // Used to cancel the initial "Notify" message so we don't call
  // NotificationPlatformBridgeLinux::NotifyCompleteInternal() with a
  // destroyed Notification.
  ScopedGObject<GCancellable> cancellable;

  // If not null, the data to update the notification with once
  // |dbus_id| becomes available.
  std::unique_ptr<Notification> update_data;
  NotificationCommon::Type update_type = NotificationCommon::TYPE_MAX;

  // If true, indicates the notification should be closed once
  // |dbus_id| becomes available.
  bool should_close = false;
};

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux(
    GDBusProxy* notification_proxy)
    : notification_proxy_(notification_proxy) {}

NotificationPlatformBridgeLinux::~NotificationPlatformBridgeLinux() {}

void NotificationPlatformBridgeLinux::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool is_incognito,
    const Notification& notification) {
  NotificationData* data = FindNotificationData(notification_id, profile_id);
  if (data) {
    // Update an existing notification.
    if (data->dbus_id) {
      NotifyNow(data->dbus_id, notification_type, notification, nullptr,
                nullptr, nullptr);
    } else {
      data->update_type = notification_type;
      data->update_data = base::MakeUnique<Notification>(notification);
    }
  } else {
    // Send the notification for the first time.
    data = new NotificationData(notification_id, profile_id, is_incognito);
    data->cancellable.reset(g_cancellable_new());
    notifications_.emplace(data, base::WrapUnique(data));
    NotifyNow(0, notification_type, notification, data->cancellable,
              NotifyCompleteReceiver, data);
  }
}

void NotificationPlatformBridgeLinux::Close(
    const std::string& profile_id,
    const std::string& notification_id) {
  NotificationData* data = FindNotificationData(notification_id, profile_id);
  if (!data)
    return;
  if (data->dbus_id) {
    CloseNow(data->dbus_id);
    notifications_.erase(data);
  } else {
    data->should_close = true;
  }
}

void NotificationPlatformBridgeLinux::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    const GetDisplayedNotificationsCallback& callback) const {
  // TODO(thomasanderson): implement.
  callback.Run(base::MakeUnique<std::set<std::string>>(), false);
}

void NotificationPlatformBridgeLinux::NotifyCompleteInternal(gpointer user_data,
                                                             GVariant* value) {
  NotificationData* data = reinterpret_cast<NotificationData*>(user_data);
  if (!base::ContainsKey(notifications_, data))
    return;
  data->cancellable.reset();
  if (value && g_variant_is_of_type(value, G_VARIANT_TYPE("(u)")))
    g_variant_get(value, "(u)", &data->dbus_id);

  if (!data->dbus_id) {
    // There was some sort of error with creating the notification.
    notifications_.erase(data);
  } else if (data->should_close) {
    CloseNow(data->dbus_id);
    notifications_.erase(data);
  } else if (data->update_data) {
    NotifyNow(data->dbus_id, data->update_type, *data->update_data, nullptr,
              nullptr, nullptr);
    data->update_data.reset();
  }
}

void NotificationPlatformBridgeLinux::NotifyNow(
    uint32_t dbus_id,
    NotificationCommon::Type notification_type,
    const Notification& notification,
    GCancellable* cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data) {
  // TODO(thomasanderson): Add a complete implementation.
  const std::string title = base::UTF16ToUTF8(notification.title());
  const std::string message = base::UTF16ToUTF8(notification.message());
  GVariant* parameters =
      g_variant_new("(susssasa{sv}i)", "", dbus_id, "", title.c_str(),
                    message.c_str(), nullptr, nullptr, -1);
  g_dbus_proxy_call(notification_proxy_, "Notify", parameters,
                    G_DBUS_CALL_FLAGS_NONE, -1, cancellable, callback,
                    user_data);
}

void NotificationPlatformBridgeLinux::CloseNow(uint32_t dbus_id) {
  g_dbus_proxy_call(notification_proxy_, "CloseNotification",
                    g_variant_new("(u)", dbus_id), G_DBUS_CALL_FLAGS_NONE, -1,
                    nullptr, nullptr, nullptr);
}

NotificationPlatformBridgeLinux::NotificationData*
NotificationPlatformBridgeLinux::FindNotificationData(
    const std::string& notification_id,
    const std::string& profile_id) {
  for (const auto& pair : notifications_) {
    NotificationData* data = pair.first;
    if (data->notification_id == notification_id &&
        data->profile_id == profile_id) {
      return data;
    }
  }

  return nullptr;
}
