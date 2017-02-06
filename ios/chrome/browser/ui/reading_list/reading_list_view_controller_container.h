// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_VIEW_CONTROLLER_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_VIEW_CONTROLLER_CONTAINER_H_

#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller.h"

namespace favicon {
class LargeIconService;
}

class ReadingListDownloadService;
class ReadingListModel;
@class ReadingListViewController;
@protocol UrlLoader;

// Container for the Reading List View Controller and the toolbar. It handles
// the interactions between the two. It also acts as a ReadingList delegate,
// opening entries and displaying context menu.
@interface ReadingListViewControllerContainer
    : UIViewController<ReadingListViewControllerDelegate>

- (instancetype)initWithModel:(ReadingListModel*)model
                        loader:(id<UrlLoader>)loader
              largeIconService:(favicon::LargeIconService*)largeIconService
    readingListDownloadService:
        (ReadingListDownloadService*)readingListDownloadService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_VIEW_CONTROLLER_CONTAINER_H_
