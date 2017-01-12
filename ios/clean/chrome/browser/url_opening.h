// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_BROWSER_URL_OPENING_H_
#define IOS_CHROME_BROWSER_URL_OPENING_H_

#import <Foundation/Foundation.h>

// Protocol adopted by objects that can "open" (usually this means loading in a
// web view) URLs.
@protocol URLOpening
// Opens |url|. Any kind of failure happens silently.
- (void)openURL:(NSURL*)URL;
@end

#endif  // IOS_CHROME_BROWSER_URL_OPENING_H_
