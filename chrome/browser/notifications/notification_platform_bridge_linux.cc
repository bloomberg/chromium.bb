// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/native_notification_display_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace {

const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

void AddActionToNotification(GVariantBuilder* actions_builder,
                             const char* action_id,
                             const char* button_label) {
  g_variant_builder_add(actions_builder, "s", action_id);
  g_variant_builder_add(actions_builder, "s", button_label);
}

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

// Runs once the profile has been loaded in order to perform a given
// |operation| on a notification.
void ProfileLoadedCallback(NotificationCommon::Operation operation,
                           NotificationCommon::Type notification_type,
                           const std::string& origin,
                           const std::string& notification_id,
                           int action_index,
                           const base::NullableString16& reply,
                           Profile* profile) {
  if (!profile)
    return;

  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);

  static_cast<NativeNotificationDisplayService*>(display_service)
      ->ProcessNotificationOperation(operation, notification_type, origin,
                                     notification_id, action_index, reply);
}

}  // namespace

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  GDBusProxy* notification_proxy = g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SESSION,
      static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                   G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START),
      nullptr, kFreedesktopNotificationsName, kFreedesktopNotificationsPath,
      kFreedesktopNotificationsName, nullptr, nullptr);
  if (!notification_proxy)
    return nullptr;
  return new NotificationPlatformBridgeLinux(notification_proxy);
}

struct NotificationPlatformBridgeLinux::NotificationData {
  NotificationData(NotificationCommon::Type notification_type,
                   const std::string& notification_id,
                   const std::string& profile_id,
                   bool is_incognito,
                   const GURL& origin_url)
      : notification_type(notification_type),
        notification_id(notification_id),
        profile_id(profile_id),
        is_incognito(is_incognito),
        origin_url(origin_url) {}

  ~NotificationData() {
    if (cancellable)
      g_cancellable_cancel(cancellable);
  }

  // The ID used by the notification server.  Will be 0 until the
  // first "Notify" message completes.
  uint32_t dbus_id = 0;

  // Same parameters used by NotificationPlatformBridge::Display().
  const NotificationCommon::Type notification_type;
  const std::string notification_id;
  const std::string profile_id;
  const bool is_incognito;

  // A copy of the origin_url from the underlying
  // message_center::Notification.  Used to pass back to
  // NativeNotificationDisplayService.
  const GURL origin_url;

  // Used to cancel the initial "Notify" message so we don't call
  // NotificationPlatformBridgeLinux::NotifyCompleteInternal() with a
  // destroyed Notification.
  ScopedGObject<GCancellable> cancellable;

  // If not null, the data to update the notification with once
  // |dbus_id| becomes available.
  std::unique_ptr<Notification> update_data;

  // If true, indicates the notification should be closed once
  // |dbus_id| becomes available.
  bool should_close = false;
};

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux(
    GDBusProxy* notification_proxy)
    : notification_proxy_(notification_proxy) {
  proxy_signal_handler_ = g_signal_connect(
      notification_proxy_, "g-signal", G_CALLBACK(GSignalReceiverThunk), this);
}

NotificationPlatformBridgeLinux::~NotificationPlatformBridgeLinux() {
  if (proxy_signal_handler_)
    g_signal_handler_disconnect(notification_proxy_, proxy_signal_handler_);
}

void NotificationPlatformBridgeLinux::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool is_incognito,
    const Notification& notification) {
  NotificationData* data =
      FindNotificationData(notification_id, profile_id, is_incognito);
  if (data) {
    // Update an existing notification.
    DCHECK_EQ(data->notification_type, notification_type);
    if (data->dbus_id) {
      NotifyNow(data->dbus_id, notification_type, notification, nullptr,
                nullptr, nullptr);
    } else {
      data->update_data = base::MakeUnique<Notification>(notification);
    }
  } else {
    // Send the notification for the first time.
    data = new NotificationData(notification_type, notification_id, profile_id,
                                is_incognito, notification.origin_url());
    data->cancellable.reset(g_cancellable_new());
    notifications_.emplace(data, base::WrapUnique(data));
    NotifyNow(0, notification_type, notification, data->cancellable,
              NotifyCompleteReceiver, data);
  }
}

void NotificationPlatformBridgeLinux::Close(
    const std::string& profile_id,
    const std::string& notification_id) {
  std::vector<NotificationData*> to_erase;
  for (const auto& pair : notifications_) {
    NotificationData* data = pair.first;
    if (data->notification_id == notification_id &&
        data->profile_id == profile_id) {
      if (data->dbus_id) {
        CloseNow(data->dbus_id);
        to_erase.push_back(data);
      } else {
        data->should_close = true;
      }
    }
  }
  for (NotificationData* data : to_erase)
    notifications_.erase(data);
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
    NotifyNow(data->dbus_id, data->notification_type, *data->update_data,
              nullptr, nullptr, nullptr);
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

  GVariantBuilder actions_builder;
  // Even-indexed elements in this array are action IDs passed back to
  // us in GSignalReceiver.  Odd-indexed ones contain the button text.
  g_variant_builder_init(&actions_builder, G_VARIANT_TYPE("as"));
  if (notification.clickable()) {
    // Special case: the pair ("default", "") will not add a button,
    // but instead makes the entire notification clickable.
    AddActionToNotification(&actions_builder, "default", "");
  }
  // Always add a settings button.
  AddActionToNotification(&actions_builder, "settings", "Settings");

  const std::string title = base::UTF16ToUTF8(notification.title());
  const std::string message = base::UTF16ToUTF8(notification.message());

  GVariant* parameters =
      g_variant_new("(susssasa{sv}i)", "", dbus_id, "", title.c_str(),
                    message.c_str(), &actions_builder, nullptr, -1);
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
    const std::string& profile_id,
    bool is_incognito) {
  for (const auto& pair : notifications_) {
    NotificationData* data = pair.first;
    if (data->notification_id == notification_id &&
        data->profile_id == profile_id && data->is_incognito == is_incognito) {
      return data;
    }
  }

  return nullptr;
}

NotificationPlatformBridgeLinux::NotificationData*
NotificationPlatformBridgeLinux::FindNotificationData(uint32_t dbus_id) {
  for (const auto& pair : notifications_) {
    NotificationData* data = pair.first;
    if (data->dbus_id == dbus_id)
      return data;
  }

  return nullptr;
}

void NotificationPlatformBridgeLinux::ForwardNotificationOperation(
    uint32_t dbus_id,
    NotificationCommon::Operation operation,
    int action_index) {
  NotificationData* data = FindNotificationData(dbus_id);
  if (!data) {
    // This notification either belongs to a different app or we
    // already removed the NotificationData after sending a
    // "CloseNotification" message.
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  profile_manager->LoadProfile(
      data->profile_id, data->is_incognito,
      base::Bind(&ProfileLoadedCallback, operation, data->notification_type,
                 data->origin_url.spec(), data->notification_id, action_index,
                 base::NullableString16()));
}

void NotificationPlatformBridgeLinux::GSignalReceiver(GDBusProxy* proxy,
                                                      const char* sender_name,
                                                      const char* sender_signal,
                                                      GVariant* parameters) {
  uint32_t dbus_id = 0;
  if (strcmp("NotificationClosed", sender_signal) == 0 &&
      g_variant_is_of_type(parameters, G_VARIANT_TYPE("(uu)"))) {
    uint32_t reason;
    g_variant_get(parameters, "(uu)", &dbus_id, &reason);
    ForwardNotificationOperation(dbus_id, NotificationCommon::CLOSE, -1);
    // std::unordered_map::erase(nullptr) is safe here.
    notifications_.erase(FindNotificationData(dbus_id));
  } else if (strcmp("ActionInvoked", sender_signal) == 0 &&
             g_variant_is_of_type(parameters, G_VARIANT_TYPE("(us)"))) {
    const gchar* action = nullptr;
    g_variant_get(parameters, "(u&s)", &dbus_id, &action);
    DCHECK(action);

    if (strcmp(action, "default") == 0) {
      ForwardNotificationOperation(dbus_id, NotificationCommon::CLICK, 0);
    } else if (strcmp(action, "settings") == 0) {
      ForwardNotificationOperation(dbus_id, NotificationCommon::SETTINGS, -1);
    } else {
      NOTIMPLEMENTED() << "No custom buttons just yet!";
    }
  }
}
