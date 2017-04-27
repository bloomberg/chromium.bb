// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"

#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/favicon/favicon_view.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item_accessibility_delegate.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kSuccessImageString = @"distillation_success";
NSString* kFailureImageString = @"distillation_fail";

// Height of the cell.
const CGFloat kCellHeight = 72;

// Distillation indicator constants.
const CGFloat kDistillationIndicatorSize = 18;

// Margin for the elements displayed in the cell.
const CGFloat kMargin = 16;

// Transparency of the distillation size and date.
const CGFloat kInfoTextTransparency = 0.38;
}  // namespace

#pragma mark - ReadingListCollectionViewItem

@interface ReadingListCollectionViewItem () {
  GURL _url;
}

// Returns the accessibility custom actions associated with this item.
- (NSArray<UIAccessibilityCustomAction*>*)customActions;

@end

@implementation ReadingListCollectionViewItem
@synthesize attributes = _attributes;
@synthesize title = _title;
@synthesize subtitle = _subtitle;
@synthesize url = _url;
@synthesize faviconPageURL = _faviconPageURL;
@synthesize distillationState = _distillationState;
@synthesize distillationDate = _distillationDate;
@synthesize distillationSize = _distillationSize;
@synthesize accessibilityDelegate = _accessibilityDelegate;

- (instancetype)initWithType:(NSInteger)type
                         url:(const GURL&)url
           distillationState:(ReadingListUIDistillationStatus)state {
  self = [super initWithType:type];
  if (!self)
    return nil;
  self.cellClass = [ReadingListCell class];
  _url = url;
  _distillationState = state;
  return self;
}

#pragma mark - CollectionViewTextItem

- (void)configureCell:(ReadingListCell*)cell {
  [super configureCell:cell];
  [cell.faviconView configureWithAttributes:self.attributes];
  cell.titleLabel.text = self.title;
  cell.subtitleLabel.text = self.subtitle;
  cell.distillationState = _distillationState;
  cell.distillationSize = _distillationSize;
  cell.distillationDate = _distillationDate;
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel = [self accessibilityLabel];
  cell.accessibilityCustomActions = [self customActions];
}

#pragma mark - Private

- (NSString*)accessibilityLabel {
  NSString* accessibilityState = nil;
  if (self.distillationState == ReadingListUIDistillationStatusSuccess) {
    accessibilityState = l10n_util::GetNSString(
        IDS_IOS_READING_LIST_ACCESSIBILITY_STATE_DOWNLOADED);
  } else {
    accessibilityState = l10n_util::GetNSString(
        IDS_IOS_READING_LIST_ACCESSIBILITY_STATE_NOT_DOWNLOADED);
  }

  return l10n_util::GetNSStringF(IDS_IOS_READING_LIST_ENTRY_ACCESSIBILITY_LABEL,
                                 base::SysNSStringToUTF16(self.title),
                                 base::SysNSStringToUTF16(accessibilityState),
                                 base::SysNSStringToUTF16(self.subtitle));
}

#pragma mark - AccessibilityCustomAction

- (NSArray<UIAccessibilityCustomAction*>*)customActions {
  UIAccessibilityCustomAction* deleteAction = [
      [UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_READING_LIST_DELETE_BUTTON)
            target:self
          selector:@selector(deleteEntry)];
  UIAccessibilityCustomAction* toogleReadStatus = nil;
  if ([self.accessibilityDelegate isEntryRead:self]) {
    toogleReadStatus = [[UIAccessibilityCustomAction alloc]
        initWithName:l10n_util::GetNSString(
                         IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON)
              target:self
            selector:@selector(markUnread)];
  } else {
    toogleReadStatus = [[UIAccessibilityCustomAction alloc]
        initWithName:l10n_util::GetNSString(
                         IDS_IOS_READING_LIST_MARK_READ_BUTTON)
              target:self
            selector:@selector(markRead)];
  }

  UIAccessibilityCustomAction* openInNewTabAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                target:self
              selector:@selector(openInNewTab)];
  UIAccessibilityCustomAction* openInNewIncognitoTabAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                target:self
              selector:@selector(openInNewIncognitoTab)];
  UIAccessibilityCustomAction* copyURLAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY)
                target:self
              selector:@selector(copyURL)];

  NSMutableArray* customActions = [NSMutableArray
      arrayWithObjects:deleteAction, toogleReadStatus, openInNewTabAction,
                       openInNewIncognitoTabAction, copyURLAction, nil];

  if (self.distillationState == ReadingListUIDistillationStatusSuccess) {
    // Add the possibility to open offline version only if the entry is
    // distilled.
    UIAccessibilityCustomAction* openOfflineAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_READING_LIST_CONTENT_CONTEXT_OFFLINE)
                  target:self
                selector:@selector(openOffline)];

    [customActions addObject:openOfflineAction];
  }

  return customActions;
}

- (BOOL)deleteEntry {
  [self.accessibilityDelegate deleteEntry:self];
  return YES;
}

- (BOOL)markRead {
  [self.accessibilityDelegate markEntryRead:self];
  return YES;
}

- (BOOL)markUnread {
  [self.accessibilityDelegate markEntryUnread:self];
  return YES;
}

- (BOOL)openInNewTab {
  [self.accessibilityDelegate openEntryInNewTab:self];
  return YES;
}

- (BOOL)openInNewIncognitoTab {
  [self.accessibilityDelegate openEntryInNewIncognitoTab:self];
  return YES;
}

- (BOOL)copyURL {
  [self.accessibilityDelegate copyEntryURL:self];
  return YES;
}

- (BOOL)openOffline {
  [self.accessibilityDelegate openEntryOffline:self];
  return YES;
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString stringWithFormat:@"Reading List item \"%@\" for url %@",
                                    self.title, self.subtitle];
}

- (BOOL)isEqual:(id)other {
  if (other == self)
    return YES;
  if (!other || ![other isKindOfClass:[self class]])
    return NO;
  ReadingListCollectionViewItem* otherItem =
      static_cast<ReadingListCollectionViewItem*>(other);
  return [self.title isEqualToString:otherItem.title] &&
         [self.subtitle isEqualToString:otherItem.subtitle] &&
         self.distillationState == otherItem.distillationState &&
         self.distillationSize == otherItem.distillationSize &&
         self.distillationDate == otherItem.distillationDate;
}

@end

#pragma mark - ReadingListCell

@implementation ReadingListCell {
  UIImageView* _downloadIndicator;
  UILayoutGuide* _textGuide;

  UILabel* _distillationSizeLabel;
  UILabel* _distillationDateLabel;

  // View containing |_distillationSizeLabel| and |_distillationDateLabel|.
  UIView* _infoView;

  // Whether |_infoView| is visible.
  BOOL _showInfo;
}
@synthesize faviconView = _faviconView;
@synthesize titleLabel = _titleLabel;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize distillationDate = _distillationDate;
@synthesize distillationSize = _distillationSize;
@synthesize distillationState = _distillationState;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    MDFRobotoFontLoader* fontLoader = [MDFRobotoFontLoader sharedInstance];
    CGFloat faviconSize = kFaviconPreferredSize;
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [fontLoader mediumFontOfSize:16];
    _titleLabel.textColor = [[MDCPalette greyPalette] tint900];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.accessibilityIdentifier = @"Reading List Item title";

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font = [fontLoader mediumFontOfSize:14];
    _subtitleLabel.textColor = [[MDCPalette greyPalette] tint500];
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.accessibilityIdentifier = @"Reading List Item subtitle";

    _distillationDateLabel = [[UILabel alloc] init];
    _distillationDateLabel.font = [fontLoader mediumFontOfSize:12];
    [_distillationDateLabel
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];
    _distillationDateLabel.textColor =
        [UIColor colorWithWhite:0 alpha:kInfoTextTransparency];
    _distillationDateLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _distillationDateLabel.accessibilityIdentifier =
        @"Reading List Item distillation date";

    _distillationSizeLabel = [[UILabel alloc] init];
    _distillationSizeLabel.font = [fontLoader mediumFontOfSize:12];
    [_distillationSizeLabel
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];
    _distillationSizeLabel.textColor =
        [UIColor colorWithWhite:0 alpha:kInfoTextTransparency];
    _distillationSizeLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _distillationSizeLabel.accessibilityIdentifier =
        @"Reading List Item distillation size";

    _faviconView = [[FaviconViewNew alloc] init];
    CGFloat fontSize = floorf(faviconSize / 2);
    [_faviconView setFont:[fontLoader regularFontOfSize:fontSize]];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    _downloadIndicator = [[UIImageView alloc] init];
    [_downloadIndicator setTranslatesAutoresizingMaskIntoConstraints:NO];
    _downloadIndicator.accessibilityIdentifier =
        @"Reading List Item download indicator";
    [_faviconView addSubview:_downloadIndicator];

    [self.contentView addSubview:_faviconView];
    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_subtitleLabel];

    _infoView = [[UIView alloc] initWithFrame:CGRectZero];
    [_infoView addSubview:_distillationDateLabel];
    [_infoView addSubview:_distillationSizeLabel];
    _infoView.translatesAutoresizingMaskIntoConstraints = NO;

    _textGuide = [[UILayoutGuide alloc] init];
    [self.contentView addLayoutGuide:_textGuide];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|[date]-(>=margin)-[size]|",
          @"V:[title][subtitle]",
          @"H:|-(margin)-[favicon]-(margin)-[title]-(>=margin)-|",
          @"H:[favicon]-(margin)-[subtitle]-(>=margin)-|",
          @"V:|[date]|",
          @"V:|[size]|",
        ],
        @{
          @"favicon" : _faviconView,
          @"title" : _titleLabel,
          @"subtitle" : _subtitleLabel,
          @"date" : _distillationDateLabel,
          @"size" : _distillationSizeLabel,
        },
        @{
          @"margin" : @(kMargin),
        });

    // Sets the bottom of the text. Lower the priority so we can add the details
    // later.
    NSLayoutConstraint* bottomTextConstraint = [_textGuide.bottomAnchor
        constraintEqualToAnchor:_subtitleLabel.bottomAnchor];
    bottomTextConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* topTextConstraint =
        [_textGuide.topAnchor constraintEqualToAnchor:_titleLabel.topAnchor];

    [NSLayoutConstraint activateConstraints:@[
      // Height for the cell.
      [self.contentView.heightAnchor constraintEqualToConstant:kCellHeight],

      // Text constraints.
      topTextConstraint,
      bottomTextConstraint,

      // Favicons are always the same size.
      [_faviconView.widthAnchor constraintEqualToConstant:faviconSize],
      [_faviconView.heightAnchor constraintEqualToConstant:faviconSize],

      // Center the content (favicon and text) vertically.
      [_faviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_textGuide.centerYAnchor
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
    (ReadingListUIDistillationStatus)distillationState {
  if (_distillationState == distillationState)
    return;

  _distillationState = distillationState;
  switch (distillationState) {
    case ReadingListUIDistillationStatusFailure:
      [_downloadIndicator setImage:[UIImage imageNamed:kFailureImageString]];
      break;

    case ReadingListUIDistillationStatusSuccess:
      [_downloadIndicator setImage:[UIImage imageNamed:kSuccessImageString]];
      break;

    case ReadingListUIDistillationStatusPending:
      [_downloadIndicator setImage:nil];
      break;
  }
}

- (void)setShowInfo:(BOOL)show {
  if (_showInfo == show) {
    return;
  }
  _showInfo = show;
  if (!show) {
    [_infoView removeFromSuperview];
    return;
  }
  [self.contentView addSubview:_infoView];
  ApplyVisualConstraintsWithMetrics(
      @[
        @"H:|-(margin)-[favicon]-(margin)-[detail]-(margin)-|",
      ],
      @{
        @"favicon" : _faviconView,
        @"detail" : _infoView,
      },
      @{
        @"margin" : @(kMargin),
      });
  [NSLayoutConstraint activateConstraints:@[
    [_infoView.topAnchor constraintEqualToAnchor:_subtitleLabel.bottomAnchor],
    [_infoView.bottomAnchor constraintEqualToAnchor:_textGuide.bottomAnchor],
  ]];
}

- (void)setDistillationSize:(int64_t)distillationSize {
  [_distillationSizeLabel
      setText:[NSByteCountFormatter
                  stringFromByteCount:distillationSize
                           countStyle:NSByteCountFormatterCountStyleFile]];
  _distillationSize = distillationSize;
  BOOL showInfo = _distillationSize != 0 && _distillationDate != 0;
  [self setShowInfo:showInfo];
}

- (void)setDistillationDate:(int64_t)distillationDate {
  int64_t now = (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  int64_t elapsed = now - distillationDate;
  NSString* text;
  if (elapsed < base::Time::kMicrosecondsPerMinute) {
    // This will also catch items added in the future. In that case, show the
    // "just now" string.
    text = l10n_util::GetNSString(IDS_IOS_READING_LIST_JUST_NOW);
  } else {
    text = base::SysUTF16ToNSString(ui::TimeFormat::SimpleWithMonthAndYear(
        ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
        base::TimeDelta::FromMicroseconds(elapsed), true));
  }

  [_distillationDateLabel setText:text];
  _distillationDate = distillationDate;
  BOOL showInfo = _distillationSize != 0 && _distillationDate != 0;
  [self setShowInfo:showInfo];
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  self.titleLabel.text = nil;
  self.subtitleLabel.text = nil;
  self.distillationState = ReadingListUIDistillationStatusPending;
  self.distillationDate = 0;
  self.distillationSize = 0;
  [self setShowInfo:NO];
  [self.faviconView configureWithAttributes:nil];
  self.accessibilityCustomActions = nil;
  [super prepareForReuse];
}

@end
