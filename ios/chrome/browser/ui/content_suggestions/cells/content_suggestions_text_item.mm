// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_text_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsTextItem

@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize attributes = _attributes;
@synthesize delegate = _delegate;
@synthesize image = _image;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CollectionViewTextCell class];
  }
  return self;
}

- (void)configureCell:(CollectionViewTextCell*)cell {
  [super configureCell:cell];

  cell.textLabel.text = self.text;
  cell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
  cell.textLabel.font = [MDCTypography body2Font];
  cell.textLabel.numberOfLines = 0;
  cell.detailTextLabel.text = self.detailText;
  cell.detailTextLabel.textColor = [[MDCPalette greyPalette] tint500];
  cell.detailTextLabel.font = [MDCTypography body1Font];
  cell.detailTextLabel.numberOfLines = 0;

  cell.isAccessibilityElement = YES;
  if (self.detailText.length == 0) {
    cell.accessibilityLabel = self.text;
  } else {
    cell.accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", self.text, self.detailText];
  }
}

@end
