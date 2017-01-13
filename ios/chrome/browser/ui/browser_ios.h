// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_IOS_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_IOS_H_

#import <Foundation/Foundation.h>

namespace ios {
class ChromeBrowserState;
}

@class TabModel;

// Modeled after chrome/browser/ui/browser, just an abstract interface to
// a browser to get the browser state and tabs from but can go into a
// BrowserListIOS.
@protocol BrowserIOS

- (ios::ChromeBrowserState*)browserState;
- (TabModel*)tabModel;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_IOS_H_
