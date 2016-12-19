// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_session_cell_data.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

@implementation SessionCellData

@synthesize type = _type;
@synthesize title = _title;
@synthesize image = _image;

+ (instancetype)incognitoSessionCellData {
  static SessionCellData* incognitoSessionCellData = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    incognitoSessionCellData = [[self alloc]
        initWithSessionCellType:ios_internal::kIncognitoSessionCell];
  });
  return incognitoSessionCellData;
}

+ (instancetype)openTabSessionCellData {
  static SessionCellData* openTabSessionCellData = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    openTabSessionCellData = [[self alloc]
        initWithSessionCellType:ios_internal::kOpenTabSessionCell];
  });
  return openTabSessionCellData;
}

+ (instancetype)otherDevicesSessionCellData {
  static SessionCellData* otherDevicesSessionCellData = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    otherDevicesSessionCellData = [[self alloc]
        initWithSessionCellType:ios_internal::kGenericRemoteSessionCell];
  });
  return otherDevicesSessionCellData;
}

- (instancetype)initWithSessionCellType:(ios_internal::SessionCellType)type {
  self = [super init];
  if (self) {
    _type = type;
    [self loadDefaultsForType];
  }
  return self;
}

#pragma mark - Private

- (void)loadDefaultsForType {
  NSString* imageName = nil;
  int messageId = 0;
  switch (self.type) {
    case ios_internal::kIncognitoSessionCell:
      imageName = @"tabswitcher_incognito";
      messageId = IDS_IOS_TAB_SWITCHER_HEADER_INCOGNITO_TABS;
      break;
    case ios_internal::kOpenTabSessionCell:
      imageName = @"tabswitcher_open_tabs";
      messageId = IDS_IOS_TAB_SWITCHER_HEADER_NON_INCOGNITO_TABS;
      break;
    case ios_internal::kGenericRemoteSessionCell:
      imageName = @"tabswitcher_other_devices";
      messageId = IDS_IOS_TAB_SWITCHER_HEADER_OTHER_DEVICES_TABS;
      break;
    case ios_internal::kPhoneRemoteSessionCell:
      imageName = @"ntp_opentabs_phone";
      messageId = IDS_IOS_TAB_SWITCHER_HEADER_OTHER_DEVICES_TABS;
      break;
    case ios_internal::kTabletRemoteSessionCell:
      imageName = @"ntp_opentabs_tablet";
      messageId = IDS_IOS_TAB_SWITCHER_HEADER_OTHER_DEVICES_TABS;
      break;
    case ios_internal::kLaptopRemoteSessionCell:
      imageName = @"ntp_opentabs_laptop";
      messageId = IDS_IOS_TAB_SWITCHER_HEADER_OTHER_DEVICES_TABS;
      break;
  }
  [self setTitle:l10n_util::GetNSString(messageId)];
  UIImage* image = [UIImage imageNamed:imageName];
  image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [self setImage:image];
}

@end
