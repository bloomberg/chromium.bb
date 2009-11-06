// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/web_drop_target.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"
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
      WebDropData data;
      [self populateURLAndTitle:&data fromPasteboard:pboard];
      tabContents_->OpenURL(data.url, GURL(), CURRENT_TAB,
                            PageTransition::AUTO_BOOKMARK);
      return YES;
    }
    return NO;
  }

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

// Populate the URL portion of |data|. There may be more than one, but we only
// handle dropping the first. |data| must not be NULL. Assumes the caller has
// already called |-containsURLData|.
- (void)populateURLAndTitle:(WebDropData*)data
             fromPasteboard:(NSPasteboard*)pboard {
  DCHECK(data);
  DCHECK([pboard containsURLData]);

  // The getURLs:andTitles: will already validate URIs so we don't need to
  // again. However, if the URI is a local file, it won't be prefixed with
  // file://, which is what GURL expects. We can detect that case because the
  // resulting URI will have no valid scheme, and we'll assume it's a local
  // file. The arrays returned are both of NSString's.
  NSArray* urls = nil;
  NSArray* titles = nil;
  [pboard getURLs:&urls andTitles:&titles];
  NSString* urlString = [urls objectAtIndex:0];
  if ([urlString length]) {
    NSURL* url = [NSURL URLWithString:urlString];
    if (![url scheme])
      urlString = [[NSURL fileURLWithPath:urlString] absoluteString];
    // Check again just to make sure to not assign NULL into a std::string,
    // which throws an exception.
    const char* utf8Url = [urlString UTF8String];
    if (utf8Url) {
      data->url = GURL(utf8Url);
      data->url_title = base::SysNSStringToUTF16([titles objectAtIndex:0]);
    }
  }
}

// Given |data|, which should not be nil, fill it in using the contents of the
// given pasteboard.
- (void)populateWebDropData:(WebDropData*)data
             fromPasteboard:(NSPasteboard*)pboard {
  DCHECK(data);
  DCHECK(pboard);
  NSArray* types = [pboard types];

  // Get URL.
  if ([pboard containsURLData])
    [self populateURLAndTitle:data fromPasteboard:pboard];

  // Get plain text.
  if ([types containsObject:NSStringPboardType]) {
    data->plain_text =
        base::SysNSStringToUTF16([pboard stringForType:NSStringPboardType]);
  }

  // Get HTML.
  if ([types containsObject:NSHTMLPboardType]) {
    data->text_html =
        base::SysNSStringToUTF16([pboard stringForType:NSHTMLPboardType]);
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

  // TODO(pinkerton): Get file contents.
}

@end
