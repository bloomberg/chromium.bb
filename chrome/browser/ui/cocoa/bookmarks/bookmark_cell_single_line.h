// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_CELL_SINGLE_LINE_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_CELL_SINGLE_LINE_H_
#pragma once

#import <Cocoa/Cocoa.h>

// Provides a category for 10.5 compilation of a selector which is only
// available on 10.6+. This purely enables compilation when the selector
// is present and does not implement the method itself.
// TODO(kushi.p): Remove this when the project hits a 10.6+ only state.
@interface NSCell(multilinebookmarks)
- (void)setUsesSingleLineMode:(BOOL)flag;
@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_CELL_SINGLE_LINE_H_
