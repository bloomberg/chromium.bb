// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_notification_observer.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

std::string Str(const std::vector<extensions::NotificationType>& types) {
  std::string str = "[";
  bool needs_comma = false;
  for (std::vector<extensions::NotificationType>::const_iterator it =
           types.begin();
       it != types.end();
       ++it) {
    if (needs_comma)
      str += ",";
    needs_comma = true;
    str += base::StringPrintf("%d", *it);
  }
  str += "]";
  return str;
}

}  // namespace

ExtensionNotificationObserver::ExtensionNotificationObserver(
    content::NotificationSource source,
    const std::set<std::string>& extension_ids)
    : extension_ids_(extension_ids) {
  registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED, source);
  registrar_.Add(
      this,
      extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      source);
  registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED, source);
}

ExtensionNotificationObserver::~ExtensionNotificationObserver() {}

testing::AssertionResult ExtensionNotificationObserver::CheckNotifications() {
  return CheckNotifications(std::vector<extensions::NotificationType>());
}

testing::AssertionResult ExtensionNotificationObserver::CheckNotifications(
    extensions::NotificationType type) {
  return CheckNotifications(std::vector<extensions::NotificationType>(1, type));
}

testing::AssertionResult ExtensionNotificationObserver::CheckNotifications(
    extensions::NotificationType t1,
    extensions::NotificationType t2) {
  std::vector<extensions::NotificationType> types;
  types.push_back(t1);
  types.push_back(t2);
  return CheckNotifications(types);
}

testing::AssertionResult ExtensionNotificationObserver::CheckNotifications(
    extensions::NotificationType t1,
    extensions::NotificationType t2,
    extensions::NotificationType t3) {
  std::vector<extensions::NotificationType> types;
  types.push_back(t1);
  types.push_back(t2);
  types.push_back(t3);
  return CheckNotifications(types);
}

testing::AssertionResult ExtensionNotificationObserver::CheckNotifications(
    extensions::NotificationType t1,
    extensions::NotificationType t2,
    extensions::NotificationType t3,
    extensions::NotificationType t4,
    extensions::NotificationType t5,
    extensions::NotificationType t6) {
  std::vector<extensions::NotificationType> types;
  types.push_back(t1);
  types.push_back(t2);
  types.push_back(t3);
  types.push_back(t4);
  types.push_back(t5);
  types.push_back(t6);
  return CheckNotifications(types);
}

// content::NotificationObserver implementation.
void ExtensionNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED: {
      const Extension* extension =
          content::Details<const InstalledExtensionInfo>(details)->extension;
      if (extension_ids_.count(extension->id()))
        notifications_.push_back(
            static_cast<extensions::NotificationType>(type));
      break;
    }

    case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension_ids_.count(extension->id()))
        notifications_.push_back(
            static_cast<extensions::NotificationType>(type));
      break;
    }

    case extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      UnloadedExtensionInfo* reason =
          content::Details<UnloadedExtensionInfo>(details).ptr();
      if (extension_ids_.count(reason->extension->id())) {
        notifications_.push_back(
            static_cast<extensions::NotificationType>(type));
        // The only way that extensions are unloaded in these tests is
        // by blacklisting.
        EXPECT_EQ(UnloadedExtensionInfo::REASON_BLACKLIST,
                  reason->reason);
      }
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

testing::AssertionResult ExtensionNotificationObserver::CheckNotifications(
    const std::vector<extensions::NotificationType>& types) {
  testing::AssertionResult result = (notifications_ == types) ?
      testing::AssertionSuccess() :
      testing::AssertionFailure() << "Expected " << Str(types) << ", " <<
                                     "Got " << Str(notifications_);
  notifications_.clear();
  return result;
}

}  // namespace extensions
