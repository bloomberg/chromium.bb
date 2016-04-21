// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_mac.h"

#include <utility>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

@class NSUserNotification;
@class NSUserNotificationCenter;

// The mapping from web notifications to NsUserNotification works as follows

// notification#title in NSUserNotification.title
// notification#message in NSUserNotification.subtitle
// notification#context_message in NSUserNotification.informativeText
// notification#tag in NSUserNotification.identifier (10.9)
// notification#icon in NSUserNotification.contentImage (10.9)
// Site settings button is implemented as NSUserNotification's action button
// Not possible to implement:
// -notification.requireInteraction
// -The event associated to the close button

// TODO(miguelg) implement the following features
// - Sound names can be implemented by setting soundName in NSUserNotification
//   NSUserNotificationDefaultSoundName gives you the platform default.

namespace {

// Keys in NSUserNotification.userInfo to map chrome notifications to
// native ones.
NSString* const kNotificationOriginKey = @"notification_origin";
NSString* const kNotificationPersistentIdKey = @"notification_persistent_id";

NSString* const kNotificationProfilePersistentIdKey =
    @"notification_profile_persistent_id";
NSString* const kNotificationIncognitoKey = @"notification_incognito";

}  // namespace

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationUIManagerMac(
      [NSUserNotificationCenter defaultUserNotificationCenter]);
}

// A Cocoa class that represents the delegate of NSUserNotificationCenter and
// can forward commands to C++.
@interface NotificationCenterDelegate
    : NSObject<NSUserNotificationCenterDelegate> {
}
@end

// /////////////////////////////////////////////////////////////////////////////

NotificationUIManagerMac::NotificationUIManagerMac(
    NSUserNotificationCenter* notification_center)
    : delegate_([NotificationCenterDelegate alloc]),
      notification_center_(notification_center) {
  [notification_center_ setDelegate:delegate_.get()];
}

NotificationUIManagerMac::~NotificationUIManagerMac() {
  [notification_center_ setDelegate:nil];

  // TODO(miguelg) lift this restriction if possible.
  [notification_center_ removeAllDeliveredNotifications];
}

void NotificationUIManagerMac::Display(const std::string& notification_id,
                                       const std::string& profile_id,
                                       bool incognito,
                                       const Notification& notification) {
  base::scoped_nsobject<NSUserNotification> toast(
      [[NSUserNotification alloc] init]);
  [toast setTitle:base::SysUTF16ToNSString(notification.title())];
  [toast setSubtitle:base::SysUTF16ToNSString(notification.message())];

  // TODO(miguelg): try to elide the origin perhaps See NSString
  // stringWithFormat. It seems that the informativeText font is constant.
  NSString* informative_text =
      notification.context_message().empty()
          ? base::SysUTF8ToNSString(notification.origin_url().spec())
          : base::SysUTF16ToNSString(notification.context_message());
  [toast setInformativeText:informative_text];

  // Some functionality requires private APIs
  // Icon
  if ([toast respondsToSelector:@selector(_identityImage)] &&
      !notification.icon().IsEmpty()) {
    [toast setValue:notification.icon().ToNSImage() forKey:@"_identityImage"];
    [toast setValue:@NO forKey:@"_identityImageHasBorder"];
  }

  // Buttons
  if ([toast respondsToSelector:@selector(_showsButtons)]) {
    [toast setValue:@YES forKey:@"_showsButtons"];
    // A default close button label is provided by the platform but we
    // explicitly override it in case the user decides to not
    // use the OS language in Chrome.
    [toast setOtherButtonTitle:l10n_util::GetNSString(
                                   IDS_NOTIFICATION_BUTTON_CLOSE)];

    // Display the Settings button as the action button if there either are no
    // developer-provided action buttons, or the alternate action menu is not
    // available on this Mac version. This avoids needlessly showing the menu.
    if (notification.buttons().empty() ||
        ![toast respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]) {
      [toast setActionButtonTitle:l10n_util::GetNSString(
                                      IDS_NOTIFICATION_BUTTON_SETTINGS)];
    } else {
      // Otherwise show the alternate menu, then show the developer actions and
      // finally the settings one.
      DCHECK(
          [toast respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]);
      DCHECK(
          [toast respondsToSelector:@selector(_alternateActionButtonTitles)]);

      [toast setActionButtonTitle:l10n_util::GetNSString(
                                      IDS_NOTIFICATION_BUTTON_OPTIONS)];
      [toast setValue:@YES
               forKey:@"_alwaysShowAlternateActionMenu"];

      NSMutableArray* buttons = [NSMutableArray arrayWithCapacity:3];
      for (const auto& action : notification.buttons())
        [buttons addObject:base::SysUTF16ToNSString(action.title)];
      [buttons
          addObject:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_SETTINGS)];

      [toast setValue:buttons forKey:@"_alternateActionButtonTitles"];
    }
  }

  // Tag
  if ([toast respondsToSelector:@selector(setIdentifier:)] &&
      !notification.tag().empty()) {
    [toast setValue:base::SysUTF8ToNSString(notification.tag())
             forKey:@"identifier"];

    // If renotify is needed, delete the notification with the same tag
    // from the notification center before displaying this one.
    if (notification.renotify()) {
      NSUserNotificationCenter* notification_center =
          [NSUserNotificationCenter defaultUserNotificationCenter];
      for (NSUserNotification* existing_notification in
           [notification_center deliveredNotifications]) {
        NSString* identifier =
            [existing_notification valueForKey:@"identifier"];
        if ([identifier isEqual:base::SysUTF8ToNSString(notification.tag())]) {
          [notification_center
              removeDeliveredNotification:existing_notification];
          break;
        }
      }
    }
  }

  toast.get().userInfo = @{
    kNotificationOriginKey :
        base::SysUTF8ToNSString(notification.origin_url().spec()),
    kNotificationPersistentIdKey : base::SysUTF8ToNSString(notification_id),
    kNotificationProfilePersistentIdKey : base::SysUTF8ToNSString(profile_id),
    kNotificationIncognitoKey : [NSNumber numberWithBool:incognito]
  };

  [notification_center_ deliverNotification:toast];
  notification.delegate()->Display();
}

void NotificationUIManagerMac::Close(const std::string& profile_id,
                                     const std::string& notification_id) {
  NSString* candidate_id = base::SysUTF8ToNSString(notification_id);

  NSString* current_profile_id = base::SysUTF8ToNSString(profile_id);
  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toast_id =
        [toast.userInfo objectForKey:kNotificationPersistentIdKey];

    NSString* persistent_profile_id =
        [toast.userInfo objectForKey:kNotificationProfilePersistentIdKey];

    if (toast_id == candidate_id &&
        persistent_profile_id == current_profile_id) {
      [notification_center_ removeDeliveredNotification:toast];
    }
  }
}

bool NotificationUIManagerMac::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    std::set<std::string>* notifications) const {
  DCHECK(notifications);
  NSString* current_profile_id = base::SysUTF8ToNSString(profile_id);
  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toast_profile_id =
        [toast.userInfo objectForKey:kNotificationProfilePersistentIdKey];
    if (toast_profile_id == current_profile_id) {
      notifications->insert(base::SysNSStringToUTF8(
          [toast.userInfo objectForKey:kNotificationPersistentIdKey]));
    }
  }
  return true;
}

bool NotificationUIManagerMac::SupportsNotificationCenter() const {
  return true;
}

// /////////////////////////////////////////////////////////////////////////////

@implementation NotificationCenterDelegate
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  std::string notificationOrigin = base::SysNSStringToUTF8(
      [notification.userInfo objectForKey:kNotificationOriginKey]);
  NSNumber* persistentNotificationId =
      [notification.userInfo objectForKey:kNotificationPersistentIdKey];
  NSString* persistentProfileId =
      [notification.userInfo objectForKey:kNotificationProfilePersistentIdKey];
  NSNumber* isIncognito =
      [notification.userInfo objectForKey:kNotificationIncognitoKey];

  GURL origin(notificationOrigin);

  // Initialize operation and button index for the case where the
  // notification itself was clicked.
  PlatformNotificationServiceImpl::NotificationOperation operation =
      PlatformNotificationServiceImpl::NOTIFICATION_CLICK;
  int buttonIndex = -1;

  // Determine whether the user clicked on a button, and if they did, whether it
  // was a developer-provided button or the mandatory Settings button.
  if (notification.activationType ==
      NSUserNotificationActivationTypeActionButtonClicked) {
    NSArray* alternateButtons = @[];
    if ([notification
            respondsToSelector:@selector(_alternateActionButtonTitles)]) {
      alternateButtons =
          [notification valueForKey:@"_alternateActionButtonTitles"];
    }

    bool multipleButtons = (alternateButtons.count > 0);

    // No developer actions, just the settings button.
    if (!multipleButtons) {
      operation = PlatformNotificationServiceImpl::NOTIFICATION_SETTINGS;
      buttonIndex = -1;
    } else {
      // 0 based array containing.
      // Button 1
      // Button 2 (optional)
      // Settings
      NSNumber* actionIndex =
          [notification valueForKey:@"_alternateActionIndex"];
      operation = (actionIndex.unsignedLongValue == alternateButtons.count - 1)
                      ? PlatformNotificationServiceImpl::NOTIFICATION_SETTINGS
                      : PlatformNotificationServiceImpl::NOTIFICATION_CLICK;
      buttonIndex =
          (actionIndex.unsignedLongValue == alternateButtons.count - 1)
              ? -1
              : actionIndex.intValue;
    }
  }

  PlatformNotificationServiceImpl::GetInstance()
      ->ProcessPersistentNotificationOperation(
          operation, base::SysNSStringToUTF8(persistentProfileId),
          [isIncognito boolValue], origin,
          persistentNotificationId.longLongValue, buttonIndex);
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end
