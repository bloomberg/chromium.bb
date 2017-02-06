// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_VIEW_CONTROLLER_BUILDER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_VIEW_CONTROLLER_BUILDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller_container.h"

namespace ios {
class ChromeBrowserState;
}

@class TabModel;
@protocol UrlLoader;

@protocol ReadingListViewControllerDelegate;

// A builder class that constructs ReadingListViewControllers.
@interface ReadingListViewControllerBuilder : NSObject

+ (ReadingListViewControllerContainer*)
readingListViewControllerInBrowserState:(ios::ChromeBrowserState*)browserState
                                 loader:(id<UrlLoader>)loader;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_VIEW_CONTROLLER_BUILDER_H_
