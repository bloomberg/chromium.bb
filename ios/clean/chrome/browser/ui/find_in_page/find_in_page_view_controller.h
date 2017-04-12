// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/commands/find_in_page_search_commands.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_consumer.h"

// FindInPageViewController provides the UI for the find bar for both regular
// and compact size classes.
@interface FindInPageViewController : UIViewController<FindInPageConsumer>

// The |dispatcher| to use when issuing commands.
@property(nonatomic, readwrite, weak)
    id<FindInPageSearchCommands, FindInPageVisibilityCommands>
        dispatcher;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_VIEW_CONTROLLER_H_
