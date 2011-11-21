// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation for Mac OS X.

#include "chrome/browser/ui/cocoa/download/download_util_mac.h"

#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/dock_icon.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

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
                  gfx::Image* icon,
                  gfx::NativeView view) {
  NSPasteboard* pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  AddFileToPasteboard(pasteboard, download->GetFullPath());

  // Synthesize a drag event, since we don't have access to the actual event
  // that initiated a drag (possibly consumed by the Web UI, for example).
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
  [[view window] dragImage:*icon
                        at:position
                    offset:NSZeroSize
                     event:dragEvent
                pasteboard:pasteboard
                    source:view
                 slideBack:YES];
}

void UpdateAppIconDownloadProgress(int download_count,
                                   bool progress_known,
                                   float progress) {
  DockIcon* dock_icon = [DockIcon sharedDockIcon];
  [dock_icon setDownloads:download_count];
  [dock_icon setIndeterminate:!progress_known];
  [dock_icon setProgress:progress];
  [dock_icon updateIcon];
}

}  // namespace download_util
