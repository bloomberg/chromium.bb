// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"

// Collection view item for representing a ReadingListEntry.
@interface ReadingListCollectionViewItem
    : CollectionViewItem<ReadingListListItem>
@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_H_
