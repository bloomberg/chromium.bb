// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_generation_offer_view.h"

#include "base/i18n/rtl.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/passwords/password_generation_utils.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Constants for the offer view.
const int kOfferLabelColor = 0x007AFF;
const CGFloat kOfferLabelFontSize = 15.0;
}  // namespace

@implementation PasswordGenerationOfferView

- (instancetype)initWithDelegate:(id<PasswordGenerationOfferDelegate>)delegate {
  // Frame will be updated later.
  const CGRect defaultFrame = CGRectMake(0, 0, 100, 100);
  self = [super initWithFrame:defaultFrame];
  if (self) {
    base::scoped_nsobject<UILabel> label([[UILabel alloc] init]);
    [label setText:l10n_util::GetNSString(IDS_IOS_GENERATE_PASSWORD_LABEL)];
    UIFont* font = [UIFont systemFontOfSize:kOfferLabelFontSize];
    [label setFont:font];
    [label setTextColor:UIColorFromRGB(kOfferLabelColor)];
    [label setBackgroundColor:[UIColor clearColor]];
    [label sizeToFit];
    [label setFrame:passwords::GetGenerationAccessoryFrame([self bounds],
                                                           [label frame])];
    [label setAutoresizingMask:UIViewAutoresizingFlexibleTopMargin |
                               UIViewAutoresizingFlexibleBottomMargin |
                               (base::i18n::IsRTL()
                                    ? UIViewAutoresizingFlexibleLeftMargin
                                    : UIViewAutoresizingFlexibleRightMargin)];
    [self addSubview:label];

    // Invisible button for tap recognition.
    base::scoped_nsobject<UIButton> button(
        [[UIButton alloc] initWithFrame:[self bounds]]);
    [button setBackgroundColor:[UIColor clearColor]];
    [button setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight];
    [button addTarget:delegate
                  action:@selector(generatePassword)
        forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:button];
  }
  return self;
}

@end
