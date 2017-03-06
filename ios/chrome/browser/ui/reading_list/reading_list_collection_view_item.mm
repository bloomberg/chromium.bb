// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/favicon_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kSuccessImageString = @"distillation_success";
NSString* kFailureImageString = @"distillation_fail";

// Distillation indicator constants.
const CGFloat kDistillationIndicatorSize = 18;

// Margin for the elements displayed in the cell.
const CGFloat kMargin = 16;
}  // namespace

#pragma mark - ReadingListCell Private interface

@protocol ReadingListCellDelegate<NSObject>

- (void)readingListCellWillPrepareForReload:(ReadingListCell*)cell;

@end

@interface ReadingListCell ()

@property(nonatomic, weak) id<ReadingListCellDelegate> delegate;
// Status of the offline version. Updates the visual indicator when updated.
@property(nonatomic, assign)
    ReadingListEntry::DistillationState distillationState;

@end

#pragma mark - ReadingListCollectionViewItem

@interface ReadingListCollectionViewItem ()<ReadingListCellDelegate> {
  GURL _url;
}
// Attributes provider used to retrieve favicons.
@property(nonatomic, strong)
    FaviconAttributesProvider* faviconAttributesProvider;
// Attributes for favicon. Fetched in init, then retained for future updates.
@property(nonatomic, strong) FaviconAttributes* attributes;
// The cell that is displaying this item, if any. Used to reload favicon when
// the cell is on screen. Backed by WeakNSObject.
@property(nonatomic, weak) ReadingListCell* displayedCell;
@end

@implementation ReadingListCollectionViewItem
@synthesize faviconAttributesProvider = _faviconAttributesProvider;
@synthesize attributes = _attributes;
@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize url = _url;
@synthesize faviconPageURL = _faviconPageURL;
@synthesize displayedCell = _displayedCell;
@synthesize distillationState = _distillationState;

- (instancetype)initWithType:(NSInteger)type
          attributesProvider:(FaviconAttributesProvider*)provider
                         url:(const GURL&)url
           distillationState:(ReadingListEntry::DistillationState)state {
  self = [super initWithType:type];
  if (!self)
    return nil;
  self.cellClass = [ReadingListCell class];
  _faviconAttributesProvider = provider;
  _url = url;
  _distillationState = state;
  return self;
}

- (void)setFaviconPageURL:(GURL)url {
  _faviconPageURL = url;
  // |self| owns |provider|, |provider| owns the block, so a week self reference
  // is necessary.
  __weak ReadingListCollectionViewItem* weakSelf = self;
  [_faviconAttributesProvider
      fetchFaviconAttributesForURL:url
                        completion:^(FaviconAttributes* _Nonnull attributes) {
                          ReadingListCollectionViewItem* strongSelf = weakSelf;
                          if (!strongSelf) {
                            return;
                          }

                          strongSelf.attributes = attributes;

                          [strongSelf.displayedCell.faviconView
                              configureWithAttributes:strongSelf.attributes];
                        }];
}

#pragma mark - property

- (void)setDistillationState:
    (ReadingListEntry::DistillationState)distillationState {
  self.displayedCell.distillationState = distillationState;
  self.displayedCell.accessibilityLabel = [self accessibilityLabel];
  _distillationState = distillationState;
}

#pragma mark - CollectionViewTextItem

- (void)configureCell:(ReadingListCell*)cell {
  [super configureCell:cell];
  if (self.attributes) {
    [cell.faviconView configureWithAttributes:self.attributes];
  }
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  self.displayedCell = cell;
  cell.delegate = self;
  cell.distillationState = _distillationState;
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel = [self accessibilityLabel];
}

#pragma mark - ReadingListCellDelegate

- (void)readingListCellWillPrepareForReload:(ReadingListCell*)cell {
  self.displayedCell = nil;
}

#pragma mark - Private

- (NSString*)accessibilityLabel {
  NSString* accessibilityState = nil;
  if (self.distillationState == ReadingListEntry::PROCESSED) {
    accessibilityState = l10n_util::GetNSString(
        IDS_IOS_READING_LIST_ACCESSIBILITY_STATE_DOWNLOADED);
  } else {
    accessibilityState = l10n_util::GetNSString(
        IDS_IOS_READING_LIST_ACCESSIBILITY_STATE_NOT_DOWNLOADED);
  }

  return l10n_util::GetNSStringF(IDS_IOS_READING_LIST_ENTRY_ACCESSIBILITY_LABEL,
                                 base::SysNSStringToUTF16(self.text),
                                 base::SysNSStringToUTF16(accessibilityState),
                                 base::SysNSStringToUTF16(self.detailText));
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString stringWithFormat:@"Reading List item \"%@\" for url %@",
                                    self.text, self.detailText];
}

- (BOOL)isEqual:(id)other {
  if (other == self)
    return YES;
  if (!other || ![other isKindOfClass:[self class]])
    return NO;
  ReadingListCollectionViewItem* otherItem =
      static_cast<ReadingListCollectionViewItem*>(other);
  return [self.text isEqualToString:otherItem.text] &&
         [self.detailText isEqualToString:otherItem.detailText] &&
         self.distillationState == otherItem.distillationState;
}

@end

#pragma mark - ReadingListCell

@implementation ReadingListCell {
  UIImageView* _downloadIndicator;
}
@synthesize faviconView = _faviconView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize distillationState = _distillationState;
@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    MDFRobotoFontLoader* fontLoader = [MDFRobotoFontLoader sharedInstance];
    CGFloat faviconSize = kFaviconPreferredSize;
    _textLabel = [[UILabel alloc] init];
    _textLabel.font = [fontLoader mediumFontOfSize:16];
    _textLabel.textColor = [[MDCPalette greyPalette] tint900];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.font = [fontLoader mediumFontOfSize:14];
    _detailTextLabel.textColor = [[MDCPalette greyPalette] tint500];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;

    _faviconView = [[FaviconViewNew alloc] init];
    CGFloat fontSize = floorf(faviconSize / 2);
    [_faviconView setFont:[fontLoader regularFontOfSize:fontSize]];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    _downloadIndicator = [[UIImageView alloc] init];
    [_downloadIndicator setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_faviconView addSubview:_downloadIndicator];

    [self.contentView addSubview:_textLabel];
    [self.contentView addSubview:_detailTextLabel];
    [self.contentView addSubview:_faviconView];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"V:|-(margin)-[title][text]-(margin)-|",
          @"H:|-(margin)-[favicon]-(margin)-[title]-(>=margin)-|",
          @"H:[favicon]-(margin)-[text]-(>=margin)-|"
        ],
        @{
          @"title" : _textLabel,
          @"text" : _detailTextLabel,
          @"favicon" : _faviconView
        },
        @{ @"margin" : @(kMargin) });

    [NSLayoutConstraint activateConstraints:@[
      // Favicons are always the same size.
      [_faviconView.widthAnchor constraintEqualToConstant:faviconSize],
      [_faviconView.heightAnchor constraintEqualToConstant:faviconSize],
      [_faviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      // Place the download indicator in the bottom right corner of the favicon.
      [[_downloadIndicator centerXAnchor]
          constraintEqualToAnchor:_faviconView.trailingAnchor],
      [[_downloadIndicator centerYAnchor]
          constraintEqualToAnchor:_faviconView.bottomAnchor],
      [[_downloadIndicator widthAnchor]
          constraintEqualToConstant:kDistillationIndicatorSize],
      [[_downloadIndicator heightAnchor]
          constraintEqualToConstant:kDistillationIndicatorSize],
    ]];

    self.editingSelectorColor = [[MDCPalette cr_bluePalette] tint500];
  }
  return self;
}

- (void)setDistillationState:
    (ReadingListEntry::DistillationState)distillationState {
  if (_distillationState == distillationState)
    return;

  _distillationState = distillationState;
  switch (distillationState) {
    case ReadingListEntry::ERROR:
      [_downloadIndicator setImage:[UIImage imageNamed:kFailureImageString]];
      break;

    case ReadingListEntry::PROCESSED:
      [_downloadIndicator setImage:[UIImage imageNamed:kSuccessImageString]];
      break;

    // Same behavior for all pre-download states.
    case ReadingListEntry::WAITING:
    case ReadingListEntry::WILL_RETRY:
    case ReadingListEntry::PROCESSING:
      [_downloadIndicator setImage:nil];
      break;
  }
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [self.delegate readingListCellWillPrepareForReload:self];
  self.delegate = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.distillationState = ReadingListEntry::WAITING;
  [super prepareForReuse];
}

@end
