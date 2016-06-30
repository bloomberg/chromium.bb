// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac.h"

#include <utility>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationConstants.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"
#include "url/origin.h"

@class NSUserNotification;
@class NSUserNotificationCenter;

// The mapping from web notifications to NsUserNotification works as follows

// notification#title in NSUserNotification.title
// notification#message in NSUserNotification.informativeText
// notification#context_message in NSUserNotification.subtitle
// notification#tag in NSUserNotification.identifier (10.9)
// notification#icon in NSUserNotification.contentImage (10.9)
// Site settings button is implemented as NSUserNotification's action button
// Not possible to implement:
// -notification.requireInteraction
// -The event associated to the close button

// TODO(miguelg) implement the following features
// - Sound names can be implemented by setting soundName in NSUserNotification
//   NSUserNotificationDefaultSoundName gives you the platform default.


// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationPlatformBridgeMac(
      [NSUserNotificationCenter defaultUserNotificationCenter]);
}

// A Cocoa class that represents the delegate of NSUserNotificationCenter and
// can forward commands to C++.
@interface NotificationCenterDelegate
    : NSObject<NSUserNotificationCenterDelegate> {
}
@end

// /////////////////////////////////////////////////////////////////////////////

NotificationPlatformBridgeMac::NotificationPlatformBridgeMac(
    NSUserNotificationCenter* notification_center)
    : delegate_([NotificationCenterDelegate alloc]),
      notification_center_(notification_center) {
  [notification_center_ setDelegate:delegate_.get()];
}

NotificationPlatformBridgeMac::~NotificationPlatformBridgeMac() {
  [notification_center_ setDelegate:nil];

  // TODO(miguelg) lift this restriction if possible.
  [notification_center_ removeAllDeliveredNotifications];
}

void NotificationPlatformBridgeMac::Display(const std::string& notification_id,
                                            const std::string& profile_id,
                                            bool incognito,
                                            const Notification& notification) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] init]);

  [builder setTitle:base::SysUTF16ToNSString(notification.title())];
  [builder setContextMessage:base::SysUTF16ToNSString(notification.message())];

  base::string16 subtitle =
      notification.context_message().empty()
          ? url_formatter::FormatOriginForSecurityDisplay(
                url::Origin(notification.origin_url()),
                url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)
          : notification.context_message();

  [builder setSubTitle:base::SysUTF16ToNSString(subtitle)];
  if (!notification.icon().IsEmpty()) {
    [builder setIcon:notification.icon().ToNSImage()];
  }

  std::vector<message_center::ButtonInfo> buttons = notification.buttons();
  if (!buttons.empty()) {
    DCHECK_LE(buttons.size(), blink::kWebNotificationMaxActions);
    NSString* buttonOne = SysUTF16ToNSString(buttons[0].title);
    NSString* buttonTwo = nullptr;
    if (buttons.size() > 1)
      buttonTwo = SysUTF16ToNSString(buttons[1].title);
    [builder setButtons:buttonOne secondaryButton:buttonTwo];
  }

  // Tag
  if (!notification.tag().empty()) {
    [builder setTag:base::SysUTF8ToNSString(notification.tag())];
    // If renotify is needed, delete the notification with the same tag
    // from the notification center before displaying this one.
    // TODO(miguelg): This will need to work for alerts as well via XPC
    // once supported.
    if (notification.renotify()) {
      NSUserNotificationCenter* notification_center =
          [NSUserNotificationCenter defaultUserNotificationCenter];
      for (NSUserNotification* existing_notification in
           [notification_center deliveredNotifications]) {
        NSString* identifier =
            [existing_notification valueForKey:@"identifier"];
        if ([identifier
                isEqualToString:base::SysUTF8ToNSString(notification.tag())]) {
          [notification_center
              removeDeliveredNotification:existing_notification];
          break;
        }
      }
    }
  }

  [builder setOrigin:base::SysUTF8ToNSString(notification.origin_url().spec())];
  [builder setNotificationId:base::SysUTF8ToNSString(notification_id)];
  [builder setProfileId:base::SysUTF8ToNSString(profile_id)];
  [builder setIncognito:incognito];

  NSUserNotification* toast = [builder buildUserNotification];
  [notification_center_ deliverNotification:toast];
}

void NotificationPlatformBridgeMac::Close(const std::string& profile_id,
                                          const std::string& notification_id) {
  NSString* candidate_id = base::SysUTF8ToNSString(notification_id);

  NSString* current_profile_id = base::SysUTF8ToNSString(profile_id);
  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toast_id =
        [toast.userInfo objectForKey:notification_constants::kNotificationId];

    NSString* persistent_profile_id = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    if ([toast_id isEqualToString:candidate_id] &&
        [persistent_profile_id isEqualToString:current_profile_id]) {
      [notification_center_ removeDeliveredNotification:toast];
    }
  }
}

bool NotificationPlatformBridgeMac::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    std::set<std::string>* notifications) const {
  DCHECK(notifications);

  NSString* current_profile_id = base::SysUTF8ToNSString(profile_id);
  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toast_profile_id = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    if (toast_profile_id == current_profile_id) {
      notifications->insert(base::SysNSStringToUTF8([toast.userInfo
          objectForKey:notification_constants::kNotificationId]));
    }
  }
  return true;
}

bool NotificationPlatformBridgeMac::SupportsNotificationCenter() const {
  return true;
}

// /////////////////////////////////////////////////////////////////////////////

@implementation NotificationCenterDelegate
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  NSDictionary* response =
      [NotificationResponseBuilder buildDictionary:notification];

  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];

  std::string notificationOrigin = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationOrigin]);
  NSString* notificationId = [notification.userInfo
      objectForKey:notification_constants::kNotificationId];
  std::string persistentNotificationId =
      base::SysNSStringToUTF8(notificationId);
  int64_t persistentId;
  if (!base::StringToInt64(persistentNotificationId, &persistentId)) {
    LOG(ERROR) << "Unable to convert notification ID: "
               << persistentNotificationId << " to integer.";
    return;
  }
  std::string profileId = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationProfileId]);
  NSNumber* isIncognito =
      [response objectForKey:notification_constants::kNotificationIncognito];

  GURL origin(notificationOrigin);

  PlatformNotificationServiceImpl::GetInstance()
      ->ProcessPersistentNotificationOperation(
          static_cast<NotificationCommon::Operation>(operation.intValue),
          profileId, [isIncognito boolValue], origin, persistentId,
          buttonIndex.intValue);
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end
