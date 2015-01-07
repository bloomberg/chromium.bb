// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/drag_util.h"

#include <cmath>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_message.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#import "ui/base/dragdrop/cocoa_dnd_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::PluginService;

namespace drag_util {

namespace {

BOOL IsSupportedFileURL(Profile* profile, const GURL& url) {
  base::FilePath full_path;
  net::FileURLToFilePath(url, &full_path);

  std::string mime_type;
  net::GetMimeTypeFromFile(full_path, &mime_type);

  // This logic mirrors |BufferedResourceHandler::ShouldDownload()|.
  // TODO(asvitkine): Refactor this out to a common location instead of
  //                  duplicating code.
  if (net::IsSupportedMimeType(mime_type))
    return YES;

  // Check whether there is a plugin that supports the mime type. (e.g. PDF)
  // TODO(bauerb): This possibly uses stale information, but it's guaranteed not
  // to do disk access.
  bool allow_wildcard = false;
  content::WebPluginInfo plugin;
  return PluginService::GetInstance()->GetPluginInfo(
      -1,                // process ID
      MSG_ROUTING_NONE,  // routing ID
      profile->GetResourceContext(),
      url, GURL(), mime_type, allow_wildcard,
      NULL, &plugin, NULL);
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

GURL GetFileURLFromDropData(id<NSDraggingInfo> info) {
  if ([[info draggingPasteboard] containsURLData]) {
    GURL url;
    ui::PopulateURLAndTitleFromPasteboard(&url,
                                          NULL,
                                          [info draggingPasteboard],
                                          YES);

    if (url.SchemeIs(url::kFileScheme))
      return url;
  }
  return GURL();
}

BOOL IsUnsupportedDropData(Profile* profile, id<NSDraggingInfo> info) {
  GURL url = GetFileURLFromDropData(info);
  if (!url.is_empty()) {
    // If dragging a file, only allow dropping supported file types (that the
    // web view can display).
    return !IsSupportedFileURL(profile, url);
  }
  return NO;
}

NSImage* DragImageForBookmark(NSImage* favicon,
                              const base::string16& title,
                              CGFloat title_width) {
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
  CGFloat max_text_width = title_width - text_left;
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

}  // namespace drag_util
