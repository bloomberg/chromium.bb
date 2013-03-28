// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation for Mac OS X.

#include "chrome/browser/ui/cocoa/download/download_util_mac.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

using content::DownloadItem;

namespace download_util {

void AddFileToPasteboard(NSPasteboard* pasteboard, const base::FilePath& path) {
  // Write information about the file being dragged to the pasteboard.
  NSString* file = base::SysUTF8ToNSString(path.value());
  NSArray* fileList = [NSArray arrayWithObject:file];
  [pasteboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType]
                     owner:nil];
  [pasteboard setPropertyList:fileList forType:NSFilenamesPboardType];
}

void DragDownload(const DownloadItem* download,
                  gfx::Image* icon,
                  gfx::NativeView view) {
  DCHECK(download->IsComplete());
  NSPasteboard* pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  AddFileToPasteboard(pasteboard, download->GetTargetFilePath());

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
  [[view window] dragImage:icon->ToNSImage()
                        at:position
                    offset:NSZeroSize
                     event:dragEvent
                pasteboard:pasteboard
                    source:view
                 slideBack:YES];
}

}  // namespace download_util
