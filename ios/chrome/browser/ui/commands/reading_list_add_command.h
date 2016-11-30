// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_READING_LIST_ADD_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_READING_LIST_ADD_COMMAND_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"

class GURL;

@interface ReadingListAddCommand : GenericChromeCommand

@property(nonatomic, readonly) const GURL& URL;
@property(nonatomic, readonly) NSString* title;

- (instancetype)initWithTag:(NSInteger)tag NS_UNAVAILABLE;

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_READING_LIST_ADD_COMMAND_H_
