// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_FORMATTER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_FORMATTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/omnibox_icon.h"

struct AutocompleteMatch;

@interface OmniboxIconFormatter : NSObject <OmniboxIcon>

- (instancetype)initWithMatch:(const AutocompleteMatch&)match
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Whether the default search engine is Google impacts which icon is used in
// some cases
@property(nonatomic, assign) BOOL defaultSearchEngineIsGoogle;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_FORMATTER_H_
