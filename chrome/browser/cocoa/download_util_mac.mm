// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation for Mac OS X.

#include "chrome/browser/cocoa/download_util_mac.h"

#include "app/gfx/native_widget_types.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/download/download_manager.h"
#include "skia/ext/skia_utils_mac.h"

namespace download_util {

void AddFileToPasteboard(NSPasteboard* pasteboard, const FilePath& path) {
  // Write information about the file being dragged to the pasteboard.
  NSString* file = base::SysUTF8ToNSString(path.value());
  NSArray* fileList = [NSArray arrayWithObject:file];
  [pasteboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType]
                     owner:nil];
  [pasteboard setPropertyList:fileList forType:NSFilenamesPboardType];
}

void NotifySystemOfDownloadComplete(const FilePath& path) {
  NSString* filePath = base::SysUTF8ToNSString(path.value());
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:@"com.apple.DownloadFileFinished"
                    object:filePath];

  NSString* parentPath = [filePath stringByDeletingLastPathComponent];
  FNNotifyByPath(
      reinterpret_cast<const UInt8*>([parentPath fileSystemRepresentation]),
      kFNDirectoryModifiedMessage,
      kNilOptions);
}

void DragDownload(const DownloadItem* download,
                  SkBitmap* icon,
                  gfx::NativeView view) {
  NSPasteboard* pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  AddFileToPasteboard(pasteboard, download->full_path());

  // Convert to an NSImage.
  NSImage* dragImage = gfx::SkBitmapToNSImage(*icon);

  // Synthesize a drag event, since we don't have access to the actual event
  // that initiated a drag (possibly consumed by the DOM UI, for example).
  NSPoint position = [[view window] mouseLocationOutsideOfEventStream];
  NSTimeInterval eventTime = [[NSApp currentEvent] timestamp];
  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                          location:position
                                     modifierFlags:NSLeftMouseDraggedMask
                                         timestamp:eventTime
                                      windowNumber:[[view window] windowNumber]
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];

  // Run the drag operation.
  [[view window] dragImage:dragImage
                        at:position
                    offset:NSZeroSize
                     event:dragEvent
                pasteboard:pasteboard
                    source:view
                 slideBack:YES];
}

}  // namespace download_util
