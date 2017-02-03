// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_H_

#import "components/reading_list/ios/reading_list_entry.h"
#import "ios/chrome/browser/favicon/favicon_attributes_provider.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

class GURL;

// Collection view item for representing a ReadingListEntry.
@interface ReadingListCollectionViewItem : CollectionViewItem

// The main text to display.
@property(nonatomic, copy) NSString* text;
// The secondary text to display.
@property(nonatomic, copy) NSString* detailText;
// The URL of the Reading List entry.
@property(nonatomic, readonly) const GURL& url;
// The URL of the page presenting the favicon to display.
@property(nonatomic, assign) GURL faviconPageURL;
// Status of the offline version.
@property(nonatomic, assign)
    ReadingListEntry::DistillationState distillationState;

// Designated initializer. The |provider| will be used for loading favicon
// attributes. The |delegate| is used to inform about changes to this item. The
// |url| is displayed as a subtitle. The |state| is used to display visual
// indicator of the distillation status.
- (instancetype)initWithType:(NSInteger)type
          attributesProvider:(FaviconAttributesProvider*)provider
                         url:(const GURL&)url
           distillationState:(ReadingListEntry::DistillationState)state
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

@class FaviconViewNew;
// Cell for ReadingListCollectionViewItem.
@interface ReadingListCell : MDCCollectionViewCell

// Title label.
@property(nonatomic, readonly, strong) UILabel* textLabel;
// Detail label.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;
// View for displaying the favicon for the reading list entry.
@property(nonatomic, readonly, strong) FaviconViewNew* faviconView;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_H_
