// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_LEGACY_READING_LIST_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_LEGACY_READING_LIST_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/reading_list/legacy_reading_list_toolbar.h"

@class ReadingListCollectionViewController;
@protocol ReadingListListViewControllerDelegate;

// Container for the ReadingList Collection View Controller and the toolbar. It
// handles the interactions between the two.
@interface LegacyReadingListViewController : UIViewController

- (instancetype)
initWithCollectionViewController:
    (ReadingListCollectionViewController*)collectionViewController
                         toolbar:(LegacyReadingListToolbar*)toolbar
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<ReadingListListViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_LEGACY_READING_LIST_VIEW_CONTROLLER_H_
