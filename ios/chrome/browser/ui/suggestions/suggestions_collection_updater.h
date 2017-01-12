// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COLLECTION_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COLLECTION_UPDATER_H_

#import <UIKit/UIKit.h>

@class CollectionViewController;

// Updater for a CollectionViewController populating it with some items and
// handling the items addition.
@interface SuggestionsCollectionUpdater : NSObject

// |collectionViewController| this Updater will update.
- (instancetype)initWithCollectionViewController:
    (CollectionViewController*)collectionViewController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Adds a text item with a |title| and a |subtitle| in the section numbered
// |section|. If |section| is greater than the current number of section, it
// will add a new section at the end.
- (void)addTextItem:(NSString*)title
           subtitle:(NSString*)subtitle
          toSection:(NSInteger)inputSection;

@end

#endif  // IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COLLECTION_UPDATER_H_
