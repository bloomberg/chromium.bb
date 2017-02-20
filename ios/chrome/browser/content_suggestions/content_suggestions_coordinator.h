// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_

#import "ios/chrome/browser/chrome_coordinator.h"

namespace ios {
class ChromeBrowserState;
}

@protocol UrlLoader;

// Coordinator to manage the Suggestions UI via a
// ContentSuggestionsViewController.
@interface ContentSuggestionsCoordinator : ChromeCoordinator

// BrowserState used to create the ContentSuggestionFactory.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// URLLoader used to open pages.
@property(nonatomic, weak) id<UrlLoader> URLLoader;
// Whether the Suggestions UI is displayed. If this is true, start is a no-op.
@property(nonatomic, readonly) BOOL visible;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
