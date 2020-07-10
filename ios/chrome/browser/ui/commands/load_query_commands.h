// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_LOAD_QUERY_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_LOAD_QUERY_COMMANDS_H_

#import <UIKit/UIKit.h>

// Protocol that describes the commands that loads a result in the omnibox.
@protocol LoadQueryCommands

// Loads |query| in the omnibox. If |immediately| is true, it is loading the
// page associated with it.
- (void)loadQuery:(NSString*)query immediately:(BOOL)immediately;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_LOAD_QUERY_COMMANDS_H_
