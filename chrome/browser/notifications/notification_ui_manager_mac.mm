// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_mac.h"

#include <utility>

#include "base/command_line.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
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
// notification.requireInteraction cannot be implemented

// TODO(miguelg) implement the following features
// - Sound names can be implemented by setting soundName in NSUserNotification
//   NSUserNotificationDefaultSoundName gives you the platform default.
// - Actions are only possible in 10.10.

namespace {

// Keys in NSUserNotification.userInfo to map chrome notifications to
// native ones.
NSString* const kNotificationOriginKey = @"notification_origin";
NSString* const kNotificationPersistentIdKey = @"notification_persistent_id";
NSString* const kNotificationDelegateIdKey = @"notification_delegate_id";

// TODO(miguelg) get rid of this key once ProfileID has been ported
// from the void* it is today to the stable identifier provided
// in kNotificationProfilePersistentIdKey.
NSString* const kNotificationProfileIdKey = @"notification_profile_id";
NSString* const kNotificationProfilePersistentIdKey =
    @"notification_profile_persistent_id";
NSString* const kNotificationIncognitoKey = @"notification_incognito";

}  // namespace

// Only use native notifications for web, behind a flag and on 10.8+
// static
NotificationUIManager*
NotificationUIManager::CreateNativeNotificationManager() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeNotifications) &&
      base::mac::IsOSMountainLionOrLater()) {
    return new NotificationUIManagerMac();
  }
  return nullptr;
}

// A Cocoa class that represents the delegate of NSUserNotificationCenter and
// can forward commands to C++.
@interface NotificationCenterDelegate
    : NSObject<NSUserNotificationCenterDelegate> {
 @private
  NotificationUIManagerMac* manager_;  // Weak, owns self.
}
- (id)initWithManager:(NotificationUIManagerMac*)manager;
@end

// /////////////////////////////////////////////////////////////////////////////

NotificationUIManagerMac::NotificationUIManagerMac()
    : delegate_([[NotificationCenterDelegate alloc] initWithManager:this]) {
  [[NSUserNotificationCenter defaultUserNotificationCenter]
      setDelegate:delegate_.get()];
}

NotificationUIManagerMac::~NotificationUIManagerMac() {
  [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:nil];
  CancelAll();
}

void NotificationUIManagerMac::Add(const Notification& notification,
                                   Profile* profile) {
  // The Mac notification UI manager only supports Web Notifications, which
  // have a PersistentNotificationDelegate. The persistent id of the
  // notification is exposed through it's interface.
  PersistentNotificationDelegate* delegate =
      static_cast<PersistentNotificationDelegate*>(notification.delegate());
  DCHECK(delegate);

  base::scoped_nsobject<NSUserNotification> toast(
      [[NSUserNotification alloc] init]);
  [toast setTitle:base::SysUTF16ToNSString(notification.title())];
  [toast setSubtitle:base::SysUTF16ToNSString(notification.message())];

  // TODO(miguelg): try to elide the origin perhaps See NSString
  // stringWithFormat. It seems that the informativeText font is constant.
  NSString* informativeText =
      notification.context_message().empty()
          ? base::SysUTF8ToNSString(notification.origin_url().spec())
          : base::SysUTF16ToNSString(notification.context_message());
  [toast setInformativeText:informativeText];

  // Some functionality is only available in 10.9+ or requires private APIs
  // Icon
  if ([toast respondsToSelector:@selector(_identityImage)] &&
      !notification.icon().IsEmpty()) {
    [toast setValue:notification.icon().ToNSImage() forKey:@"_identityImage"];
    [toast setValue:@NO forKey:@"_identityImageHasBorder"];
  }

  // Buttons
  if ([toast respondsToSelector:@selector(_showsButtons)]) {
    [toast setValue:@YES forKey:@"_showsButtons"];
    [toast setActionButtonTitle:l10n_util::GetNSString(
                                    IDS_NOTIFICATION_BUTTON_SETTINGS)];
    // A default close button label is provided by the platform but we
    // explicitly override it in case the user decides to not
    // use the OS language in Chrome.
    [toast setOtherButtonTitle:l10n_util::GetNSString(
                                   IDS_NOTIFICATION_BUTTON_CLOSE)];
  }

  // Tag
  if ([toast respondsToSelector:@selector(setIdentifier:)] &&
      !notification.tag().empty()) {
    [toast setValue:base::SysUTF8ToNSString(notification.tag())
             forKey:@"identifier"];
  }

  int64_t persistent_notification_id = delegate->persistent_notification_id();
  int64_t profile_id = reinterpret_cast<int64_t>(GetProfileID(profile));

  toast.get().userInfo = @{
    kNotificationOriginKey :
        base::SysUTF8ToNSString(notification.origin_url().spec()),
    kNotificationPersistentIdKey :
        [NSNumber numberWithLongLong:persistent_notification_id],
    kNotificationDelegateIdKey :
        base::SysUTF8ToNSString(notification.delegate_id()),
    kNotificationProfileIdKey : [NSNumber numberWithLongLong:profile_id],
    kNotificationProfilePersistentIdKey :
        base::SysUTF8ToNSString(profile->GetPath().BaseName().value()),
    kNotificationIncognitoKey :
        [NSNumber numberWithBool:profile->IsOffTheRecord()]
  };

  [[NSUserNotificationCenter defaultUserNotificationCenter]
      deliverNotification:toast];
}

bool NotificationUIManagerMac::Update(const Notification& notification,
                                      Profile* profile) {
  NOTREACHED();
  return false;
}

const Notification* NotificationUIManagerMac::FindById(
    const std::string& delegate_id,
    ProfileID profile_id) const {
  NOTREACHED();
  return nil;
}

bool NotificationUIManagerMac::CancelById(const std::string& delegate_id,
                                          ProfileID profile_id) {
  int64_t persistent_notification_id = 0;
  // TODO(peter): Use the |delegate_id| directly when notification ids are being
  // generated by content/ instead of us.
  if (!base::StringToInt64(delegate_id, &persistent_notification_id))
    return false;

  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  for (NSUserNotification* toast in
       [notificationCenter deliveredNotifications]) {
    NSNumber* toast_id =
        [toast.userInfo objectForKey:kNotificationPersistentIdKey];
    if (toast_id.longLongValue == persistent_notification_id) {
      [notificationCenter removeDeliveredNotification:toast];
      return true;
    }
  }

  return false;
}

std::set<std::string>
NotificationUIManagerMac::GetAllIdsByProfileAndSourceOrigin(
    ProfileID profile_id,
    const GURL& source) {
  NOTREACHED();
  return std::set<std::string>();
}

std::set<std::string> NotificationUIManagerMac::GetAllIdsByProfile(
    ProfileID profile_id) {
  // ProfileID in mac is not safe to use across browser restarts
  // Therefore because when chrome quits we cancel all pending notifications.
  // TODO(miguelg) get rid of ProfileID as a void* for native notifications.
  std::set<std::string> delegate_ids;
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  for (NSUserNotification* toast in
       [notificationCenter deliveredNotifications]) {
    NSNumber* toast_profile_id =
        [toast.userInfo objectForKey:kNotificationProfileIdKey];
    if (toast_profile_id.longLongValue ==
        reinterpret_cast<int64_t>(profile_id)) {
      delegate_ids.insert(base::SysNSStringToUTF8(
          [toast.userInfo objectForKey:kNotificationDelegateIdKey]));
    }
  }
  return delegate_ids;
}

bool NotificationUIManagerMac::CancelAllBySourceOrigin(
    const GURL& source_origin) {
  NOTREACHED();
  return false;
}

bool NotificationUIManagerMac::CancelAllByProfile(ProfileID profile_id) {
  NOTREACHED();
  return false;
}

void NotificationUIManagerMac::CancelAll() {
  [[NSUserNotificationCenter defaultUserNotificationCenter]
      removeAllDeliveredNotifications];
}

// /////////////////////////////////////////////////////////////////////////////

@implementation NotificationCenterDelegate

- (id)initWithManager:(NotificationUIManagerMac*)manager {
  if ((self = [super init])) {
    DCHECK(manager);
    manager_ = manager;
  }
  return self;
}

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

  PlatformNotificationServiceImpl::NotificationOperation operation =
      notification.activationType ==
              NSUserNotificationActivationTypeActionButtonClicked
          ? PlatformNotificationServiceImpl::NOTIFICATION_SETTINGS
          : PlatformNotificationServiceImpl::NOTIFICATION_CLICK;

  PlatformNotificationServiceImpl::GetInstance()
      ->ProcessPersistentNotificationOperation(
          operation, base::SysNSStringToUTF8(persistentProfileId),
          [isIncognito boolValue], origin,
          persistentNotificationId.longLongValue,
          -1 /* buttons not yet implemented */);
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end
