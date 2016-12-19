// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_entries_status_item.h"

#include "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#import "base/mac/objc_property_releaser.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

// Delegate for HistoryEntriesStatusCell.
@protocol HistoryEntriesStatusCellDelegate<NSObject>
// Notifies the delegate that |URL| should be opened.
- (void)historyEntriesStatusCell:(HistoryEntriesStatusCell*)cell
               didRequestOpenURL:(const GURL&)URL;
@end

@interface HistoryEntriesStatusCell () {
  // Property releaser for HistoryEntriesStatusItem.
  base::mac::ObjCPropertyReleaser _propertyReleaser_HistoryEntriesStatusCell;
  // Delegate for the HistoryEntriesStatusCell. Is notified when a link is
  // tapped.
  base::WeakNSProtocol<id<HistoryEntriesStatusCellDelegate>> _delegate;
}
// Redeclare as readwrite.
@property(nonatomic, retain, readwrite)
    LabelLinkController* labelLinkController;
// Delegate for the HistoryEntriesStatusCell. Is notified when a link is
// tapped.
@property(nonatomic, assign) id<HistoryEntriesStatusCellDelegate> delegate;
// Sets links on the cell label.
- (void)setLinksForSyncURL:(const GURL&)syncURL
           browsingDataURL:(const GURL&)browsingDataURL;
@end

@interface HistoryEntriesStatusItem ()<HistoryEntriesStatusCellDelegate> {
  // Delegate for the HistoryEntriesStatusItem. Is notified when a link is
  // tapped.
  base::WeakNSProtocol<id<HistoryEntriesStatusItemDelegate>> _delegate;
}
@end

@implementation HistoryEntriesStatusItem

@synthesize entriesStatus = _entriesStatus;
@synthesize hidden = _hidden;
@synthesize showsOtherBrowsingDataNotice = _showsOtherBrowsingDataNotice;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    _entriesStatus = NO_ENTRIES;
    _hidden = NO;
    _showsOtherBrowsingDataNotice = NO;
  }
  return self;
}

- (Class)cellClass {
  return [HistoryEntriesStatusCell class];
}

- (void)configureCell:(HistoryEntriesStatusCell*)cell {
  [super configureCell:cell];
  [cell setDelegate:self];
  if (self.hidden) {
    cell.textLabel.text = nil;
    return;
  }

  NSString* text = nil;
  GURL syncLearnMoreURL;
  switch (self.entriesStatus) {
    case HistoryEntriesStatus::NO_ENTRIES:
      text = l10n_util::GetNSString(IDS_HISTORY_NO_RESULTS);
      break;
    case HistoryEntriesStatus::SYNCED_ENTRIES:
      text = l10n_util::GetNSString(IDS_IOS_HISTORY_HAS_SYNCED_RESULTS);
      syncLearnMoreURL =
          GURL(l10n_util::GetStringUTF8(IDS_IOS_HISTORY_SYNC_LEARN_MORE_URL));
      break;
    case HistoryEntriesStatus::LOCAL_ENTRIES:
      text = l10n_util::GetNSString(IDS_IOS_HISTORY_NO_SYNCED_RESULTS);
      syncLearnMoreURL =
          GURL(l10n_util::GetStringUTF8(IDS_IOS_HISTORY_SYNC_LEARN_MORE_URL));
      break;
    default:
      NOTREACHED();
      break;
  }

  GURL otherBrowsingDataURL;
  if (self.showsOtherBrowsingDataNotice) {
    NSString* otherBrowsingNotice =
        l10n_util::GetNSString(IDS_IOS_HISTORY_OTHER_FORMS_OF_HISTORY);
    text = [NSString stringWithFormat:@"%@ %@", text, otherBrowsingNotice];
    otherBrowsingDataURL = GURL(kHistoryMyActivityURL);
  }

  cell.textLabel.text = text;
  [cell setLinksForSyncURL:syncLearnMoreURL
           browsingDataURL:otherBrowsingDataURL];
}

- (void)setDelegate:(id<HistoryEntriesStatusItemDelegate>)delegate {
  _delegate.reset(delegate);
}

- (id<HistoryEntriesStatusItemDelegate>)delegate {
  return _delegate;
}

- (void)historyEntriesStatusCell:(HistoryEntriesStatusCell*)cell
               didRequestOpenURL:(const GURL&)URL {
  [self.delegate historyEntriesStatusItem:self didRequestOpenURL:URL];
}

- (BOOL)isEqualToHistoryEntriesStatusItem:(HistoryEntriesStatusItem*)object {
  return self.entriesStatus == object.entriesStatus &&
         self.hidden == object.hidden &&
         self.showsOtherBrowsingDataNotice == self.showsOtherBrowsingDataNotice;
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[HistoryEntriesStatusItem class]]) {
    return NO;
  }
  return [self
      isEqualToHistoryEntriesStatusItem:base::mac::ObjCCastStrict<
                                            HistoryEntriesStatusItem>(object)];
}

@end

@implementation HistoryEntriesStatusCell

@synthesize labelLinkController = _labelLinkController;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_HistoryEntriesStatusCell.Init(
        self, [HistoryEntriesStatusCell class]);
  }
  return self;
}

- (void)setLinksForSyncURL:(const GURL&)syncURL
           browsingDataURL:(const GURL&)browsingDataURL {
  base::WeakNSObject<HistoryEntriesStatusCell> weakSelf(self);
  self.labelLinkController = [[[LabelLinkController alloc]
      initWithLabel:self.textLabel
             action:^(const GURL& URL) {
               [[weakSelf delegate] historyEntriesStatusCell:weakSelf
                                           didRequestOpenURL:URL];
             }] autorelease];
  [self.labelLinkController setLinkColor:[[MDCPalette cr_bluePalette] tint500]];

  // Remove link delimiters from text and get ranges for links. Both links
  // must be parsed before being added to the controller because modifying the
  // label text clears all added links.
  // The sync URL, if present, will always come before the browsing data URL,
  // if present.
  NSRange syncRange;
  if (syncURL.is_valid()) {
    self.textLabel.text = ParseStringWithLink(self.textLabel.text, &syncRange);
    DCHECK(syncRange.location != NSNotFound && syncRange.length);
  }
  NSRange otherBrowsingDataRange;
  if (browsingDataURL.is_valid()) {
    self.textLabel.text =
        ParseStringWithLink(self.textLabel.text, &otherBrowsingDataRange);
    DCHECK(otherBrowsingDataRange.location != NSNotFound &&
           otherBrowsingDataRange.length);
  }
  // Add links to the cell.
  if (syncURL.is_valid()) {
    [self.labelLinkController addLinkWithRange:syncRange url:syncURL];
  }
  if (browsingDataURL.is_valid()) {
    [self.labelLinkController addLinkWithRange:otherBrowsingDataRange
                                           url:browsingDataURL];
  }
}

- (void)setDelegate:(id<HistoryEntriesStatusCellDelegate>)delegate {
  _delegate.reset(delegate);
}

- (id<HistoryEntriesStatusCellDelegate>)delegate {
  return _delegate;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.labelLinkController = nil;
  self.delegate = nil;
}

@end
