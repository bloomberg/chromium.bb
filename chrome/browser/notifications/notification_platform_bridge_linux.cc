// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification.h"

namespace {

const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

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

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux(
    GDBusProxy* notification_proxy)
    : notification_proxy_(notification_proxy) {}

NotificationPlatformBridgeLinux::~NotificationPlatformBridgeLinux() {
  g_object_unref(notification_proxy_);
}

void NotificationPlatformBridgeLinux::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool is_incognito,
    const Notification& notification) {
  // TODO(thomasanderson): Add a complete implementation.
  g_dbus_proxy_call(
      notification_proxy_, "Notify",
      g_variant_new("(susssasa{sv}i)", "", 0, "",
                    base::UTF16ToUTF8(notification.title()).c_str(),
                    base::UTF16ToUTF8(notification.message()).c_str(), nullptr,
                    nullptr, -1),
      G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
}

void NotificationPlatformBridgeLinux::Close(
    const std::string& profile_id,
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeLinux::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    const DisplayedNotificationsCallback& callback) const {
  callback.Run(base::MakeUnique<std::set<std::string>>(), false);
}
