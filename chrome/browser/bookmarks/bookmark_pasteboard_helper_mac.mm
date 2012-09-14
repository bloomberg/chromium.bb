// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"

#import <Cocoa/Cocoa.h>

#include <cmath>

#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/mac/nsimage_cache.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

NSString* const kBookmarkDictionaryListPboardType =
    @"BookmarkDictionaryListPboardType";

namespace {

// An unofficial standard pasteboard title type to be provided alongside the
// |NSURLPboardType|.
NSString* const kNSURLTitlePboardType =
    @"public.url-name";

// Pasteboard type used to store profile path to determine which profile
// a set of bookmarks came from.
NSString* const kChromiumProfilePathPboardType =
    @"ChromiumProfilePathPboardType";

// Internal bookmark ID for a bookmark node.  Used only when moving inside
// of one profile.
NSString* const kChromiumBookmarkId =
    @"ChromiumBookmarkId";

// Mac WebKit uses this type, declared in
// WebKit/mac/History/WebURLsWithTitles.h.
NSString* const kCrWebURLsWithTitlesPboardType =
    @"WebURLsWithTitlesPboardType";

// Keys for the type of node in BookmarkDictionaryListPboardType.
NSString* const kWebBookmarkType =
    @"WebBookmarkType";

NSString* const kWebBookmarkTypeList =
    @"WebBookmarkTypeList";

NSString* const kWebBookmarkTypeLeaf =
    @"WebBookmarkTypeLeaf";

void ConvertPlistToElements(NSArray* input,
                            std::vector<BookmarkNodeData::Element>& elements) {
  NSUInteger len = [input count];
  for (NSUInteger i = 0; i < len; ++i) {
    NSDictionary* pboardBookmark = [input objectAtIndex:i];
    scoped_ptr<BookmarkNode> new_node(new BookmarkNode(GURL()));
    int64 node_id =
        [[pboardBookmark objectForKey:kChromiumBookmarkId] longLongValue];
    new_node->set_id(node_id);
    BOOL is_folder = [[pboardBookmark objectForKey:kWebBookmarkType]
        isEqualToString:kWebBookmarkTypeList];
    if (is_folder) {
      new_node->set_type(BookmarkNode::FOLDER);
      NSString* title = [pboardBookmark objectForKey:@"Title"];
      new_node->SetTitle(base::SysNSStringToUTF16(title));
    } else {
      new_node->set_type(BookmarkNode::URL);
      NSDictionary* uriDictionary =
          [pboardBookmark objectForKey:@"URIDictionary"];
      NSString* title = [uriDictionary objectForKey:@"title"];
      NSString* urlString = [pboardBookmark objectForKey:@"URLString"];
      new_node->SetTitle(base::SysNSStringToUTF16(title));
      new_node->set_url(GURL(base::SysNSStringToUTF8(urlString)));
    }
    BookmarkNodeData::Element e = BookmarkNodeData::Element(new_node.get());
    if(is_folder)
      ConvertPlistToElements([pboardBookmark objectForKey:@"Children"],
                             e.children);
    elements.push_back(e);
  }
}

bool ReadBookmarkDictionaryListPboardType(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>& elements) {
  NSArray* bookmarks =
      [pb propertyListForType:kBookmarkDictionaryListPboardType];
  if (!bookmarks)
    return false;
  ConvertPlistToElements(bookmarks, elements);
  return true;
}

bool ReadWebURLsWithTitlesPboardType(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>& elements) {
  NSArray* bookmarkPairs =
      [pb propertyListForType:kCrWebURLsWithTitlesPboardType];
  if (![bookmarkPairs isKindOfClass:[NSArray class]])
    return false;

  NSArray* urlsArr = [bookmarkPairs objectAtIndex:0];
  NSArray* titlesArr = [bookmarkPairs objectAtIndex:1];
  if ([urlsArr count] < 1)
    return false;
  if ([urlsArr count] != [titlesArr count])
    return false;

  NSUInteger len = [titlesArr count];
  for (NSUInteger i = 0; i < len; ++i) {
    string16 title = base::SysNSStringToUTF16([titlesArr objectAtIndex:i]);
    std::string url = base::SysNSStringToUTF8([urlsArr objectAtIndex:i]);
    if (!url.empty()) {
      BookmarkNodeData::Element element;
      element.is_url = true;
      element.url = GURL(url);
      element.title = title;
      elements.push_back(element);
    }
  }
  return true;
}

bool ReadNSURLPboardType(NSPasteboard* pb,
                         std::vector<BookmarkNodeData::Element>& elements) {
  NSURL* url = [NSURL URLFromPasteboard:pb];
  if (url == nil)
    return false;

  std::string urlString = base::SysNSStringToUTF8([url absoluteString]);
  NSString* title = [pb stringForType:kNSURLTitlePboardType];
  if (!title)
    title = [pb stringForType:NSStringPboardType];

  BookmarkNodeData::Element element;
  element.is_url = true;
  element.url = GURL(urlString);
  element.title = base::SysNSStringToUTF16(title);
  elements.push_back(element);
  return true;
}

NSArray* GetPlistForBookmarkList(
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSMutableArray* plist = [NSMutableArray array];
  for (size_t i = 0; i < elements.size(); ++i) {
    BookmarkNodeData::Element element = elements[i];
    if (element.is_url) {
      NSString* title = base::SysUTF16ToNSString(element.title);
      NSString* url = base::SysUTF8ToNSString(element.url.spec());
      int64 elementId = element.id();
      NSNumber* idNum = [NSNumber numberWithLongLong:elementId];
      NSDictionary* uriDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
              title, @"title", nil];
      NSDictionary* object = [NSDictionary dictionaryWithObjectsAndKeys:
          uriDictionary, @"URIDictionary",
          url, @"URLString",
          kWebBookmarkTypeLeaf, kWebBookmarkType,
          idNum, kChromiumBookmarkId,
          nil];
      [plist addObject:object];
    } else {
      NSString* title = base::SysUTF16ToNSString(element.title);
      NSArray* children = GetPlistForBookmarkList(element.children);
      int64 elementId = element.id();
      NSNumber* idNum = [NSNumber numberWithLongLong:elementId];
      NSDictionary* object = [NSDictionary dictionaryWithObjectsAndKeys:
          title, @"Title",
          children, @"Children",
          kWebBookmarkTypeList, kWebBookmarkType,
          idNum, kChromiumBookmarkId,
          nil];
      [plist addObject:object];
    }
  }
  return plist;
}

void WriteBookmarkDictionaryListPboardType(
    NSPasteboard* pb,
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSArray* plist = GetPlistForBookmarkList(elements);
  [pb setPropertyList:plist forType:kBookmarkDictionaryListPboardType];
}

void FillFlattenedArraysForBookmarks(
    const std::vector<BookmarkNodeData::Element>& elements,
    NSMutableArray* titles, NSMutableArray* urls) {
  for (size_t i = 0; i < elements.size(); ++i) {
    BookmarkNodeData::Element element = elements[i];
    if (element.is_url) {
      NSString* title = base::SysUTF16ToNSString(element.title);
      NSString* url = base::SysUTF8ToNSString(element.url.spec());
      [titles addObject:title];
      [urls addObject:url];
    } else {
      FillFlattenedArraysForBookmarks(element.children, titles, urls);
    }
  }
}

void WriteSimplifiedBookmarkTypes(NSPasteboard* pb,
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSMutableArray* titles = [NSMutableArray array];
  NSMutableArray* urls = [NSMutableArray array];
  FillFlattenedArraysForBookmarks(elements, titles, urls);

  // These bookmark types only act on urls, not folders.
  if ([urls count] < 1)
    return;

  // Write WebURLsWithTitlesPboardType.
  [pb setPropertyList:[NSArray arrayWithObjects:urls, titles, nil]
              forType:kCrWebURLsWithTitlesPboardType];

  // Write NSStringPboardType.
  [pb setString:[urls componentsJoinedByString:@"\n"]
        forType:NSStringPboardType];

  // Write NSURLPboardType (with title).
  NSURL* url = [NSURL URLWithString:[urls objectAtIndex:0]];
  [url writeToPasteboard:pb];
  NSString* titleString = [titles objectAtIndex:0];
  [pb setString:titleString forType:kNSURLTitlePboardType];
}

NSPasteboard* PasteboardFromType(
    bookmark_pasteboard_helper_mac::PasteboardType type) {
  NSString* type_string = nil;
  switch (type) {
    case bookmark_pasteboard_helper_mac::kCopyPastePasteboard:
      type_string = NSGeneralPboard;
      break;
    case bookmark_pasteboard_helper_mac::kDragPasteboard:
      type_string = NSDragPboard;
      break;
  }

  return [NSPasteboard pasteboardWithName:type_string];
}

// Make a drag image from the drop data.
NSImage* MakeDragImage(BookmarkModel* model,
                       const std::vector<const BookmarkNode*>& nodes) {
  if (nodes.size() == 1) {
    const BookmarkNode* node = nodes[0];
    const gfx::Image& favicon = model->GetFavicon(node);
    return bookmark_pasteboard_helper_mac::DragImageForBookmark(
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
  scoped_nsobject<NSGradient> mask(
      [[NSGradient alloc] initWithStartingColor:color
                                    endingColor:alpha_color]);
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

namespace bookmark_pasteboard_helper_mac {

void WriteToPasteboard(PasteboardType type,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       FilePath::StringType profile_path) {
  if (elements.empty())
    return;

  NSPasteboard* pb = PasteboardFromType(type);

  NSArray* types = [NSArray arrayWithObjects:kBookmarkDictionaryListPboardType,
                                             kCrWebURLsWithTitlesPboardType,
                                             NSStringPboardType,
                                             NSURLPboardType,
                                             kNSURLTitlePboardType,
                                             kChromiumProfilePathPboardType,
                                             nil];
  [pb declareTypes:types owner:nil];
  [pb setString:base::SysUTF8ToNSString(profile_path)
        forType:kChromiumProfilePathPboardType];
  WriteBookmarkDictionaryListPboardType(pb, elements);
  WriteSimplifiedBookmarkTypes(pb, elements);
}

bool ReadFromPasteboard(PasteboardType type,
                        std::vector<BookmarkNodeData::Element>& elements,
                        FilePath* profile_path) {
  NSPasteboard* pb = PasteboardFromType(type);

  elements.clear();
  NSString* profile = [pb stringForType:kChromiumProfilePathPboardType];
  *profile_path = FilePath(base::SysNSStringToUTF8(profile));
  return ReadBookmarkDictionaryListPboardType(pb, elements) ||
         ReadWebURLsWithTitlesPboardType(pb, elements) ||
         ReadNSURLPboardType(pb, elements);
}

bool PasteboardContainsBookmarks(PasteboardType type) {
  NSPasteboard* pb = PasteboardFromType(type);

  NSArray* availableTypes =
      [NSArray arrayWithObjects:kBookmarkDictionaryListPboardType,
                                kCrWebURLsWithTitlesPboardType,
                                NSURLPboardType,
                                nil];
  return [pb availableTypeFromArray:availableTypes] != nil;
}

NSImage* DragImageForBookmark(NSImage* favicon, const string16& title) {
  // If no favicon, use a default.
  if (!favicon) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
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
  scoped_nsobject<NSAttributedString> rich_title(
      [[NSAttributedString alloc] initWithString:ns_title
                                      attributes:attrs]);

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
  [favicon drawAtPoint:NSMakePoint(0, 0)
              fromRect:NSZeroRect
             operation:NSCompositeSourceOver
              fraction:0.7];
  NSRect target_text_rect = NSMakeRect(text_left, 0,
                                       text_size.width, drag_image_size.height);
  DrawTruncatedTitle(rich_title, target_text_rect);
  [drag_image unlockFocus];

  return drag_image;
}

void StartDrag(Profile* profile,
               const std::vector<const BookmarkNode*>& nodes,
               gfx::NativeView view) {

  std::vector<BookmarkNodeData::Element> elements;
  for (std::vector<const BookmarkNode*>::const_iterator it = nodes.begin();
       it != nodes.end(); ++it) {
    elements.push_back(BookmarkNodeData::Element(*it));
  }

  WriteToPasteboard(kDragPasteboard, elements, profile->GetPath().value());

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
  NSImage* drag_image =
      MakeDragImage(BookmarkModelFactory::GetForProfile(profile), nodes);
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
}

}  // namespace bookmark_pasteboard_helper_mac
