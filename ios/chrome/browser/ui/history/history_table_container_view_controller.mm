// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_table_container_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/table_view/table_container_bottom_toolbar.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HistoryTableContainerViewController

- (instancetype)initWithTable:(ChromeTableViewController*)table {
  self = [super initWithTable:table];
  if (self) {
    NSString* leadingButtonString = l10n_util::GetNSStringWithFixup(
        IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);
    NSString* trailingButtonString =
        l10n_util::GetNSString(IDS_HISTORY_START_EDITING_BUTTON);
    TableContainerBottomToolbar* toolbar = [[TableContainerBottomToolbar alloc]
        initWithLeadingButtonText:leadingButtonString
               trailingButtonText:trailingButtonString];
    [toolbar.leadingButton setTitleColor:[UIColor redColor]
                                forState:UIControlStateNormal];
    self.bottomToolbar = toolbar;
  }
  return self;
}
@end
