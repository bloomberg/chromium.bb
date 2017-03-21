// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_FOOTER_ITEM_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Item for a footer of a Content Suggestions section.
@interface ContentSuggestionsFooterItem : CollectionViewItem

// Initialize a footer with a button taking all the space, with a |title| and a
// |block| run when tapped.
- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                       block:(ProceduralBlock)block NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Corresponding cell for a Content Suggestions' section footer.
@interface ContentSuggestionsFooterCell : MDCCollectionViewCell

@property(nonatomic, readonly, strong) UIButton* button;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_FOOTER_ITEM_H_
