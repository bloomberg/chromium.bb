// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_FOOTER_LABEL_H_
#define IOS_CHROME_TODAY_EXTENSION_FOOTER_LABEL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <base/ios/block_types.h>

// IDs of the different footer labels that can be displayed.
enum FooterLabel {
  NO_FOOTER_LABEL,
  PW_BT_OFF_FOOTER_LABEL,
  PW_IS_ON_FOOTER_LABEL,
  PW_IS_OFF_FOOTER_LABEL,
  PW_OPTIN_DIALOG,
  PW_SCANNING_FOOTER_LABEL,
  FOOTER_LABEL_COUNT,
};

typedef void (^LearnMoreBlock)(void);
typedef void (^EnableDisableBlock)(void);

// Protocol that all footer labels must implement to make layout of today
// extension possible.
@protocol FooterLabel<NSObject>

// Computes the size needed to display the footer label with the given |width|.
- (CGFloat)heightForWidth:(CGFloat)width;

// The view containing the footer label.
- (UIView*)view;

@end

// Generic class for footer label. This label can contain one link (delimited
// by BEGIN_LINK and END_LINK in label string) and a button (if buttonString and
// buttonBlock are both non nil).
@interface SimpleFooterLabel : NSObject<FooterLabel>

- (instancetype)initWithLabelString:(NSString*)labelString
                       buttonString:(NSString*)buttonString
                          linkBlock:(ProceduralBlock)linkBlock
                        buttonBlock:(ProceduralBlock)buttonBlock;

@end

// The footer label displaying information that physical web is on.
// Contains a learn more link and a disable button.
@interface PWIsOnFooterLabel : SimpleFooterLabel

- (instancetype)initWithLearnMoreBlock:(LearnMoreBlock)learnMore
                          turnOffBlock:(EnableDisableBlock)turnOffBlock;

@end

// The footer label displaying information that physical web is off.
// Contains a learn more link and an enable button.
@interface PWIsOffFooterLabel : SimpleFooterLabel

- (instancetype)initWithLearnMoreBlock:(LearnMoreBlock)learnMore
                           turnOnBlock:(EnableDisableBlock)turnOnBlock;

@end

// The footer label displaying information that physical web is scanning.
// Contains a learn more link and a disable button.
@interface PWScanningFooterLabel : SimpleFooterLabel

- (instancetype)initWithLearnMoreBlock:(LearnMoreBlock)learnMore
                          turnOffBlock:(EnableDisableBlock)turnOffBlock;

@end

// The footer label displaying information that Bluetooth is off.
@interface PWBTOffFooterLabel : SimpleFooterLabel

- (instancetype)initWithLearnMoreBlock:(LearnMoreBlock)learnMore;

@end

#endif  // IOS_CHROME_TODAY_EXTENSION_FOOTER_LABEL_H_
