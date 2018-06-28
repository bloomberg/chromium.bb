// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_ITEM_CUSTOM_ACTION_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_ITEM_CUSTOM_ACTION_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/reading_list/reading_list_ui_distillation_status.h"

class GURL;
@class ListItem;
@protocol ReadingListListViewItemAccessibilityDelegate;

// Factory object that creates arrays of custom accessibility actions for
// ListItems used by the reading list.
@interface ReadingListListViewItemCustomActionFactory : NSObject

// Delegate for the accessibility actions.
@property(nonatomic, weak) id<ReadingListListViewItemAccessibilityDelegate>
    accessibilityDelegate;

// Creates an array of custom a11y actions for a reading list cell configured
// for |item| with |status|.
- (NSArray<UIAccessibilityCustomAction*>*)
customActionsForItem:(ListItem*)item
             withURL:(const GURL&)URL
  distillationStatus:(ReadingListUIDistillationStatus)status;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_ITEM_CUSTOM_ACTION_FACTORY_H_
