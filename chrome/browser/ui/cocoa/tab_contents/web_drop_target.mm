// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/web_drop_target.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/window_open_disposition.h"

using WebKit::WebDragOperationsMask;

@implementation WebDropTarget

// |contents| is the TabContents representing this tab, used to communicate
// drag&drop messages to WebCore and handle navigation on a successful drop
// (if necessary).
- (id)initWithTabContents:(TabContents*)contents {
  if ((self = [super init])) {
    tabContents_ = contents;
  }
  return self;
}

// Call to set whether or not we should allow the drop. Takes effect the
// next time |-draggingUpdated:| is called.
- (void)setCurrentOperation: (NSDragOperation)operation {
  current_operation_ = operation;
}

// Given a point in window coordinates and a view in that window, return a
// flipped point in the coordinate system of |view|.
- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint
                            view:(NSView*)view {
  DCHECK(view);
  NSPoint viewPoint =  [view convertPoint:windowPoint fromView:nil];
  NSRect viewFrame = [view frame];
  viewPoint.y = viewFrame.size.height - viewPoint.y;
  return viewPoint;
}

// Given a point in window coordinates and a view in that window, return a
// flipped point in screen coordinates.
- (NSPoint)flipWindowPointToScreen:(const NSPoint&)windowPoint
                              view:(NSView*)view {
  DCHECK(view);
  NSPoint screenPoint = [[view window] convertBaseToScreen:windowPoint];
  NSScreen* screen = [[view window] screen];
  NSRect screenFrame = [screen frame];
  screenPoint.y = screenFrame.size.height - screenPoint.y;
  return screenPoint;
}

// Return YES if the drop site only allows drops that would navigate.  If this
// is the case, we don't want to pass messages to the renderer because there's
// really no point (i.e., there's nothing that cares about the mouse position or
// entering and exiting).  One example is an interstitial page (e.g., safe
// browsing warning).
- (BOOL)onlyAllowsNavigation {
  return tabContents_->showing_interstitial_page();
}

// Messages to send during the tracking of a drag, ususally upon recieving
// calls from the view system. Communicates the drag messages to WebCore.

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info
                              view:(NSView*)view {
  // Save off the RVH so we can tell if it changes during a drag. If it does,
  // we need to send a new enter message in draggingUpdated:.
  currentRVH_ = tabContents_->render_view_host();

  if ([self onlyAllowsNavigation]) {
    if ([[info draggingPasteboard] containsURLData])
      return NSDragOperationCopy;
    return NSDragOperationNone;
  }

  // If the tab is showing the boomark manager, send BookmarkDrag events
  RenderViewHostDelegate::BookmarkDrag* dragDelegate =
      tabContents_->GetBookmarkDragDelegate();
  BookmarkNodeData dragData;
  if(dragDelegate && dragData.ReadFromDragClipboard())
    dragDelegate->OnDragEnter(dragData);

  // Fill out a WebDropData from pasteboard.
  WebDropData data;
  [self populateWebDropData:&data fromPasteboard:[info draggingPasteboard]];

  // Create the appropriate mouse locations for WebCore. The draggingLocation
  // is in window coordinates. Both need to be flipped.
  NSPoint windowPoint = [info draggingLocation];
  NSPoint viewPoint = [self flipWindowPointToView:windowPoint view:view];
  NSPoint screenPoint = [self flipWindowPointToScreen:windowPoint view:view];
  NSDragOperation mask = [info draggingSourceOperationMask];
  tabContents_->render_view_host()->DragTargetDragEnter(data,
      gfx::Point(viewPoint.x, viewPoint.y),
      gfx::Point(screenPoint.x, screenPoint.y),
      static_cast<WebDragOperationsMask>(mask));

  // We won't know the true operation (whether the drag is allowed) until we
  // hear back from the renderer. For now, be optimistic:
  current_operation_ = NSDragOperationCopy;
  return current_operation_;
}

- (void)draggingExited:(id<NSDraggingInfo>)info {
  DCHECK(currentRVH_);
  if (currentRVH_ != tabContents_->render_view_host())
    return;

  // Nothing to do in the interstitial case.

  tabContents_->render_view_host()->DragTargetDragLeave();
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info
                              view:(NSView*)view {
  DCHECK(currentRVH_);
  if (currentRVH_ != tabContents_->render_view_host())
    [self draggingEntered:info view:view];

  if ([self onlyAllowsNavigation]) {
    if ([[info draggingPasteboard] containsURLData])
      return NSDragOperationCopy;
    return NSDragOperationNone;
  }

  // Create the appropriate mouse locations for WebCore. The draggingLocation
  // is in window coordinates.
  NSPoint windowPoint = [info draggingLocation];
  NSPoint viewPoint = [self flipWindowPointToView:windowPoint view:view];
  NSPoint screenPoint = [self flipWindowPointToScreen:windowPoint view:view];
  NSDragOperation mask = [info draggingSourceOperationMask];
  tabContents_->render_view_host()->DragTargetDragOver(
      gfx::Point(viewPoint.x, viewPoint.y),
      gfx::Point(screenPoint.x, screenPoint.y),
      static_cast<WebDragOperationsMask>(mask));

  // If the tab is showing the boomark manager, send BookmarkDrag events
  RenderViewHostDelegate::BookmarkDrag* dragDelegate =
      tabContents_->GetBookmarkDragDelegate();
  BookmarkNodeData dragData;
  if(dragDelegate && dragData.ReadFromDragClipboard())
    dragDelegate->OnDragOver(dragData);
  return current_operation_;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info
                              view:(NSView*)view {
  if (currentRVH_ != tabContents_->render_view_host())
    [self draggingEntered:info view:view];

  // Check if we only allow navigation and navigate to a url on the pasteboard.
  if ([self onlyAllowsNavigation]) {
    NSPasteboard* pboard = [info draggingPasteboard];
    if ([pboard containsURLData]) {
      GURL url;
      [self populateURL:&url
          andTitle:NULL
          fromPasteboard:pboard
          convertingFilenames:YES];
      tabContents_->OpenURL(url, GURL(), CURRENT_TAB,
                            PageTransition::AUTO_BOOKMARK);
      return YES;
    }
    return NO;
  }

  // If the tab is showing the boomark manager, send BookmarkDrag events
  RenderViewHostDelegate::BookmarkDrag* dragDelegate =
      tabContents_->GetBookmarkDragDelegate();
  BookmarkNodeData dragData;
  if(dragDelegate && dragData.ReadFromDragClipboard())
    dragDelegate->OnDrop(dragData);

  currentRVH_ = NULL;

  // Create the appropriate mouse locations for WebCore. The draggingLocation
  // is in window coordinates. Both need to be flipped.
  NSPoint windowPoint = [info draggingLocation];
  NSPoint viewPoint = [self flipWindowPointToView:windowPoint view:view];
  NSPoint screenPoint = [self flipWindowPointToScreen:windowPoint view:view];
  tabContents_->render_view_host()->DragTargetDrop(
      gfx::Point(viewPoint.x, viewPoint.y),
      gfx::Point(screenPoint.x, screenPoint.y));

  return YES;
}

// Populate the |url| and |title| with URL data in |pboard|. There may be more
// than one, but we only handle dropping the first. |url| must not be |NULL|;
// |title| is an optional parameter. Returns |YES| if URL data was obtained from
// the pasteboard, |NO| otherwise. If |convertFilenames| is |YES|, the function
// will also attempt to convert filenames in |pboard| to file URLs.
- (BOOL)populateURL:(GURL*)url
    andTitle:(string16*)title
    fromPasteboard:(NSPasteboard*)pboard
    convertingFilenames:(BOOL)convertFilenames {
  DCHECK(url);
  DCHECK(title);

  // Bail out early if there's no URL data.
  if (![pboard containsURLData])
    return NO;

  // |-getURLs:andTitles:convertingFilenames:| will already validate URIs so we
  // don't need to again. The arrays returned are both of NSString's.
  NSArray* urls = nil;
  NSArray* titles = nil;
  [pboard getURLs:&urls andTitles:&titles convertingFilenames:convertFilenames];
  DCHECK_EQ([urls count], [titles count]);
  // It's possible that no URLs were actually provided!
  if (![urls count])
    return NO;
  NSString* urlString = [urls objectAtIndex:0];
  if ([urlString length]) {
    // Check again just to make sure to not assign NULL into a std::string,
    // which throws an exception.
    const char* utf8Url = [urlString UTF8String];
    if (utf8Url) {
      *url = GURL(utf8Url);
      // Extra paranoia check.
      if (title && [titles count])
        *title = base::SysNSStringToUTF16([titles objectAtIndex:0]);
    }
  }
  return YES;
}

// Given |data|, which should not be nil, fill it in using the contents of the
// given pasteboard.
- (void)populateWebDropData:(WebDropData*)data
             fromPasteboard:(NSPasteboard*)pboard {
  DCHECK(data);
  DCHECK(pboard);
  NSArray* types = [pboard types];

  // Get URL if possible. To avoid exposing file system paths to web content,
  // filenames in the drag are not converted to file URLs.
  [self populateURL:&data->url
      andTitle:&data->url_title
      fromPasteboard:pboard
      convertingFilenames:NO];

  // Get plain text.
  if ([types containsObject:NSStringPboardType]) {
    data->plain_text =
        base::SysNSStringToUTF16([pboard stringForType:NSStringPboardType]);
  }

  // Get HTML. If there's no HTML, try RTF.
  if ([types containsObject:NSHTMLPboardType]) {
    data->text_html =
        base::SysNSStringToUTF16([pboard stringForType:NSHTMLPboardType]);
  } else if ([types containsObject:NSRTFPboardType]) {
    NSString* html = [pboard htmlFromRtf];
    data->text_html = base::SysNSStringToUTF16(html);
  }

  // Get files.
  if ([types containsObject:NSFilenamesPboardType]) {
    NSArray* files = [pboard propertyListForType:NSFilenamesPboardType];
    if ([files isKindOfClass:[NSArray class]] && [files count]) {
      for (NSUInteger i = 0; i < [files count]; i++) {
        NSString* filename = [files objectAtIndex:i];
        BOOL isDir = NO;
        BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:filename
                                                           isDirectory:&isDir];
        if (exists && !isDir)
          data->filenames.push_back(base::SysNSStringToUTF16(filename));
      }
    }
  }

  // TODO(pinkerton): Get file contents. http://crbug.com/34661
}

@end
