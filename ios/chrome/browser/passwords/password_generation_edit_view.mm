// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_generation_edit_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "ios/chrome/browser/passwords/password_generation_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constants for the edit view.
const CGFloat kEditViewFontSize = 15;
}  // namespace

@implementation PasswordGenerationEditView {
  UITextField* _textField;
}

- (instancetype)initWithPassword:(NSString*)password {
  // Frame size will updated later.
  const CGRect defaultFrameSize = CGRectMake(0, 0, 100, 30);
  self = [super initWithFrame:defaultFrameSize];
  if (self) {
    _textField = [[UITextField alloc] init];
    [_textField setText:password];
    [_textField setFont:[UIFont systemFontOfSize:kEditViewFontSize]];
    [_textField setBackgroundColor:[UIColor clearColor]];
    [_textField sizeToFit];
    [_textField setUserInteractionEnabled:NO];
    [_textField setFrame:passwords::GetGenerationAccessoryFrame(
                             [self bounds], [_textField frame])];
    [_textField setAutoresizingMask:UIViewAutoresizingFlexibleTopMargin |
                                    UIViewAutoresizingFlexibleBottomMargin |
                                    UIViewAutoresizingFlexibleWidth];
    [self addSubview:_textField];
  }
  return self;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

@end
