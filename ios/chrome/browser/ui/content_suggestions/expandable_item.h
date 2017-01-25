// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_EXPANDABLE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_EXPANDABLE_ITEM_H_

#import <UIKit/UIKit.h>

// Protocol allowing a CollectionViewItem to be expanded.
@protocol ExpandableItem

// Whether the cells should be in expanded mode.
@property(nonatomic) BOOL expanded;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_EXPANDABLE_ITEM_H_
