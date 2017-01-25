// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_STACK_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_STACK_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Item for a suggestions stack item. The stack items show an item on the top
// and hint that there are other items available. Click on the top item will
// trigger an action, clicking the rest of the cell another.
@interface ContentSuggestionsStackItem : CollectionViewItem

// Initialize a suggestions item with a |title| and a |subtitle|. |type| is the
// type of the item.
- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Corresponding cell for a suggestions stack item.
@interface ContentSuggestionsStackCell : MDCCollectionViewCell

@property(nonatomic, readonly, strong) UIButton* titleButton;
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_STACK_ITEM_H_
