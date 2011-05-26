// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DRAG_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_DRAG_UTIL_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace drag_util {

// Populates the |url| and |title| with URL data in |pboard|. There may be more
// than one, but we only handle dropping the first. |url| must not be |NULL|;
// |title| is an optional parameter. Returns |YES| if URL data was obtained from
// the pasteboard, |NO| otherwise. If |convertFilenames| is |YES|, the function
// will also attempt to convert filenames in |pboard| to file URLs.
BOOL PopulateURLAndTitleFromPasteBoard(GURL* url,
                                       string16* title,
                                       NSPasteboard* pboard,
                                       BOOL convertFilenames);

// Determines whether the given drag and drop operation contains content that
// is supported by the web view. In particular, if the content is a local file
// URL, this checks if it is of a type that can be shown in the tab contents.
BOOL IsUnsupportedDropData(id<NSDraggingInfo> info);

// Sets the current cursor to the "no drop cursor".
void SetNoDropCursor();

}  // namespace drag_util

#endif  // CHROME_BROWSER_UI_COCOA_DRAG_UTIL_H_
