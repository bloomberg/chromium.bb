// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/ui/test_infobar_view.h"

@implementation TestInfoBarView
@synthesize visibleHeight = _visibleHeight;

- (NSString*)markedLabel {
  return @"";
}

+ (NSString*)stringAsLink:(NSString*)str tag:(NSUInteger)tag {
  return str;
}

- (void)resetDelegate {
}

- (void)addCloseButtonWithTag:(NSInteger)tag
                       target:(id)target
                       action:(SEL)action {
}

- (void)addLeftIcon:(UIImage*)image {
}

- (void)addPlaceholderTransparentIcon:(CGSize const&)imageSize {
}

- (void)addLeftIconWithRoundedCornersAndShadow:(UIImage*)image {
}

- (void)addLabel:(NSString*)label {
}

- (void)addLabel:(NSString*)label target:(id)target action:(SEL)action {
}

- (void)addButton1:(NSString*)title1
              tag1:(NSInteger)tag1
           button2:(NSString*)title2
              tag2:(NSInteger)tag2
            target:(id)target
            action:(SEL)action {
}

- (void)addButton:(NSString*)title
              tag:(NSInteger)tag
           target:(id)target
           action:(SEL)action {
}

- (void)addSwitchWithLabel:(NSString*)label
                      isOn:(BOOL)isOn
                       tag:(NSInteger)tag
                    target:(id)target
                    action:(SEL)action {
}

@end
