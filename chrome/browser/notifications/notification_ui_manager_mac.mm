// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_mac.h"

#include "base/mac/cocoa_protocols.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/balloon_notification_ui_manager.h"

#if defined(ENABLE_MESSAGE_CENTER)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "ui/message_center/message_center_util.h"
#endif

@class NSUserNotificationCenter;

// Since NSUserNotification and NSUserNotificationCenter are new classes in
// 10.8, they cannot simply be declared with an @interface. An @implementation
// is needed to link, but providing one would cause a runtime conflict when
// running on 10.8. Instead, provide the interface defined as a protocol and
// use that instead, because sizeof(id<Protocol>) == sizeof(Class*). In order to
// instantiate, use NSClassFromString and simply assign the alloc/init'd result
// to an instance of the proper protocol. This way the compiler, linker, and
// loader are all happy. And the code isn't full of objc_msgSend.
@protocol CrUserNotification <NSObject>
@property(copy) NSString* title;
@property(copy) NSString* subtitle;
@property(copy) NSString* informativeText;
@property(copy) NSString* actionButtonTitle;
@property(copy) NSDictionary* userInfo;
@property(copy) NSDate* deliveryDate;
@property(copy) NSTimeZone* deliveryTimeZone;
@property(copy) NSDateComponents* deliveryRepeatInterval;
@property(readonly) NSDate* actualDeliveryDate;
@property(readonly, getter=isPresented) BOOL presented;
@property(readonly, getter=isRemote) BOOL remote;
@property(copy) NSString* soundName;
@property BOOL hasActionButton;
@end

@protocol CrUserNotificationCenter
+ (NSUserNotificationCenter*)defaultUserNotificationCenter;
@property(assign) id<NSUserNotificationCenterDelegate> delegate;
@property(copy) NSArray* scheduledNotifications;
- (void)scheduleNotification:(id<CrUserNotification>)notification;
- (void)removeScheduledNotification:(id<CrUserNotification>)notification;
@property(readonly) NSArray* deliveredNotifications;
- (void)deliverNotification:(id<CrUserNotification>)notification;
- (void)removeDeliveredNotification:(id<CrUserNotification>)notification;
- (void)removeAllDeliveredNotifications;
@end

////////////////////////////////////////////////////////////////////////////////

namespace {

// A "fun" way of saying:
//   +[NSUserNotificationCenter defaultUserNotificationCenter].
id<CrUserNotificationCenter> GetNotificationCenter() {
  return [NSClassFromString(@"NSUserNotificationCenter")
      performSelector:@selector(defaultUserNotificationCenter)];
}

// The key in NSUserNotification.userInfo that stores the C++ notification_id.
NSString* const kNotificationIDKey = @"notification_id";

}  // namespace

// A Cocoa class that can be the delegate of NSUserNotificationCenter that
// forwards commands to C++.
@interface NotificationCenterDelegate : NSObject
    <NSUserNotificationCenterDelegate> {
 @private
  NotificationUIManagerMac* manager_;  // Weak, owns self.
}
- (id)initWithManager:(NotificationUIManagerMac*)manager;
@end

////////////////////////////////////////////////////////////////////////////////

NotificationUIManagerMac::ControllerNotification::ControllerNotification(
    Profile* a_profile,
    id<CrUserNotification> a_view,
    Notification* a_model)
    : profile(a_profile),
      view(a_view),
      model(a_model) {
}

NotificationUIManagerMac::ControllerNotification::~ControllerNotification() {
  [view release];
  delete model;
}

////////////////////////////////////////////////////////////////////////////////

// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
#if defined(ENABLE_MESSAGE_CENTER)
  // TODO(rsesek): Remove this function and merge it with the one in
  // notification_ui_manager.cc.
  if (DelegatesToMessageCenter()) {
    return new MessageCenterNotificationManager(
        g_browser_process->message_center());
  }
#endif

  BalloonNotificationUIManager* balloon_manager = NULL;
  if (base::mac::IsOSMountainLionOrLater())
    balloon_manager = new NotificationUIManagerMac(local_state);
  else
    balloon_manager = new BalloonNotificationUIManager(local_state);
  balloon_manager->SetBalloonCollection(BalloonCollection::Create());
  return balloon_manager;
}

NotificationUIManagerMac::NotificationUIManagerMac(PrefService* local_state)
    : BalloonNotificationUIManager(local_state),
      delegate_(ALLOW_THIS_IN_INITIALIZER_LIST(
          [[NotificationCenterDelegate alloc] initWithManager:this])) {
  DCHECK(!GetNotificationCenter().delegate);
  GetNotificationCenter().delegate = delegate_.get();
}

NotificationUIManagerMac::~NotificationUIManagerMac() {
  CancelAll();
}

void NotificationUIManagerMac::Add(const Notification& notification,
                                   Profile* profile) {
  if (notification.is_html()) {
    BalloonNotificationUIManager::Add(notification, profile);
  } else {
    if (!notification.replace_id().empty()) {
      id<CrUserNotification> replacee = FindNotificationWithReplacementId(
          notification.replace_id());
      if (replacee)
        RemoveNotification(replacee);
    }

    // Owned by ControllerNotification.
    id<CrUserNotification> ns_notification =
        [[NSClassFromString(@"NSUserNotification") alloc] init];

    ns_notification.title = base::SysUTF16ToNSString(notification.title());
    ns_notification.subtitle =
        base::SysUTF16ToNSString(notification.display_source());
    ns_notification.informativeText =
        base::SysUTF16ToNSString(notification.body());
    ns_notification.userInfo =
        [NSDictionary dictionaryWithObject:base::SysUTF8ToNSString(
            notification.notification_id())
                                    forKey:kNotificationIDKey];
    ns_notification.hasActionButton = NO;

    notification_map_.insert(std::make_pair(
        notification.notification_id(),
        new ControllerNotification(profile,
                                   ns_notification,
                                   new Notification(notification))));

    [GetNotificationCenter() deliverNotification:ns_notification];
  }
}

bool NotificationUIManagerMac::CancelById(const std::string& notification_id) {
  NotificationMap::iterator it = notification_map_.find(notification_id);
  if (it == notification_map_.end())
    return BalloonNotificationUIManager::CancelById(notification_id);

  return RemoveNotification(it->second->view);
}

bool NotificationUIManagerMac::CancelAllBySourceOrigin(
    const GURL& source_origin) {
  bool success =
      BalloonNotificationUIManager::CancelAllBySourceOrigin(source_origin);

  for (NotificationMap::iterator it = notification_map_.begin();
       it != notification_map_.end();) {
    if (it->second->model->origin_url() == source_origin) {
      // RemoveNotification will erase from the map, invalidating iterator
      // references to the removed element.
      success |= RemoveNotification((it++)->second->view);
    } else {
      ++it;
    }
  }

  return success;
}

bool NotificationUIManagerMac::CancelAllByProfile(Profile* profile) {
  bool success = BalloonNotificationUIManager::CancelAllByProfile(profile);

  for (NotificationMap::iterator it = notification_map_.begin();
       it != notification_map_.end();) {
    if (it->second->profile == profile) {
      // RemoveNotification will erase from the map, invalidating iterator
      // references to the removed element.
      success |= RemoveNotification((it++)->second->view);
    } else {
      ++it;
    }
  }

  return success;
}

void NotificationUIManagerMac::CancelAll() {
  id<CrUserNotificationCenter> center = GetNotificationCenter();

  // Calling RemoveNotification would loop many times over, so just replicate
  // a small bit of its logic here.
  for (NotificationMap::iterator it = notification_map_.begin();
       it != notification_map_.end();
       ++it) {
    it->second->model->Close(false);
    delete it->second;
  }
  notification_map_.clear();

  // Clean up any lingering ones in the system tray.
  [center removeAllDeliveredNotifications];

  BalloonNotificationUIManager::CancelAll();
}

const Notification*
NotificationUIManagerMac::FindNotificationWithCocoaNotification(
    id<CrUserNotification> notification) const {
  std::string notification_id = base::SysNSStringToUTF8(
      [notification.userInfo objectForKey:kNotificationIDKey]);

  NotificationMap::const_iterator it = notification_map_.find(notification_id);
  if (it == notification_map_.end())
    return NULL;

  return it->second->model;
}

bool NotificationUIManagerMac::RemoveNotification(
    id<CrUserNotification> notification) {
  std::string notification_id = base::SysNSStringToUTF8(
      [notification.userInfo objectForKey:kNotificationIDKey]);
  id<CrUserNotificationCenter> center = GetNotificationCenter();

  // First remove all Cocoa notifications from the center that match the
  // notification. Notifications in the system tray do not share pointer
  // equality with the balloons or any other message delievered to the
  // delegate, so this loop must be run through every time to clean up stale
  // notifications.
  NSArray* delivered_notifications = center.deliveredNotifications;
  for (id<CrUserNotification> delivered in delivered_notifications) {
    if ([delivered isEqual:notification]) {
      [center removeDeliveredNotification:delivered];
    }
  }

  // Then clean up the C++ model side.
  NotificationMap::iterator it = notification_map_.find(notification_id);
  if (it == notification_map_.end())
    return false;

  it->second->model->Close(false);
  delete it->second;
  notification_map_.erase(it);

  return true;
}

id<CrUserNotification>
NotificationUIManagerMac::FindNotificationWithReplacementId(
    const string16& replacement_id) const {
  for (NotificationMap::const_iterator it = notification_map_.begin();
       it != notification_map_.end();
       ++it) {
    if (it->second->model->replace_id() == replacement_id)
      return it->second->view;
  }
  return nil;
}

////////////////////////////////////////////////////////////////////////////////

@implementation NotificationCenterDelegate

- (id)initWithManager:(NotificationUIManagerMac*)manager {
  if ((self = [super init])) {
    CHECK(manager);
    manager_ = manager;
  }
  return self;
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
        didDeliverNotification:(id<CrUserNotification>)nsNotification {
  const Notification* notification =
      manager_->FindNotificationWithCocoaNotification(nsNotification);
  if (notification)
    notification->Display();
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(id<CrUserNotification>)nsNotification {
  const Notification* notification =
      manager_->FindNotificationWithCocoaNotification(nsNotification);
  if (notification)
    notification->Click();
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(id<CrUserNotification>)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end
