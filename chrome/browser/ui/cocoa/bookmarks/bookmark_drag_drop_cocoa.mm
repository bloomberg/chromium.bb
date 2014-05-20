// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bookmarks/bookmark_drag_drop_cocoa.h"

#import <Cocoa/Cocoa.h>

#include <cmath>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "grit/ui_resources.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace chrome {

namespace {

// Make a drag image from the drop data.
NSImage* MakeDragImage(BookmarkModel* model,
                       const std::vector<const BookmarkNode*>& nodes) {
  if (nodes.size() == 1) {
    const BookmarkNode* node = nodes[0];
    const gfx::Image& favicon = model->GetFavicon(node);
    return DragImageForBookmark(
        favicon.IsEmpty() ? nil : favicon.ToNSImage(), node->GetTitle());
  } else {
    // TODO(feldstein): Do something better than this. Should have badging
    // and a single drag image.
    // http://crbug.com/37264
    return [NSImage imageNamed:NSImageNameMultipleDocuments];
  }
}

// Draws string |title| within box |frame|, positioning it at the origin.
// Truncates text with fading if it is too long to fit horizontally.
// Based on code from GradientButtonCell but simplified where possible.
void DrawTruncatedTitle(NSAttributedString* title, NSRect frame) {
  NSSize size = [title size];
  if (std::floor(size.width) <= NSWidth(frame)) {
    [title drawAtPoint:frame.origin];
    return;
  }

  // Gradient is about twice our line height long.
  CGFloat gradient_width = std::min(size.height * 2, NSWidth(frame) / 4);
  NSRect solid_part, gradient_part;
  NSDivideRect(frame, &gradient_part, &solid_part, gradient_width, NSMaxXEdge);
  CGContextRef context = static_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  CGContextBeginTransparencyLayerWithRect(context, NSRectToCGRect(frame), 0);
  { // Draw text clipped to frame.
    gfx::ScopedNSGraphicsContextSaveGState scoped_state;
    [NSBezierPath clipRect:frame];
    [title drawAtPoint:frame.origin];
  }

  NSColor* color = [NSColor blackColor];
  NSColor* alpha_color = [color colorWithAlphaComponent:0.0];
  base::scoped_nsobject<NSGradient> mask(
      [[NSGradient alloc] initWithStartingColor:color endingColor:alpha_color]);
  // Draw the gradient mask.
  CGContextSetBlendMode(context, kCGBlendModeDestinationIn);
  [mask drawFromPoint:NSMakePoint(NSMaxX(frame) - gradient_width,
                                  NSMinY(frame))
              toPoint:NSMakePoint(NSMaxX(frame),
                                  NSMinY(frame))
              options:NSGradientDrawsBeforeStartingLocation];
  CGContextEndTransparencyLayer(context);
}

}  // namespace

NSImage* DragImageForBookmark(NSImage* favicon, const base::string16& title) {
  // If no favicon, use a default.
  if (!favicon) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    favicon = rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage();
  }

  // If no title, just use icon.
  if (title.empty())
    return favicon;
  NSString* ns_title = base::SysUTF16ToNSString(title);

  // Set the look of the title.
  NSDictionary* attrs =
      [NSDictionary dictionaryWithObject:[NSFont systemFontOfSize:
                                           [NSFont smallSystemFontSize]]
                                  forKey:NSFontAttributeName];
  base::scoped_nsobject<NSAttributedString> rich_title(
      [[NSAttributedString alloc] initWithString:ns_title attributes:attrs]);

  // Set up sizes and locations for rendering.
  const CGFloat kIconMargin = 2.0;  // Gap between icon and text.
  CGFloat text_left = [favicon size].width + kIconMargin;
  NSSize drag_image_size = [favicon size];
  NSSize text_size = [rich_title size];
  CGFloat max_text_width = bookmarks::kDefaultBookmarkWidth - text_left;
  text_size.width = std::min(text_size.width, max_text_width);
  drag_image_size.width = text_left + text_size.width;

  // Render the drag image.
  NSImage* drag_image =
      [[[NSImage alloc] initWithSize:drag_image_size] autorelease];
  [drag_image lockFocus];
  [favicon drawAtPoint:NSZeroPoint
              fromRect:NSZeroRect
             operation:NSCompositeSourceOver
              fraction:0.7];
  NSRect target_text_rect = NSMakeRect(text_left, 0,
                                       text_size.width, drag_image_size.height);
  DrawTruncatedTitle(rich_title, target_text_rect);
  [drag_image unlockFocus];

  return drag_image;
}

void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view,
                   ui::DragDropTypes::DragEventSource source) {
  DCHECK(!nodes.empty());

  // Allow nested message loop so we get DnD events as we drag this around.
  bool was_nested = base::MessageLoop::current()->IsNested();
  base::MessageLoop::current()->SetNestableTasksAllowed(true);

  BookmarkNodeData drag_data(nodes);
  drag_data.SetOriginatingProfilePath(profile->GetPath());
  drag_data.WriteToClipboard(ui::CLIPBOARD_TYPE_DRAG);

  // Synthesize an event for dragging, since we can't be sure that
  // [NSApp currentEvent] will return a valid dragging event.
  NSWindow* window = [view window];
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval event_time = [[NSApp currentEvent] timestamp];
  NSEvent* drag_event = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                           location:position
                                      modifierFlags:NSLeftMouseDraggedMask
                                          timestamp:event_time
                                       windowNumber:[window windowNumber]
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:1.0];

  // TODO(avi): Do better than this offset.
  NSImage* drag_image = chrome::MakeDragImage(
      BookmarkModelFactory::GetForProfile(profile), nodes);
  NSSize image_size = [drag_image size];
  position.x -= std::floor(image_size.width / 2);
  position.y -= std::floor(image_size.height / 5);
  [window dragImage:drag_image
                 at:position
             offset:NSZeroSize
              event:drag_event
         pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
             source:nil
          slideBack:YES];

  base::MessageLoop::current()->SetNestableTasksAllowed(was_nested);
}

}  // namespace chrome
