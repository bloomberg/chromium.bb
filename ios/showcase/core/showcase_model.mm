// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/core/showcase_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShowcaseModel

// Insert additional rows in this array. All rows will be sorted upon
// import into Showcase.
// |kShowcaseClassForDisplayKey| and |kShowcaseClassForInstantiationKey| are
// required. |kShowcaseUseCaseKey| is optional.
+ (NSArray<showcase::ModelRow*>*)model {
  return @[
    @{
      showcase::kClassForDisplayKey : @"ContentSuggestionsViewController",
      showcase::kClassForInstantiationKey : @"SCContentSuggestionsCoordinator",
      showcase::kUseCaseKey : @"Content Suggestions UI",
    },
    @{
      showcase::kClassForDisplayKey : @"PaymentRequestEditViewController",
      showcase::kClassForInstantiationKey : @"SCPaymentsEditorCoordinator",
      showcase::kUseCaseKey : @"Generic payment request editor",
    },
    @{
      showcase::kClassForDisplayKey : @"PaymentRequestPickerViewController",
      showcase::kClassForInstantiationKey : @"SCPaymentsPickerCoordinator",
      showcase::kUseCaseKey : @"Payment request picker view",
    },
    @{
      showcase::kClassForDisplayKey : @"PaymentRequestSelectorViewController",
      showcase::kClassForInstantiationKey : @"SCPaymentsSelectorCoordinator",
      showcase::kUseCaseKey : @"Payment request selector view",
    },
    @{
      showcase::kClassForDisplayKey : @"SettingsViewController",
      showcase::kClassForInstantiationKey : @"SCSettingsCoordinator",
      showcase::kUseCaseKey : @"Main settings screen",
    },
    @{
      showcase::kClassForDisplayKey : @"UITableViewCell",
      showcase::kClassForInstantiationKey : @"UIKitTableViewCellViewController",
      showcase::kUseCaseKey : @"UIKit Table Cells",
    },
    @{
      showcase::kClassForDisplayKey : @"SearchWidgetViewController",
      showcase::kClassForInstantiationKey : @"SCSearchWidgetCoordinator",
      showcase::kUseCaseKey : @"Search Widget",
    },
    @{
      showcase::kClassForDisplayKey : @"ContentWidgetViewController",
      showcase::kClassForInstantiationKey : @"SCContentWidgetCoordinator",
      showcase::kUseCaseKey : @"Content Widget",
    },
    @{
      showcase::kClassForDisplayKey : @"TextBadgeView",
      showcase::kClassForInstantiationKey : @"SCTextBadgeViewController",
      showcase::kUseCaseKey : @"Text badge",
    },
    @{
      showcase::kClassForDisplayKey : @"BubbleViewController",
      showcase::kClassForInstantiationKey : @"SCBubbleCoordinator",
      showcase::kUseCaseKey : @"Bubble",
    },
  ];
}

@end
