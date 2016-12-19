// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/cells/order_summary_total_item.h"

#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

@implementation OrderSummaryTotalItem

#pragma mark CollectionViewItem

- (void)configureCell:(CollectionViewDetailCell*)cell {
  [super configureCell:cell];

  cell.textLabel.font =
      [[MDFRobotoFontLoader sharedInstance] boldFontOfSize:14];
  cell.textLabel.textColor = [[MDCPalette greyPalette] tint600];

  cell.detailTextLabel.font =
      [[MDFRobotoFontLoader sharedInstance] boldFontOfSize:14];
  cell.detailTextLabel.textColor = [[MDCPalette greyPalette] tint900];
}

@end
