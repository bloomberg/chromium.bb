// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"

#include "base/strings/sys_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_cell.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_item_custom_action_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_item_util.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - ReadingListCollectionViewItem

@implementation ReadingListCollectionViewItem
@synthesize title = _title;
@synthesize entryURL = _entryURL;
@synthesize faviconPageURL = _faviconPageURL;
@synthesize distillationState = _distillationState;
@synthesize distillationSizeText = _distillationSizeText;
@synthesize distillationDateText = _distillationDateText;
@synthesize customActionFactory = _customActionFactory;
@synthesize attributes = _attributes;

#pragma mark - ListItem

- (Class)cellClass {
  return [ReadingListCell class];
}

#pragma mark - CollectionViewTextItem

- (void)configureCell:(ReadingListCell*)cell {
  [super configureCell:cell];
  [cell.faviconView configureWithAttributes:self.attributes];
  cell.titleLabel.text = self.title;
  NSString* subtitle = base::SysUTF16ToNSString(
      url_formatter::FormatUrl(self.entryURL.GetOrigin()));
  cell.subtitleLabel.text = subtitle;
  cell.distillationSizeLabel.text = self.distillationSizeText;
  cell.distillationDateLabel.text = self.distillationDateText;
  cell.showDistillationInfo = self.distillationSizeText.length > 0 &&
                              self.distillationDateText.length > 0;
  cell.distillationState = _distillationState;
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel = GetReadingListCellAccessibilityLabel(
      self.title, subtitle, self.distillationState);
  cell.accessibilityCustomActions =
      [self.customActionFactory customActionsForItem:self
                                             withURL:self.entryURL
                                  distillationStatus:self.distillationState];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString stringWithFormat:@"Reading List item \"%@\" for url %s",
                                    self.title, self.entryURL.spec().c_str()];
}

- (BOOL)isEqual:(id)other {
  return AreReadingListListItemsEqual(self, other);
}

@end
