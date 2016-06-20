// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_builder_mac.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Internal builder constants representing the different notification fields
// They don't need to be exposed outside the builder.

NSString* const kNotificationTitle = @"title";
NSString* const kNotificationSubTitle = @"subtitle";
NSString* const kNotificationInformativeText = @"informativeText";
NSString* const kNotificationImage = @"icon";
NSString* const kNotificationButtonOne = @"buttonOne";
NSString* const kNotificationButtonTwo = @"buttonTwo";
NSString* const kNotificationTag = @"tag";

}  // namespace

namespace notification_builder {

// Exposed constants to include user related data in the notification.
NSString* const kNotificationOrigin = @"notificationOrigin";
NSString* const kNotificationId = @"notificationId";
NSString* const kNotificationProfileId = @"notificationProfileId";
NSString* const kNotificationIncognito = @"notificationIncognito";

}  // namespace notification_builder

@implementation NotificationBuilder {
  base::scoped_nsobject<NSMutableDictionary> notificationData_;
}

- (instancetype)init {
  if ((self = [super init])) {
    notificationData_.reset([[NSMutableDictionary alloc] init]);
  }
  return self;
}

- (instancetype)initWithDictionary:(NSDictionary*)data {
  if ((self = [super init])) {
    notificationData_.reset([data copy]);
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  if (title.length)
    [notificationData_ setObject:title forKey:kNotificationTitle];
}

- (void)setSubTitle:(NSString*)subTitle {
  if (subTitle.length)
    [notificationData_ setObject:subTitle forKey:kNotificationSubTitle];
}

- (void)setContextMessage:(NSString*)contextMessage {
  if (contextMessage.length)
    [notificationData_ setObject:contextMessage
                          forKey:kNotificationInformativeText];
}

- (void)setIcon:(NSImage*)icon {
  if (icon)
    [notificationData_ setObject:icon forKey:kNotificationImage];
}

- (void)setButtons:(NSString*)primaryButton
    secondaryButton:(NSString*)secondaryButton {
  DCHECK(primaryButton.length);
  [notificationData_ setObject:primaryButton forKey:kNotificationButtonOne];
  if (secondaryButton.length) {
    [notificationData_ setObject:secondaryButton forKey:kNotificationButtonTwo];
  }
}

- (void)setTag:(NSString*)tag {
  if (tag.length)
    [notificationData_ setObject:tag forKey:kNotificationTag];
}

- (void)setOrigin:(NSString*)origin {
  if (origin.length)
    [notificationData_ setObject:origin
                          forKey:notification_builder::kNotificationOrigin];
}

- (void)setNotificationId:(NSString*)notificationId {
  DCHECK(notificationId.length);
  [notificationData_ setObject:notificationId
                        forKey:notification_builder::kNotificationId];
}

- (void)setProfileId:(NSString*)profileId {
  DCHECK(profileId.length);
  [notificationData_ setObject:profileId
                        forKey:notification_builder::kNotificationProfileId];
}

- (void)setIncognito:(BOOL)incognito {
  [notificationData_ setObject:[NSNumber numberWithBool:incognito]
                        forKey:notification_builder::kNotificationIncognito];
}

- (NSUserNotification*)buildUserNotification {
  base::scoped_nsobject<NSUserNotification> toast(
      [[NSUserNotification alloc] init]);
  [toast setTitle:[notificationData_ objectForKey:kNotificationTitle]];
  [toast setSubtitle:[notificationData_ objectForKey:kNotificationSubTitle]];
  [toast setInformativeText:[notificationData_
                                objectForKey:kNotificationInformativeText]];

  // Icon
  if ([notificationData_ objectForKey:kNotificationImage]) {
    if ([toast respondsToSelector:@selector(_identityImage)]) {
      NSImage* image = [notificationData_ objectForKey:kNotificationImage];
      [toast setValue:image forKey:@"_identityImage"];
      [toast setValue:@NO forKey:@"_identityImageHasBorder"];
    }
  }

  // Buttons
  if ([toast respondsToSelector:@selector(_showsButtons)]) {
    [toast setValue:@YES forKey:@"_showsButtons"];
    // A default close button label is provided by the platform but we
    // explicitly override it in case the user decides to not
    // use the OS language in Chrome.
    [toast setOtherButtonTitle:l10n_util::GetNSString(
                                   IDS_NOTIFICATION_BUTTON_CLOSE)];

    // Display the Settings button as the action button if there are either no
    // developer-provided action buttons, or the alternate action menu is not
    // available on this Mac version. This avoids needlessly showing the menu.
    // TODO(miguelg): Extensions should not have a settings button.
    if (![notificationData_ objectForKey:kNotificationButtonOne] ||
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
      [toast setValue:@YES forKey:@"_alwaysShowAlternateActionMenu"];

      NSMutableArray* buttons = [NSMutableArray arrayWithCapacity:3];
      [buttons
          addObject:[notificationData_ objectForKey:kNotificationButtonOne]];
      if ([notificationData_ objectForKey:kNotificationButtonTwo]) {
        [buttons
            addObject:[notificationData_ objectForKey:kNotificationButtonTwo]];
      }
      [buttons
          addObject:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_SETTINGS)];
      [toast setValue:buttons forKey:@"_alternateActionButtonTitles"];
    }
  }

  // Tag
  if ([toast respondsToSelector:@selector(setIdentifier:)] &&
      [notificationData_ objectForKey:kNotificationTag]) {
    [toast setValue:[notificationData_ objectForKey:kNotificationTag]
             forKey:@"identifier"];
  }

  NSString* origin =
      [notificationData_ objectForKey:notification_builder::kNotificationOrigin]
          ? [notificationData_
                objectForKey:notification_builder::kNotificationOrigin]
          : @"";
  DCHECK(
      [notificationData_ objectForKey:notification_builder::kNotificationId]);
  NSString* notificationId =
      [notificationData_ objectForKey:notification_builder::kNotificationId];

  DCHECK([notificationData_
      objectForKey:notification_builder::kNotificationProfileId]);
  NSString* profileId = [notificationData_
      objectForKey:notification_builder::kNotificationProfileId];

  DCHECK([notificationData_
      objectForKey:notification_builder::kNotificationIncognito]);
  NSNumber* incognito = [notificationData_
      objectForKey:notification_builder::kNotificationIncognito];

  toast.get().userInfo = @{
    notification_builder::kNotificationOrigin : origin,
    notification_builder::kNotificationId : notificationId,
    notification_builder::kNotificationProfileId : profileId,
    notification_builder::kNotificationIncognito : incognito,
  };

  return toast.autorelease();
}

- (NSDictionary*)buildDictionary {
  return [[notificationData_ copy] autorelease];
}

@end
