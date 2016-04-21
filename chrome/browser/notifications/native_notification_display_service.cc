// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/native_notification_display_service.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/profiles/profile.h"

namespace {

std::string GetProfileId(Profile* profile) {
#if defined(OS_WIN)
  std::string profile_id =
      base::WideToUTF8(profile->GetPath().BaseName().value());
#elif defined(OS_POSIX)
  std::string profile_id = profile->GetPath().BaseName().value();
#endif
  return profile_id;
}

}  // namespace

NativeNotificationDisplayService::NativeNotificationDisplayService(
    Profile* profile,
    NotificationPlatformBridge* notification_bridge)
    : profile_(profile), notification_bridge_(notification_bridge) {
  DCHECK(profile_);
  DCHECK(notification_bridge_);
}

NativeNotificationDisplayService::~NativeNotificationDisplayService() {}

void NativeNotificationDisplayService::Display(
    const std::string& notification_id,
    const Notification& notification) {
  notification_bridge_->Display(notification_id, GetProfileId(profile_),
                                profile_->IsOffTheRecord(), notification);
}

void NativeNotificationDisplayService::Close(
    const std::string& notification_id) {
  notification_bridge_->Close(GetProfileId(profile_), notification_id);
}

bool NativeNotificationDisplayService::GetDisplayed(
    std::set<std::string>* notifications) const {
  return notification_bridge_->GetDisplayed(
      GetProfileId(profile_), profile_->IsOffTheRecord(), notifications);
}

bool NativeNotificationDisplayService::SupportsNotificationCenter() const {
  return notification_bridge_->SupportsNotificationCenter();
}
