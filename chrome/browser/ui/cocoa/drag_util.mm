// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/drag_util.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "content/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "webkit/glue/webcursor.h"

namespace drag_util {

BOOL PopulateURLAndTitleFromPasteBoard(GURL* url,
                                       string16* title,
                                       NSPasteboard* pboard,
                                       BOOL convertFilenames) {
  CHECK(url);

  // Bail out early if there's no URL data.
  if (![pboard containsURLData])
    return NO;

  // -getURLs:andTitles:convertingFilenames: will already validate URIs so we
  // don't need to again. The arrays returned are both of NSStrings.
  NSArray* urlArray = nil;
  NSArray* titleArray = nil;
  [pboard getURLs:&urlArray andTitles:&titleArray
      convertingFilenames:convertFilenames];
  DCHECK_EQ([urlArray count], [titleArray count]);
  // It's possible that no URLs were actually provided!
  if (![urlArray count])
    return NO;
  NSString* urlString = [urlArray objectAtIndex:0];
  if ([urlString length]) {
    // Check again just to make sure to not assign NULL into a std::string,
    // which throws an exception.
    const char* utf8Url = [urlString UTF8String];
    if (utf8Url) {
      *url = GURL(utf8Url);
      // Extra paranoia check.
      if (title && [titleArray count])
        *title = base::SysNSStringToUTF16([titleArray objectAtIndex:0]);
    }
  }
  return YES;
}

BOOL IsUnsupportedDropData(id<NSDraggingInfo> info) {
  if ([[info draggingPasteboard] containsURLData]) {
    GURL url;
    PopulateURLAndTitleFromPasteBoard(&url,
                                      NULL,
                                      [info draggingPasteboard],
                                      YES);

    // If dragging a file, only allow dropping supported file types (that the
    // web view can display).
    if (url.SchemeIs(chrome::kFileScheme)) {
      FilePath full_path;
      net::FileURLToFilePath(url, &full_path);

      std::string mime_type;
      net::GetMimeTypeFromFile(full_path, &mime_type);

      if (!net::IsSupportedMimeType(mime_type))
        return YES;
    }
  }
  return NO;
}

void SetNoDropCursor() {
  WebKit::WebCursorInfo cursorInfo(WebKit::WebCursorInfo::TypeNoDrop);
  [WebCursor(cursorInfo).GetCursor() set];
}

}  // namespace drag_util
