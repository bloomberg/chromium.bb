// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_OPEN_URL_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_OPEN_URL_COMMAND_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#import "ios/chrome/browser/ui/url_loader.h"

namespace web {
struct Referrer;
}

class GURL;

// A command to open a new tab.
@interface OpenUrlCommand : GenericChromeCommand

// Whether or not this URL command comes from a chrome context (e.g., settings),
// as opposed to a web page context.
@property(nonatomic, readonly) BOOL fromChrome;

// Initializes a command intended to open a URL as a link from a page.
// Designated initializer.
- (id)initWithURL:(const GURL&)url
         referrer:(const web::Referrer&)referrer
       windowName:(NSString*)windowName
      inIncognito:(BOOL)inIncognito
     inBackground:(BOOL)inBackground
         appendTo:(OpenPosition)append;

// Initializes a command intended to open a URL from browser chrome (e.g.,
// settings). This will always open in a new foreground tab in non-incognito
// mode.
- (id)initWithURLFromChrome:(const GURL&)url;

- (const GURL&)url;
- (const web::Referrer&)referrer;
- (NSString*)windowName;
- (BOOL)inIncognito;
- (BOOL)inBackground;
- (OpenPosition)appendTo;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_OPEN_URL_COMMAND_H_
