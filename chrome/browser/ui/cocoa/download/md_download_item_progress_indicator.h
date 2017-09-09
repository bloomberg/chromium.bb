// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DOWNLOAD_MD_DOWNLOAD_ITEM_PROGRESS_INDICATOR_H_
#define CHROME_BROWSER_UI_COCOA_DOWNLOAD_MD_DOWNLOAD_ITEM_PROGRESS_INDICATOR_H_

#import <AppKit/AppKit.h>

// MDDownloadItemProgressIndicator is the animated, circular progress bar
// that's part of each item in the download shelf.
@interface MDDownloadItemProgressIndicator : NSView

// A number less than or equal to 1 representing current progress. If less than
// 0, shows an indeterminate progress bar.
@property(nonatomic) CGFloat progress;

// Sets progress to 0 with an animation representing cancelation.
- (void)cancel;

// Sets progress to 1 with an animation representing completion.
- (void)complete;
@end

#endif  // CHROME_BROWSER_UI_COCOA_DOWNLOAD_MD_DOWNLOAD_ITEM_PROGRESS_INDICATOR_H_
