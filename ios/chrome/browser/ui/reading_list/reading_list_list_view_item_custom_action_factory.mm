// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_item_custom_action_factory.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_item_accessibility_delegate.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - ReadingListCustomAction

// A custom item subclass that holds a reference to its ListItem.
@interface ReadingListCustomAction : UIAccessibilityCustomAction

// The reading list item.
@property(nonatomic, readonly, strong) ListItem* item;
// The URL.
@property(nonatomic, readonly) GURL itemURL;

- (instancetype)initWithName:(NSString*)name
                      target:(id)target
                    selector:(SEL)selector
                        item:(ListItem*)item
                         URL:(const GURL&)URL NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithName:(NSString*)name
                      target:(id)target
                    selector:(SEL)selector NS_UNAVAILABLE;
@end

@implementation ReadingListCustomAction
@synthesize item = _item;
@synthesize itemURL = _itemURL;

- (instancetype)initWithName:(NSString*)name
                      target:(id)target
                    selector:(SEL)selector
                        item:(ListItem*)item
                         URL:(const GURL&)URL {
  if (self = [super initWithName:name target:target selector:selector]) {
    _item = item;
    _itemURL = URL;
  }
  return self;
}

@end

#pragma mark - ReadingListListViewItemCustomActionFactory

@implementation ReadingListListViewItemCustomActionFactory
@synthesize accessibilityDelegate = _accessibilityDelegate;

- (NSArray<UIAccessibilityCustomAction*>*)
customActionsForItem:(ListItem*)item
             withURL:(const GURL&)URL
  distillationStatus:(ReadingListUIDistillationStatus)status {
  ReadingListCustomAction* deleteAction = [[ReadingListCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_READING_LIST_DELETE_BUTTON)
            target:self
          selector:@selector(deleteEntry:)
              item:item
               URL:URL];
  ReadingListCustomAction* toggleReadStatus = nil;
  if ([self.accessibilityDelegate isEntryRead:item]) {
    toggleReadStatus = [[ReadingListCustomAction alloc]
        initWithName:l10n_util::GetNSString(
                         IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON)
              target:self
            selector:@selector(markUnread:)
                item:item
                 URL:URL];
  } else {
    toggleReadStatus = [[ReadingListCustomAction alloc]
        initWithName:l10n_util::GetNSString(
                         IDS_IOS_READING_LIST_MARK_READ_BUTTON)
              target:self
            selector:@selector(markRead:)
                item:item
                 URL:URL];
  }

  ReadingListCustomAction* openInNewTabAction = [[ReadingListCustomAction alloc]
      initWithName:l10n_util::GetNSString(
                       IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
            target:self
          selector:@selector(openInNewTab:)
              item:item
               URL:URL];
  ReadingListCustomAction* openInNewIncognitoTabAction =
      [[ReadingListCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                target:self
              selector:@selector(openInNewIncognitoTab:)
                  item:item
                   URL:URL];
  ReadingListCustomAction* copyURLAction = [[ReadingListCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY)
            target:self
          selector:@selector(copyURL:)
              item:item
               URL:URL];

  NSMutableArray* customActions = [NSMutableArray
      arrayWithObjects:deleteAction, toggleReadStatus, openInNewTabAction,
                       openInNewIncognitoTabAction, copyURLAction, nil];

  if (status == ReadingListUIDistillationStatusSuccess) {
    // Add the possibility to open offline version only if the entry is
    // distilled.
    ReadingListCustomAction* openOfflineAction =
        [[ReadingListCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_READING_LIST_CONTENT_CONTEXT_OFFLINE)
                  target:self
                selector:@selector(openOffline:)
                    item:item
                     URL:URL];

    [customActions addObject:openOfflineAction];
  }

  return customActions;
}

- (BOOL)deleteEntry:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate deleteEntry:action.item];
  return YES;
}

- (BOOL)markRead:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate markEntryRead:action.item];
  return YES;
}

- (BOOL)markUnread:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate markEntryUnread:action.item];
  return YES;
}

- (BOOL)openInNewTab:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate openEntryInNewTab:action.item];
  return YES;
}

- (BOOL)openInNewIncognitoTab:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate openEntryInNewIncognitoTab:action.item];
  return YES;
}

- (BOOL)copyURL:(ReadingListCustomAction*)action {
  StoreURLInPasteboard(action.itemURL);
  return YES;
}

- (BOOL)openOffline:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate openEntryOffline:action.item];
  return YES;
}

@end
