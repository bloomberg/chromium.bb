// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/today_extension/url_table_cell.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/physical_web/physical_web_device.h"
#import "ios/chrome/today_extension/notification_center_url_button.h"
#include "ios/chrome/today_extension/ui_util.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

@implementation URLTableCell {
  base::scoped_nsobject<NotificationCenterURLButton> _button;
}

- (instancetype)initWithTitle:(NSString*)title
                          url:(NSString*)url
                         icon:(NSString*)icon
                    leftInset:(CGFloat)leftInset
              reuseIdentifier:(NSString*)reuseIdentifier
                        block:(URLActionBlock)block {
  self = [super initWithStyle:UITableViewCellStyleDefault
              reuseIdentifier:reuseIdentifier];
  if (self) {
    _button.reset([[NotificationCenterURLButton alloc] initWithTitle:title
                                                                 url:url
                                                                icon:icon
                                                           leftInset:leftInset
                                                               block:block]);
    [[self contentView] addSubview:_button];

    // The button takes the whole frame.
    self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
    _button.get().translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_button.get().leadingAnchor
          constraintEqualToAnchor:[self contentView].leadingAnchor],
      [_button.get().trailingAnchor
          constraintEqualToAnchor:[self contentView].trailingAnchor],
      [_button.get().topAnchor
          constraintEqualToAnchor:[self contentView].topAnchor],
      [_button.get().bottomAnchor
          constraintEqualToAnchor:[self contentView].bottomAnchor],
      [self.leadingAnchor
          constraintEqualToAnchor:[self contentView].leadingAnchor],
      [self.trailingAnchor
          constraintEqualToAnchor:[self contentView].trailingAnchor],
      [self.topAnchor constraintEqualToAnchor:[self contentView].topAnchor],
      [self.bottomAnchor
          constraintEqualToAnchor:[self contentView].bottomAnchor]
    ]];
  }
  return self;
}

- (void)setTitle:(NSString*)title url:(NSString*)url {
  [_button setTitle:title url:url];
}

- (void)setSeparatorVisible:(BOOL)visible {
  [_button setSeparatorVisible:visible];
}

@end
