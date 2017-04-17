// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ITEM_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ITEM_COLLECTION_VIEW_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

// CollectionViewItem can be created with a default item type but it needs to
// have a valid item type to be inserted in the model. Only the
// CollectionViewController managing the item is allowed to set the item type.
@interface CollectionViewItem (CollectionViewController)

@property(nonatomic, readwrite, assign) NSInteger type;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ITEM_COLLECTION_VIEW_CONTROLLER_H_
