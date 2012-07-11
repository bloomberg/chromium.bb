// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DRAG_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_DRAG_UTIL_H_

#import <Cocoa/Cocoa.h>

class GURL;
class Profile;

namespace drag_util {

// Returns the first file URL from |info|, if there is one. If |info| doesn't
// contain any file URLs, an empty |GURL| is returned.
GURL GetFileURLFromDropData(id<NSDraggingInfo> info);

// Determines whether the given drag and drop operation contains content that
// is supported by the web view. In particular, if the content is a local file
// URL, this checks if it is of a type that can be shown in the tab contents.
BOOL IsUnsupportedDropData(Profile* profile, id<NSDraggingInfo> info);

}  // namespace drag_util

#endif  // CHROME_BROWSER_UI_COCOA_DRAG_UTIL_H_
