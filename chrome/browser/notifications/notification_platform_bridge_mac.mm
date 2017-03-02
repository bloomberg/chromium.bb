// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/native_notification_display_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#import "chrome/browser/ui/cocoa/notifications/notification_delivery.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/common/features.h"
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
// Not easy to implement:
// -notification.requireInteraction

// TODO(miguelg) implement the following features
// - Sound names can be implemented by setting soundName in NSUserNotification
//   NSUserNotificationDefaultSoundName gives you the platform default.

namespace {

// Callback to run once the profile has been loaded in order to perform a
// given |operation| in a notification.
void ProfileLoadedCallback(NotificationCommon::Operation operation,
                           NotificationCommon::Type notification_type,
                           const std::string& origin,
                           const std::string& notification_id,
                           int action_index,
                           Profile* profile) {
  if (!profile) {
    // TODO(miguelg): Add UMA for this condition.
    // Perhaps propagate this through PersistentNotificationStatus.
    LOG(WARNING) << "Profile not loaded correctly";
    return;
  }

  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);

  static_cast<NativeNotificationDisplayService*>(display_service)
      ->ProcessNotificationOperation(operation, notification_type, origin,
                                     notification_id, action_index,
                                     base::NullableString16() /* reply */);
}

// Loads the profile and process the Notification response
void DoProcessNotificationResponse(NotificationCommon::Operation operation,
                                   NotificationCommon::Type type,
                                   const std::string& profile_id,
                                   bool incognito,
                                   const std::string& origin,
                                   const std::string& notification_id,
                                   int32_t button_index) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ProfileManager* profileManager = g_browser_process->profile_manager();
  DCHECK(profileManager);

  profileManager->LoadProfile(
      profile_id, incognito, base::Bind(&ProfileLoadedCallback, operation, type,
                                        origin, notification_id, button_index));
}

}  // namespace

// A Cocoa class that represents the delegate of NSUserNotificationCenter and
// can forward commands to C++.
@interface NotificationCenterDelegate
    : NSObject<NSUserNotificationCenterDelegate> {
}
@end

// Interface to communicate with the Alert XPC service.
@interface AlertDispatcherImpl : NSObject<AlertDispatcher>

// Deliver a notification to the XPC service to be displayed as an alert.
- (void)dispatchNotification:(NSDictionary*)data;

// Close a notification for a given |notificationId| and |profileId|.
- (void)closeNotificationWithId:(NSString*)notificationId
                  withProfileId:(NSString*)profileId;

// Close all notifications.
- (void)closeAllNotifications;

@end

// /////////////////////////////////////////////////////////////////////////////
NotificationPlatformBridgeMac::NotificationPlatformBridgeMac(
    NSUserNotificationCenter* notification_center,
    id<AlertDispatcher> alert_dispatcher)
    : delegate_([NotificationCenterDelegate alloc]),
      notification_center_([notification_center retain]),
      alert_dispatcher_([alert_dispatcher retain]) {
  [notification_center_ setDelegate:delegate_.get()];
}

NotificationPlatformBridgeMac::~NotificationPlatformBridgeMac() {
  [notification_center_ setDelegate:nil];

  // TODO(miguelg) do not remove banners if possible.
  [notification_center_ removeAllDeliveredNotifications];
#if BUILDFLAG(ENABLE_XPC_NOTIFICATIONS)
  [alert_dispatcher_ closeAllNotifications];
#endif  // BUILDFLAG(ENABLE_XPC_NOTIFICATIONS)
}

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
#if BUILDFLAG(ENABLE_XPC_NOTIFICATIONS)
  base::scoped_nsobject<AlertDispatcherImpl> alert_dispatcher(
      [[AlertDispatcherImpl alloc] init]);
  return new NotificationPlatformBridgeMac(
      [NSUserNotificationCenter defaultUserNotificationCenter],
      alert_dispatcher.get());
#else
  return new NotificationPlatformBridgeMac(
      [NSUserNotificationCenter defaultUserNotificationCenter], nil);
#endif  // ENABLE_XPC_NOTIFICATIONS
}

void NotificationPlatformBridgeMac::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool incognito,
    const Notification& notification) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc]
      initWithCloseLabel:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_CLOSE)
            optionsLabel:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_OPTIONS)
           settingsLabel:l10n_util::GetNSString(
                             IDS_NOTIFICATION_BUTTON_SETTINGS)]);

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
  [builder setNotificationType:[NSNumber numberWithInteger:notification_type]];

#if BUILDFLAG(ENABLE_XPC_NOTIFICATIONS)
  // Send persistent notifications to the XPC service so they
  // can be displayed as alerts. Chrome itself can only display
  // banners.
  if (notification.never_timeout()) {
    NSDictionary* dict = [builder buildDictionary];
    [alert_dispatcher_ dispatchNotification:dict];
  } else {
    NSUserNotification* toast = [builder buildUserNotification];
    [notification_center_ deliverNotification:toast];
  }
#else
  NSUserNotification* toast = [builder buildUserNotification];
  [notification_center_ deliverNotification:toast];
#endif  // ENABLE_XPC_NOTIFICATIONS
}

void NotificationPlatformBridgeMac::Close(const std::string& profile_id,
                                          const std::string& notification_id) {
  NSString* candidate_id = base::SysUTF8ToNSString(notification_id);
  NSString* current_profile_id = base::SysUTF8ToNSString(profile_id);

  bool notification_removed = false;
  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toast_id =
        [toast.userInfo objectForKey:notification_constants::kNotificationId];

    NSString* persistent_profile_id = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];

    if ([toast_id isEqualToString:candidate_id] &&
        [persistent_profile_id isEqualToString:current_profile_id]) {
      [notification_center_ removeDeliveredNotification:toast];
      notification_removed = true;
      break;
    }
  }
#if BUILDFLAG(ENABLE_XPC_NOTIFICATIONS)
  // If no banner existed with that ID try to see if there is an alert
  // in the xpc server.
  if (!notification_removed) {
    [alert_dispatcher_ closeNotificationWithId:candidate_id
                                 withProfileId:current_profile_id];
  }
#endif  // ENABLE_XPC_NOTIFICATIONS
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
    if ([toast_profile_id isEqualToString:current_profile_id]) {
      notifications->insert(base::SysNSStringToUTF8([toast.userInfo
          objectForKey:notification_constants::kNotificationId]));
    }
  }
  return true;
}

// static
void NotificationPlatformBridgeMac::ProcessNotificationResponse(
    NSDictionary* response) {
  if (!NotificationPlatformBridgeMac::VerifyNotificationData(response))
    return;

  NSNumber* button_index =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];

  std::string notification_origin = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationOrigin]);
  std::string notification_id = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationId]);
  std::string profile_id = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationProfileId]);
  NSNumber* is_incognito =
      [response objectForKey:notification_constants::kNotificationIncognito];
  NSNumber* notification_type =
      [response objectForKey:notification_constants::kNotificationType];

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(DoProcessNotificationResponse,
                 static_cast<NotificationCommon::Operation>(
                     operation.unsignedIntValue),
                 static_cast<NotificationCommon::Type>(
                     notification_type.unsignedIntValue),
                 profile_id, [is_incognito boolValue], notification_origin,
                 notification_id, button_index.intValue));
}

// static
bool NotificationPlatformBridgeMac::VerifyNotificationData(
    NSDictionary* response) {
  if (![response
          objectForKey:notification_constants::kNotificationButtonIndex] ||
      ![response objectForKey:notification_constants::kNotificationOperation] ||
      ![response objectForKey:notification_constants::kNotificationId] ||
      ![response objectForKey:notification_constants::kNotificationProfileId] ||
      ![response objectForKey:notification_constants::kNotificationIncognito] ||
      ![response objectForKey:notification_constants::kNotificationType]) {
    LOG(ERROR) << "Missing required key";
    return false;
  }

  NSNumber* button_index =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSString* notification_id =
      [response objectForKey:notification_constants::kNotificationId];
  NSString* profile_id =
      [response objectForKey:notification_constants::kNotificationProfileId];
  NSNumber* notification_type =
      [response objectForKey:notification_constants::kNotificationType];

  if (button_index.intValue < -1 ||
      button_index.intValue >=
          static_cast<int>(blink::kWebNotificationMaxActions)) {
    LOG(ERROR) << "Invalid number of buttons supplied "
               << button_index.intValue;
    return false;
  }

  if (operation.unsignedIntValue > NotificationCommon::OPERATION_MAX) {
    LOG(ERROR) << operation.unsignedIntValue
               << " does not correspond to a valid operation.";
    return false;
  }

  if (notification_id.length <= 0) {
    LOG(ERROR) << "Notification Id is empty";
    return false;
  }

  if (profile_id.length <= 0) {
    LOG(ERROR) << "ProfileId not provided";
    return false;
  }

  if (notification_type.unsignedIntValue > NotificationCommon::TYPE_MAX) {
    LOG(ERROR) << notification_type.unsignedIntValue
               << " Does not correspond to a valid operation.";
    return false;
  }

  // Origin is not actually required but if it's there it should be a valid one.
  NSString* origin =
      [response objectForKey:notification_constants::kNotificationOrigin];
  if (origin) {
    std::string notificationOrigin = base::SysNSStringToUTF8(origin);
    GURL url(notificationOrigin);
    if (!url.is_valid())
      return false;
  }

  return true;
}

// /////////////////////////////////////////////////////////////////////////////
@implementation NotificationCenterDelegate
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  NSDictionary* notificationResponse =
      [NotificationResponseBuilder buildDictionary:notification];
  NotificationPlatformBridgeMac::ProcessNotificationResponse(
      notificationResponse);
}

// Overriden from _NSUserNotificationCenterDelegatePrivate.
// Emitted when a user clicks the "Close" button in the notification.
// It not is emitted if the notification is closed from the notification
// center or if the app is not running at the time the Close button is
// pressed so it's essentially just a best effort way to detect
// notifications closed by the user.
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
               didDismissAlert:(NSUserNotification*)notification {
  NSDictionary* notificationResponse =
      [NotificationResponseBuilder buildDictionary:notification];
  NotificationPlatformBridgeMac::ProcessNotificationResponse(
      notificationResponse);
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end

@implementation AlertDispatcherImpl {
  // The connection to the XPC server in charge of delivering alerts.
  base::scoped_nsobject<NSXPCConnection> xpcConnection_;
}

- (instancetype)init {
  if ((self = [super init])) {
    xpcConnection_.reset([[NSXPCConnection alloc]
        initWithServiceName:
            [NSString
                stringWithFormat:notification_constants::kAlertXPCServiceName,
                                 [base::mac::FrameworkBundle()
                                     bundleIdentifier]]]);
    xpcConnection_.get().remoteObjectInterface =
        [NSXPCInterface interfaceWithProtocol:@protocol(NotificationDelivery)];

    xpcConnection_.get().interruptionHandler = ^{
      LOG(WARNING) << "connection interrupted: interruptionHandler: ";
      // TODO(miguelg): perhaps add some UMA here.
      // We will be getting this handler both when the XPC server crashes or
      // when it decides to close the connection.
    };
    xpcConnection_.get().invalidationHandler = ^{
      LOG(WARNING) << "connection invalidationHandler received";
      // This means that the connection should be recreated if it needs
      // to be used again. It should not really happen.
      DCHECK(false) << "XPC Connection invalidated";
    };

    xpcConnection_.get().exportedInterface =
        [NSXPCInterface interfaceWithProtocol:@protocol(NotificationReply)];
    xpcConnection_.get().exportedObject = self;
    [xpcConnection_ resume];
  }

  return self;
}

- (void)dispatchNotification:(NSDictionary*)data {
  [[xpcConnection_ remoteObjectProxy] deliverNotification:data];
}

- (void)closeNotificationWithId:(NSString*)notificationId
                  withProfileId:(NSString*)profileId {
  [[xpcConnection_ remoteObjectProxy] closeNotificationWithId:notificationId
                                                withProfileId:profileId];
}

- (void)closeAllNotifications {
  [[xpcConnection_ remoteObjectProxy] closeAllNotifications];
}

// NotificationReply implementation
- (void)notificationClick:(NSDictionary*)notificationResponseData {
  NotificationPlatformBridgeMac::ProcessNotificationResponse(
      notificationResponseData);
}

@end
