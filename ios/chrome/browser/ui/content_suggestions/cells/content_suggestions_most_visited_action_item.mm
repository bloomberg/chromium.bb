// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMostVisitedActionItem

@synthesize action = _action;

- (instancetype)initWithAction:(ContentSuggestionsMostVisitedAction)action {
  self = [super initWithType:0];
  if (self) {
    _action = action;
    switch (_action) {
      case ContentSuggestionsMostVisitedActionBookmark:
        self.title =
            l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS);
        self.attributes = [FaviconAttributes
            attributesWithImage:[UIImage imageNamed:@"ntp_bookmarks_icon"]];
        break;
      case ContentSuggestionsMostVisitedActionReadingList:
        self.title =
            l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST);
        self.attributes = [FaviconAttributes
            attributesWithImage:[UIImage imageNamed:@"ntp_readinglist_icon"]];
        break;
      case ContentSuggestionsMostVisitedActionRecentTabs:
        self.title =
            l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS);
        self.attributes = [FaviconAttributes
            attributesWithImage:[UIImage imageNamed:@"ntp_recent_icon"]];
        break;
      case ContentSuggestionsMostVisitedActionHistory:
        self.title =
            l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_HISTORY);
        self.attributes = [FaviconAttributes
            attributesWithImage:[UIImage imageNamed:@"ntp_history_icon"]];
        break;
    }
  }
  return self;
}

#pragma mark - AccessibilityCustomAction

- (void)configureCell:(ContentSuggestionsMostVisitedCell*)cell {
  [super configureCell:cell];
  cell.accessibilityCustomActions = nil;
}

@end
