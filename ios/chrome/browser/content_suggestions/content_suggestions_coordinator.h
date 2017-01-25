// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_

#import "ios/chrome/browser/chrome_coordinator.h"

// Coordinator to manage the Suggestions UI via a SuggestionsViewController.
@interface ContentSuggestionsCoordinator : ChromeCoordinator

// Whether the Suggestions UI is displayed. If this is true, start is a no-op.
@property(nonatomic, readonly) BOOL visible;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
