// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_PHYSICAL_WEB_OPTIN_FOOTER_H_
#define IOS_CHROME_TODAY_EXTENSION_PHYSICAL_WEB_OPTIN_FOOTER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/common/physical_web/physical_web_types.h"
#import "ios/chrome/today_extension/footer_label.h"

// Opt-in dialog shown the first time a physical web beacon is discovered.
// Contains a learn more link, a button to opt in and a button to opt out.
@interface PhysicalWebOptInFooter : NSObject<FooterLabel>

- (id)initWithLeftInset:(CGFloat)leftInset
         learnMoreBlock:(LearnMoreBlock)learnMoreBlock
            optinAction:(EnableDisableBlock)optinAction
          dismissAction:(EnableDisableBlock)dismissAction;

@end

#endif  // IOS_CHROME_TODAY_EXTENSION_PHYSICAL_WEB_OPTIN_FOOTER_H_
