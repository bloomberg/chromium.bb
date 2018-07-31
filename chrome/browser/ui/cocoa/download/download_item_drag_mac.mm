// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/drag_download_item.h"

#include "chrome/browser/ui/cocoa/download/download_util_mac.h"
#include "components/download/public/common/download_item.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

void DragDownloadItem(const download::DownloadItem* download,
                      gfx::Image* icon,
                      gfx::NativeView view) {
  // If this drag was initiated from a views::Widget, that widget may have
  // mouse capture. Drags via View::DoDrag() usually release it. The code below
  // bypasses that, so release manually. See https://crbug.com/863377.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(view);
  if (widget)
    widget->ReleaseCapture();

  DCHECK_EQ(download::DownloadItem::COMPLETE, download->GetState());
  NSPasteboard* pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  download_util::AddFileToPasteboard(pasteboard, download->GetTargetFilePath());

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
